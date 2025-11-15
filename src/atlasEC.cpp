#include "atlasEC.h"
#include "atlasRTD.h"
#include "esp_random.h"

// --- AtlasEC ---

AtlasEC::AtlasEC()
    : AtlasTemperatureCompensatedSensor(&AtlasRTD::shared())
{
    // isDumpResponseBufferEnabled = true;
    // isLogSentCommandsEnabled = true;
}

double AtlasEC::convertReadingResponseToDouble(char *response) {
    if (!(response && *response)) return DBL_MIN;
    if (!strcasecmp(response, "no output")) return DBL_MIN;

    double ec = DBL_MIN;
    int i;
    const char *s;

    for (i = 0; (s = strsep(&response, ",")); ++i) {
        if (i == readingResponseFieldIndexForConductivity) {
            if (sscanf(s, "%lf", &ec) != 1) {
                ec = DBL_MIN;
                loge("sscanf failed to convert '%s' to double", s);
                dump(s, strlen(s));
            }
            break;
        }
    }

    return ec;
}

#if ENABLE_ATLAS_SIMULATOR
err_t AtlasEC::getSimulatedReading(char *buffer, size_t bufferSize) {
    static uint32_t ec_mS_cm = 1000;

#if 0
    static int count = 0;

    if (++count == 11) {    // approximately every 10 seconds
        count = 0;
        ec_mS_cm = 1000 + esp_random() % 1001;
    }
#endif

    snprintf(buffer, bufferSize, "\x01" "%lu", ec_mS_cm);

    return 0;
}
#endif

void AtlasEC::handleGetParametersResponse(ParametersResponse &response) {
    if (response.err) return;

    // const char *name = getName();
    // _logi("%s conductivity is %senabled", name, response.isConductivityEnabled ? "" : "not ");
    // _logi("%s salinity is %senabled", name, response.isSalinityEnabled ? "" : "not ");
    // _logi("%s specific gravity is %senabled", name, response.isSpecificGravityEnabled ? "" : "not ");
    // _logi("%s total dissolved solids is %senabled", name, response.isTotalDissolvedSolidsEnabled ? "" : "not ");

    readingResponseFieldIndexForConductivity = response.readingResponseFieldIndexForConductivity;
    readingResponseFieldIndexForSalinity = response.readingResponseFieldIndexForSalinity;
    readingResponseFieldIndexForSpecificGravity = response.readingResponseFieldIndexForSpecificGravity;
    readingResponseFieldIndexForTotalDissolvedSolids = response.readingResponseFieldIndexForTotalDissolvedSolids;
}

err_t AtlasEC::init(const char *name, uint8_t i2cSlaveAddress, DispatchTask *task) {
    err_t err = AtlasSensor::init(name, i2cSlaveAddress, task, true);

    if (!err) err = sendSetConductivity(isConductivityEnabled);
    if (!err) err = sendSetSalinity(isSalinityEnabled);
    if (!err) err = sendSetSpecificGravity(isSpecificGravityEnabled);
    if (!err) err = sendSetTotalDissolvedSolids(isTotalDissolvedSolidsEnabled);
    if (!err) err = sendGetParameters();
    if (!err) err = sendGetProbeKValue();
    if (!err) err = sendGetTotalDissolvedSolidsConversionFactor();
    if (!err) err = enqueueSendGetReading();

    return err;
}

bool AtlasEC::isSetTemperatureCompensationAndTakeReadingSupported() {
    return firmwareMajorVersion > 2 || (firmwareMajorVersion == 2 && firmwareMinorVersion >= 13);
}

err_t AtlasEC::sendCalibrateDry(bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "cal,dry", context, callback, nullptr, 600);
}

err_t AtlasEC::sendCalibrateHigh(double calibrationSolutionEC, double solutionTemperatureC, bool synchronous, void *context, CommandCallback callback) {
    return sendCalibration(",high", calibrationSolutionEC, solutionTemperatureC, synchronous, context, callback);
}

err_t AtlasEC::sendCalibrateLow(double calibrationSolutionEC, double solutionTemperatureC, bool synchronous, void *context, CommandCallback callback) {
    return sendCalibration(",low", calibrationSolutionEC, solutionTemperatureC, synchronous, context, callback);
}

err_t AtlasEC::sendCalibrateSingle(double calibrationSolutionEC, double solutionTemperatureC, bool synchronous, void *context, CommandCallback callback) {
    return sendCalibration("", calibrationSolutionEC, solutionTemperatureC, synchronous, context, callback);
}

err_t AtlasEC::sendCalibration(const char *prefix, double calibrationSolutionEC, double solutionTemperatureC, bool synchronous, void *context, CommandCallback callback) {
    err_t err = 0;

    if ((solutionTemperatureC < 20 || solutionTemperatureC > 25)) {
        loge("calibration solution temperature out of range for EC calibration");
        err = ERANGE;
    }
    if (!err) {
        // // we're assuming KCl 2% condutivity change per °C per http://www.riccachemical.com/Ricca/media/Documents/Technical%20Reference%20Documents/Ricca_Potassium_Conductivity_Chart.pdf
        // double deltaDegreesC = solutionTemperatureC - 25.0;
        // double adjustmentPerDegreeC = calibrationSolutionEC * 0.02;
        // double calibrationAdjustment = adjustmentPerDegreeC * deltaDegreesC;
        // int adjustedCalibrationSolutionEC = int(round(calibrationSolutionEC + calibrationAdjustment));

        // logi("calibrationSolutionEC %d adjusted to %d to compensate for temperature %0.02f°C (∆%0.02f)", calibrationSolutionEC, adjustedCalibrationSolutionEC, solutionTemperatureC, deltaDegreesC);

        // calibrationSolutionEC = adjustedCalibrationSolutionEC;

        err = makeAndSendCommand<Response>(synchronous, "cal%s,%0.1f", context, callback, nullptr, 600, Priority::defaultPriority, CompletionBehavior::dequeue, prefix, calibrationSolutionEC);
    }

    return err;
}

err_t AtlasEC::sendGetParameters(bool synchronous, void *context, ParametersResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            AtlasEC *ec = static_cast<AtlasEC *>(sensor);
            ParametersResponse &parametersResponse = static_cast<ParametersResponse &>(response);

            if (!parametersResponse.err) {
                ec->handleGetParametersResponse(parametersResponse);
            }
        };
    }
    err = makeCommand<ParametersResponse>(command, "o,?", context, callback, "?o,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            AtlasEC *ec = static_cast<AtlasEC *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "?O%s%s%s%s", ec->isConductivityEnabled ? ",EC" : "", ec->isTotalDissolvedSolidsEnabled ? ",TDS" : "", ec->isSalinityEnabled ? ",S" : "", ec->isSpecificGravityEnabled ? ",SG" : "");
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasEC::sendGetProbeKValue(bool synchronous, void *context, DoubleResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            DoubleResponse &doubleResponse = static_cast<DoubleResponse &>(response);

            if (!doubleResponse.err) {
                _logd("%s probe K value is %0.3f", sensor->getName(), doubleResponse.value);
            }
        };
    }
    err = makeCommand<DoubleResponse>(command, "k,?", context, callback, "?k,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            AtlasEC *ecSensor = static_cast<AtlasEC *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "?K,%0.3f", ecSensor->probeKValue);
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasEC::sendGetTotalDissolvedSolidsConversionFactor(bool synchronous, void *context, DoubleResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            DoubleResponse &doubleResponse = static_cast<DoubleResponse &>(response);

            if (!doubleResponse.err) {
                _logd("%s TDS conversion factor is %0.3f", sensor->getName(), doubleResponse.value);
            }
        };
    }
    err = makeCommand<DoubleResponse>(command, "tds,?", context, callback, "?tds,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            AtlasEC *ecSensor = static_cast<AtlasEC *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "?TDS,%0.3f", ecSensor->totalDissolvedSolidsConversionFactor);
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasEC::sendSetConductivity(bool isEnabled, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "o,ec,%u", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, int(isEnabled));
}

err_t AtlasEC::sendSetProbeKValue(double K, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "k,%0.3f", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, K);
}

err_t AtlasEC::sendSetSalinity(bool isEnabled, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "o,s,%u", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, int(isEnabled));
}

err_t AtlasEC::sendSetSpecificGravity(bool isEnabled, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "o,sg,%u", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, int(isEnabled));
}

err_t AtlasEC::sendSetTotalDissolvedSolids(bool isEnabled, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "o,tds,%u", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, int(isEnabled));
}

err_t AtlasEC::sendSetTotalDissolvedSolidsConversionFactor(double conversionFactor, bool synchronous, void *context, CommandCallback callback) {
    err_t err = makeAndSendCommand<Response>(synchronous, "tds,%0.3f", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, conversionFactor);
#if ENABLE_ATLAS_SIMULATOR
    if (!err) totalDissolvedSolidsConversionFactor = conversionFactor;
#endif
    return err;
}

// --- AtlasEC::ParametersResponse ---

err_t AtlasEC::ParametersResponse::parse(char *response) {
    err_t err;
    int i;
    const char *value;

    err = Response::parse(response);
    for (i = 0; !err && (value = field()); ++i) {
        if (!strcasecmp(value, "ec")) {
            isConductivityEnabled = true;
            readingResponseFieldIndexForConductivity = i;
        } else if (!strcasecmp(value, "s")) {
            isSalinityEnabled = true;
            readingResponseFieldIndexForSalinity = i;
        } else if (!strcasecmp(value, "sg")) {
            isSpecificGravityEnabled = true;
            readingResponseFieldIndexForSpecificGravity = i;
        } else if (!strcasecmp(value, "tds")) {
            isTotalDissolvedSolidsEnabled = true;
            readingResponseFieldIndexForTotalDissolvedSolids = i;
        } else {
            err = EBADMSG;
        }
    }

    return err;
}

AtlasEC &AtlasEC::shared() {
    static AtlasEC *singleton = new AtlasEC();

    return *singleton;
}
