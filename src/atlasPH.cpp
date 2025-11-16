//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "atlasPH.h"
#include "atlasRTD.h"

AtlasPH::AtlasPH() :
#if ATLAS_RTD_ENABLE_PH_SENSOR
    AtlasTemperatureCompensatedSensor(&AtlasRTD::sharedPHSensor())
#else
    AtlasTemperatureCompensatedSensor(&AtlasRTD::shared())
#endif
{
    // isDumpResponseBufferEnabled = true;
    // isLogSentCommandsEnabled = true;
}

#if ENABLE_ATLAS_SIMULATOR
err_t AtlasPH::getSimulatedReading(char *buffer, size_t bufferSize) {
    double pH = simulatedPH;

#if 0
    double nextPH = pH + simulatedPHIncrement;

    if (nextPH < simulatedPHMin || nextPH > simulatedPHMax) {
        simulatedPHIncrement *= -1.0;
        nextPH = pH + simulatedPHIncrement;
    }
    simulatedPH = nextPH;
#endif

    snprintf(buffer, bufferSize, "\x01" "%0.03f", pH);

    // _logi("pH: %0.03f", pH);

    return 0;
}
#endif

err_t AtlasPH::init(const char *name, uint8_t i2cSlaveAddress, DispatchTask *task) {
    return AtlasSensor::init(name, i2cSlaveAddress, task);
}

bool AtlasPH::isSetTemperatureCompensationAndTakeReadingSupported() {
    return firmwareMajorVersion > 2 || (firmwareMajorVersion == 2 && firmwareMinorVersion >= 12);
}

err_t AtlasPH::sendCalibration(CalibrationPoint calibrationPoint, double calibrationSolutionPH, bool synchronous, void *context, CommandCallback callback) {
    const char *calibrationPointString;

    switch (calibrationPoint) {
        case CalibrationPoint::high:    calibrationPointString = "high";    break;
        case CalibrationPoint::low:     calibrationPointString = "low";     break;
        case CalibrationPoint::mid:     calibrationPointString = "mid";     break;
        default:                        return EINVAL;
    }

    return makeAndSendCommand<Response>(synchronous, "cal,%s,%0.3f", context, callback, nullptr, 900, Priority::defaultPriority, CompletionBehavior::dequeue, calibrationPointString, calibrationSolutionPH);
}

err_t AtlasPH::sendGetSlope(bool synchronous, void *context, SlopeResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            SlopeResponse &slopeResponse = static_cast<SlopeResponse &>(response);

            if (!slopeResponse.err) {
                _logd("%s probe slope acid match %0.1f%%, base match %0.1f%%, mV from zero %0.2f",
                    sensor->getName(),
                    slopeResponse.acidCalibrationToIdealProbe,
                    slopeResponse.baseCalibrationToIdealProbe,
                    slopeResponse.millivoltsZeroPointIsOffFromTrueZero);
            }
        };
    }
    err = makeCommand<SlopeResponse>(command, "slope,?", context, callback, "?slope,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            // AtlasPH *phSensor = static_cast<AtlasPH *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "?Slope,99.7,100.3,-0.89");
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

AtlasPH &AtlasPH::shared() {
    static AtlasPH *singleton = new AtlasPH();

    return *singleton;
}

// --- AtlasPH::SlopeResponse ---

err_t AtlasPH::SlopeResponse::parse(char *response) {
    err_t err = Response::parse(response);

    if (!err && sscanf(responseString, "%lf,%lf,%lf", &acidCalibrationToIdealProbe, &baseCalibrationToIdealProbe, &millivoltsZeroPointIsOffFromTrueZero) != 3) err = EBADMSG;

    return err;
}
