#include "atlasRTD.h"
#include "atlasTemperatureCompensatedSensor.h"

double AtlasRTD::getCurrentTemperature() {
    double value = getLastReading().value;

#if ENABLE_RTD_CLAMP_TO_25C
    // we're expect to be testing water, so values significantly out of bounds
    // probably mean the rtd probe isn't hooked up.
    if (value <= -10.0 || value >= 110.0) {
        value = 20.0;
    }
#endif

    return value;
}

#if ENABLE_ATLAS_SIMULATOR
err_t AtlasRTD::getSimulatedReading(char *buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "\x01" "20.000");
    return 0;
}
#endif

err_t AtlasRTD::init(const char *name, uint8_t i2cSlaveAddress, DispatchTask *task) {
    err_t err = AtlasSensor::init(name, i2cSlaveAddress, task, true);

    if (!err) err = sendSetDataLoggerInterval(0);
    if (!err) err = sendSetTemperatureScale(TemperatureScale::celsius);
    if (!err) err = enqueueSendGetReading();

    return err;
}

err_t AtlasRTD::sendCalibration(double temperature, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "cal,%0.3f", context, callback, nullptr, 600, Priority::defaultPriority, CompletionBehavior::dequeue, temperature);
}

err_t AtlasRTD::sendClearMemory(bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "m,clear", context, callback);
}

err_t AtlasRTD::sendGetDataLoggerInterval(bool synchronous, void *context, IntResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            IntResponse &intResponse = static_cast<IntResponse &>(response);

            if (!intResponse.err) {
                if (intResponse.value) {
                    _logd("%s data logger interval is %d (%d seconds)", sensor->getName(), intResponse.value, intResponse.value * 10);
                } else {
                    _logd("%s data logger is disabled", sensor->getName());
                }
            }
        };
    }
    err = makeCommand<IntResponse>(command, "d,?", context, callback, "?d,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            AtlasRTD *rtdSensor = static_cast<AtlasRTD *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "?D,%d", rtdSensor->dataLoggerInterval);
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasRTD::sendGetMemoryLastStoredValue(bool synchronous, void *context, MemoryResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            MemoryResponse &memoryResponse = static_cast<MemoryResponse &>(response);

            if (!memoryResponse.err) {
                _logd("%s memory last stored index index %d: %0.3f", sensor->getName(), memoryResponse.valueIndex, memoryResponse.value);
            }
        };
    }
    err = makeCommand<MemoryResponse>(command, "m,?", context, callback);
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            // AtlasRTD *rtdSensor = static_cast<AtlasRTD *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "1,%0.2f", AtlasTemperatureCompensatedSensor::defaultTemperatureC);
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasRTD::sendGetMemoryNextValue(bool synchronous, void *context, MemoryResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            MemoryResponse &memoryResponse = static_cast<MemoryResponse &>(response);

            if (!memoryResponse.err) {
                _logd("%s memory at index %d: %0.3f", sensor->getName(), memoryResponse.valueIndex, memoryResponse.value);
            }
        };
    }
    err = makeCommand<MemoryResponse>(command, "m", context, callback);
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            // AtlasRTD *rtdSensor = static_cast<AtlasRTD *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "1,%0.2f", AtlasTemperatureCompensatedSensor::defaultTemperatureC);
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasRTD::sendGetTemperatureScale(bool synchronous, void *context, TemperatureScaleResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            TemperatureScaleResponse &temperatureScaleResponse = static_cast<TemperatureScaleResponse &>(response);

            if (!temperatureScaleResponse.err) {
                _logd("%s temperature scale set to %s",
                    sensor->getName(),
                    temperatureScaleResponse.temperatureScaleString);
            }
        };
    }
    err = makeCommand<TemperatureScaleResponse>(command, "s,?", context, callback, "?s,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            AtlasRTD *rtdSensor = static_cast<AtlasRTD *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "?S,%c", rtdSensor->temperatureScale);
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasRTD::sendSetDataLoggerInterval(int dataLoggerInterval, bool synchronous, void *context, CommandCallback callback) {
    if (dataLoggerInterval < 0 || dataLoggerInterval > 32000) return EINVAL;

    err_t err = makeAndSendCommand<Response>(synchronous, "d,%d", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, dataLoggerInterval);

#if ENABLE_ATLAS_SIMULATOR
    if (!err) this->dataLoggerInterval = dataLoggerInterval;
#endif

    return err;
}

err_t AtlasRTD::sendSetTemperatureScale(TemperatureScale temperatureScale, bool synchronous, void *context, CommandCallback callback) {
    char temperatureScaleChar;

    switch (temperatureScale) {
        case TemperatureScale::celsius:     temperatureScaleChar = 'c';     break;
        case TemperatureScale::fahrenheit:  temperatureScaleChar = 'f';     break;
        case TemperatureScale::kelvin:      temperatureScaleChar = 'k';     break;
        default:                            return EINVAL;
    }

    err_t err = makeAndSendCommand<Response>(synchronous, "s,%c", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, temperatureScaleChar);

#if ENABLE_ATLAS_SIMULATOR
    if (!err) this->temperatureScale = temperatureScaleChar;
#endif

    return err;
}

AtlasRTD &AtlasRTD::shared() {
    static AtlasRTD *singleton = new AtlasRTD();

    return *singleton;
}

#if ATLAS_RTD_ENABLE_PH_SENSOR
AtlasRTD &AtlasRTD::sharedPHSensor() {
    static AtlasRTD singleton;

    return singleton;
}
#endif

// --- AtlasRTD::MemoryResponse ---

err_t AtlasRTD::MemoryResponse::parse(char *response) {
    err_t err = Response::parse(response);

    if (!err && sscanf(responseString, "%d,%lf", &valueIndex, &value) != 2) err = EBADMSG;

    return err;
}

// --- AtlasRTD::TemperatureScaleResponse ---

err_t AtlasRTD::TemperatureScaleResponse::parse(char *response) {
    err_t err = Response::parse(response);

    if (!err) {
        switch (*responseString) {
            case 'c': {
                temperatureScale = TemperatureScale::celsius;
                temperatureScaleString = "celsius";
            } break;

            case 'f': {
                temperatureScale = TemperatureScale::fahrenheit;
                temperatureScaleString = "fahrenheit";
            } break;

            case 'k': {
                temperatureScale = TemperatureScale::kelvin;
                temperatureScaleString = "kelvin";
            } break;

            default:    err = EBADMSG;                                      break;
        }
    }

    return err;
}
