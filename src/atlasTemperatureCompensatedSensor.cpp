//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "atlasTemperatureCompensatedSensor.h"

AtlasTemperatureCompensatedSensor::AtlasTemperatureCompensatedSensor(TemperatureProvider *temperatureProvider) :
    temperatureProvider(temperatureProvider)
{ }

double AtlasTemperatureCompensatedSensor::getCurrentTemperature() {
    if (isForcedTemperature) {
        return forcedDegreesC;
    } else {
        return temperatureProvider ? temperatureProvider->getCurrentTemperature() : DBL_MIN;
    }
}

uint32_t AtlasTemperatureCompensatedSensor::getRollingMeanNumberOfValues() {
    // store one minute of readings
    return uint32_t(ceil((60.0 * 1000.0) / double(getTemperatureCompensatedReadingResponseWaitMs())));
}

bool AtlasTemperatureCompensatedSensor::isSetTemperatureCompensationAndTakeReadingSupported() {
    return true;
}

err_t AtlasTemperatureCompensatedSensor::sendGetReading(bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior) {
    if (!temperatureProvider) {
        return AtlasSensor::sendGetReading(synchronous, context, callback, priority, completionBehavior);
    }

    // if the firmware supports the RT command, use it
    if (isSetTemperatureCompensationAndTakeReadingSupported()) {
        _logw("%s isSetTemperatureCompensationAndTakeReadingSupported is true, calling sendSetTemperatureCompensationAndTakeReading()", getName());

        return sendSetTemperatureCompensationAndTakeReading(nullptr, synchronous, context, callback, priority, completionBehavior);
    }

    // otherwise use two commands (T then R) to manually set the temperature
    // compensation and take a reading.
    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            AtlasTemperatureCompensatedSensor *tcSensor = static_cast<AtlasTemperatureCompensatedSensor *>(sensor);
            if (!response.err) tcSensor->handleReading(response);
        };
    }

    struct Context {
        CommandCallback     callback;
        CompletionBehavior  completionBehavior;
        void *              context;
        bool                hasSetTemperatureCompensation;
    };
    
    Command *command = nullptr;
    const size_t commandBufferSize = 16;  
    Context *commandContext;
    err_t err = 0;

    CommandCallback commandCallback = [](AtlasSensor *sensor, void *context, Response &response) {
        Context *commandContext = static_cast<Context *>(context);
        commandContext->callback(sensor, commandContext->context, response);
    };
    ProcessingCallback processingCallback = [](AtlasSensor *sensor, Command *command) {
        Context *commandContext = static_cast<Context *>(command->completionContext);
        commandContext->hasSetTemperatureCompensation = !commandContext->hasSetTemperatureCompensation;
    };
    SendCallback sendCallback = [](AtlasSensor *sensor, Command *command) -> err_t {
        Context *commandContext = static_cast<Context *>(command->completionContext);

        if (!commandContext->hasSetTemperatureCompensation) {
            AtlasTemperatureCompensatedSensor *tcSensor = static_cast<AtlasTemperatureCompensatedSensor *>(sensor);
            double temperature;

            if (tcSensor->isForcedTemperature) {
                temperature = tcSensor->forcedDegreesC;
            } else if (tcSensor->temperatureProvider) {
                temperature = tcSensor->temperatureProvider->getCurrentTemperature();
            } else {
#if ENABLE_ATLAS_SIMULATOR
                temperature = tcSensor->isSimulatorEnabled ? tcSensor->temperatureCompensationDegreesC : AtlasTemperatureCompensatedSensor::defaultTemperatureC;
#else
                temperature = AtlasTemperatureCompensatedSensor::defaultTemperatureC;
#endif
            }
            snprintf(command->commandString, commandBufferSize, "t,%0.3f", temperature);
            command->completionBehavior = CompletionBehavior::resend;
#if ENABLE_ATLAS_SIMULATOR
            command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
                snprintf((char *) buffer, bufferSize, "\x01");
                return 0;
            };
#endif
        } else {
            snprintf(command->commandString, commandBufferSize, "r");
            command->completionBehavior = commandContext->completionBehavior;
#if ENABLE_ATLAS_SIMULATOR
            command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
                return sensor->getSimulatedReading((char *) buffer, bufferSize);
            };
#endif
        }

        return 0;
    };

    if ((commandContext = new Context) == nullptr) err = ENOMEM;
    if (!err) {
        commandContext->callback = callback;
        commandContext->completionBehavior = completionBehavior;
        commandContext->context = context;
        commandContext->hasSetTemperatureCompensation = false;

        err = makeCommand<Response>(
            command,
            "...............",  // commandBufferSize
            commandContext,
            commandCallback,
            nullptr,
            getSetTemperatureCompensatedResponseWaitMs(),
            priority,
            CompletionBehavior::resend
        );
    }
    if (!err) {
        command->processingCallback = processingCallback;
        command->sendCallback = sendCallback;
        command->shouldFreeCompletionContext = true;

        commandContext = nullptr;

        enqueueCommand(command);
        err = send(synchronous);
    }

    _delete(commandContext);

    return err;
}

err_t AtlasTemperatureCompensatedSensor::sendGetTemperatureCompensation(bool synchronous, void *context, DoubleResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            DoubleResponse &doubleResponse = static_cast<DoubleResponse &>(response);

            if (!doubleResponse.err) {
                _logi("%s temperature compensation value is %0.3f", sensor->getName(), doubleResponse.value);
            }
        };
    }
    err = makeCommand<DoubleResponse>(command, "t,?", context, callback, "?t,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            AtlasTemperatureCompensatedSensor *tcSensor = static_cast<AtlasTemperatureCompensatedSensor *>(sensor);
            snprintf((char *) buffer, bufferSize, "\x01" "?T,%0.3f", tcSensor->temperatureCompensationDegreesC);
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasTemperatureCompensatedSensor::sendSetTemperatureCompensation(double degreesC, bool synchronous, void *context, CommandCallback callback) {
    return sendSetTemperatureCompensation(&degreesC, synchronous, context, callback, Priority::defaultPriority, CompletionBehavior::dequeue);
}

err_t AtlasTemperatureCompensatedSensor::sendSetTemperatureCompensation(double *degreesC, bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior) {
    Command *command = nullptr;
    err_t err;
    SendCallback sendCallback = nullptr;
    double temperature;

    // honor the forced temperature setting
    if (isForcedTemperature) {
        degreesC = &forcedDegreesC;
    }
    if (degreesC) {
        // if a temperature is specified, set the temperature compensation with it.
        temperature = *degreesC;
    } else {
        // otherwise, get the temperature from the temperature provider or simulator
        temperature = AtlasTemperatureCompensatedSensor::defaultTemperatureC;
        sendCallback = [](AtlasSensor *sensor, Command *command) -> err_t {
            err_t err = 0;
            char *string;
            AtlasTemperatureCompensatedSensor *tcSensor = static_cast<AtlasTemperatureCompensatedSensor *>(sensor);
            double temperature;

            if (tcSensor->temperatureProvider) {
                temperature = tcSensor->temperatureProvider->getCurrentTemperature();
            } else {
#if ENABLE_ATLAS_SIMULATOR
                temperature = tcSensor->isSimulatorEnabled ? tcSensor->temperatureCompensationDegreesC : AtlasTemperatureCompensatedSensor::defaultTemperatureC;
#else
                temperature = AtlasTemperatureCompensatedSensor::defaultTemperatureC;
#endif
            }
            if (asprintf(&string, "t,%0.3f", temperature) < 0) err = ENOMEM;
            if (!err) {
                free(command->commandString);
                command->commandString = string;
            }

            return err;
        };
    }
    err = makeCommand<Response>(command, "t,%0.3f", context, callback, nullptr, defaultResponseWaitMs, priority, completionBehavior, temperature);
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        temperatureCompensationDegreesC = temperature;
#endif
        command->sendCallback = sendCallback;
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasTemperatureCompensatedSensor::sendSetTemperatureCompensationAndTakeReading(double degreesC, bool synchronous, void *context, CommandCallback callback) {
    return sendSetTemperatureCompensationAndTakeReading(&degreesC, synchronous, context, callback, Priority::defaultPriority, CompletionBehavior::dequeue);
}

err_t AtlasTemperatureCompensatedSensor::sendSetTemperatureCompensationAndTakeReading(double *degreesC, bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior) {
    if (!isSetTemperatureCompensationAndTakeReadingSupported()) return ENOTSUP;

    Command *command = nullptr;
    err_t err;
    SendCallback sendCallback = nullptr;
    double temperature;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            AtlasTemperatureCompensatedSensor *tcSensor = static_cast<AtlasTemperatureCompensatedSensor *>(sensor);
            if (!response.err) tcSensor->handleReading(response);
        };
    }
    // honor the forced temperature setting
    if (isForcedTemperature) {
        degreesC = &forcedDegreesC;
    }
    if (degreesC) {
        // if a temperature is specified, set the temperature compensation with it.
        temperature = *degreesC;
    } else {
        // otherwise, get the temperature from the temperature provider or simulator
        temperature = AtlasTemperatureCompensatedSensor::defaultTemperatureC;
        sendCallback = [](AtlasSensor *sensor, Command *command) -> err_t {
            err_t err = 0;
            char *string;
            AtlasTemperatureCompensatedSensor *tcSensor = static_cast<AtlasTemperatureCompensatedSensor *>(sensor);
            double temperature;

            if (tcSensor->isForcedTemperature) {
                temperature = tcSensor->forcedDegreesC;
            } else if (tcSensor->temperatureProvider) {
                temperature = tcSensor->temperatureProvider->getCurrentTemperature();
            } else {
#if ENABLE_ATLAS_SIMULATOR
                temperature = tcSensor->isSimulatorEnabled ? tcSensor->temperatureCompensationDegreesC : AtlasTemperatureCompensatedSensor::defaultTemperatureC;
#else
                temperature = AtlasTemperatureCompensatedSensor::defaultTemperatureC;
#endif
            }
            if (asprintf(&string, "rt,%0.3f", temperature) < 0) err = ENOMEM;
            if (!err) {
                // _logi("sending %s from %s", string, sensor->getName());
                free(command->commandString);
                command->commandString = string;
            }

            return err;
        };
    }
    err = makeCommand<Response>(command, "rt,%0.3f", context, callback, nullptr, getTemperatureCompensatedReadingResponseWaitMs(), priority, completionBehavior, temperature);
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            return sensor->getSimulatedReading((char *) buffer, bufferSize);
        };
#endif
        command->sendCallback = sendCallback;
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasTemperatureCompensatedSensor::setForcedTemperature(bool isEnabled, double forcedDegreesC, bool shouldSendSetTemperatureCompensation, bool synchronous) {
    err_t err = 0;

    lock();
    this->forcedDegreesC = forcedDegreesC;
    this->isForcedTemperature = isEnabled;
    unlock();
    
    if (isEnabled && shouldSendSetTemperatureCompensation) {
        err = sendSetTemperatureCompensation(forcedDegreesC, synchronous);
    }

    return err;
}
