//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "atlasSensor.h"

// response byte (1) + largest string (40) + terminator (1: '\0')
#define EZO_BUFFER_SIZE     42

// --- AtlasSensor ---

I2C &AtlasSensor::i2c = I2C::shared(I2C_NUM_0);

AtlasSensor::ReadingMessage::ReadingMessage(double value, UnixTime when) :
    Message(uint32_t(MessageTag::read), when),
    value(value)
{ }

AtlasSensor::AtlasSensor() {
    // isDumpResponseBufferEnabled = true;
    // isLogSentCommandsEnabled = true;
}

AtlasSensor::~AtlasSensor() {
    if (i2cDevice) i2c.unregisterDevice(i2cDevice);
    if (timer) {
        timer->stop();
        timer->release();
    }
}

double AtlasSensor::convertReadingResponseToDouble(char *response) {
    if (!(response && *response)) return DBL_MIN;

    double value;

    if (sscanf(response, "%lf", &value) != 1) {
        value = DBL_MIN;
        loge("sscanf failed to convert '%s' to double", response);
        dump(response, strlen(response));
    }

    return value;
}

void AtlasSensor::enqueueCommand(Command *command) {
    lock();

    Command **p;

    for (p = &commands; *p && (*p)->priority >= command->priority; p = &(*p)->next) ;

    if (isStopped) command->response->err = EINTR;
    command->next = *p;
    *p = command;

    unlock();
}

err_t AtlasSensor::enqueueSendGetReading() {
    err_t err;

    lock();

    if (isGetReadingActive) {
        err = 0;
    } else {
        err = sendGetReading(false, nullptr, nullptr, Priority::read, CompletionBehavior::reenqueue);
        if (!err) isGetReadingActive = true;
    }

    unlock();

    return err;
}

void AtlasSensor::eventHandler(void *context, DispatchEventSource *source) {
    ((AtlasSensor *) context)->handleEvent(source);
}

AtlasSensor::Reading AtlasSensor::getLastReading() {
    Reading result;

    lock();

    result = lastReading;

    unlock();

    return result;
}

double AtlasSensor::getLastValue() {
    return getLastReading().value;
}

uint32_t AtlasSensor::getReadingResponseWaitMs() {
    return 600;
}

#if ENABLE_ATLAS_SIMULATOR
err_t AtlasSensor::getSimulatedReading(char *buffer, size_t bufferSize) { return EINVAL; }
#endif

void AtlasSensor::handleEvent(DispatchEventSource *source) {
    lock();
    Command *command = pendingCommand;
    unlock();

    if (!command) return;

    // _logi("in handleEvent, command is %s", command->commandString);

    char buffer[EZO_BUFFER_SIZE] = {0};
    Response *response = command->response;
    err_t err = response->err;

    if (!err && command->responseWaitMs) {
        // If readResponse returns busy we haven't waited long enough for
        // the command completion. In this case, try again in 100ms.
        if ((err = readResponse(command, buffer, sizeof(buffer))) == EBUSY) {
            timer->startOnce(100 * 1000);
            return;
        }
        if (!err) {
            _logv("%s command '%s' response '%s'", getName(), command->commandString, buffer);
            err = command->response->parse(buffer);
        }
    }

    if (err) {
        _loge("%s sensor command '%s' failed with error %d %lld", getName(), command->commandString, err, esp_timer_get_time());

        response->err = err;

        if (command->completionBehavior != CompletionBehavior::reenqueue) {
            command->completionBehavior = CompletionBehavior::dequeue;
        }
    }

    if (command->processingCallback) command->processingCallback(this, command);

    switch (command->completionBehavior) {
        case CompletionBehavior::dequeue: {
            _logv("dequeue %s", command->commandString);

            if (command->completionCallback) command->completionCallback(this, command->completionContext, *command->response);

            lock();
            pendingCommand = nullptr;
            unlock();

            if (command->taskToWake) {
                *command->err = response->err;
                xTaskNotifyGive(command->taskToWake);
            }
            
            delete command;
        } break;

        case CompletionBehavior::reenqueue: {
            _logv("reenqueue %s", command->commandString);

            if (command->completionCallback) {
                command->completionCallback(this, command->completionContext, *command->response);
            }

            lock();
            pendingCommand = nullptr;
            unlock();

            command->prepareForReuse();
            enqueueCommand(command);
        } break;

        case CompletionBehavior::resend: {
            _logv("resend %s", command->commandString);

            command->prepareForReuse();
        } break;
    }

    send(false);
}

void AtlasSensor::handleReading(Response &response) {
    err_t err = 0;
    ReadingMessage *message = nullptr;
    double value = convertReadingResponseToDouble(response.responseString);

    // if the value is garbage don't report it
    if (value == DBL_MIN) return;

    double when = getCurrentTime();

    // _logi("%s sensor response string is '%s', value is %0.3f", getName(), response.responseString, value);

    lock();

    lastReading.value = isForcedValue ? forcedValue : value;
    lastReading.when = when;

    unlock();

    if ((message = new ReadingMessage(value, when)) == nullptr) setErr(ENOMEM);
    if (!err) notifyObservers(message);
}

err_t AtlasSensor::init(const char *name, uint8_t i2cSlaveAddress, DispatchTask *task) {
    return init(name, i2cSlaveAddress, task, false);
}

err_t AtlasSensor::init(const char *name, uint8_t i2cSlaveAddress, DispatchTask *task, bool deferEnqueueSendGetReading) {
    err_t err = setName(name);

    if (!err) err = i2c.registerDevice(i2cSlaveAddress, i2cDevice);

#if ENABLE_ATLAS_SIMULATOR
    if (!err) {
        isSimulatorEnabled = true;
        _logi("%s simulator enabled", getName());
    }
#endif
    if (!err && (timer = new DispatchTimerSource()) == nullptr) setErr(ENOMEM);
    if (!err) err = timer->init(eventHandler, this, name, task);
    if (!err) err = sendGetInfo();
    if (!err) err = sendGetStatus();
    if (!err) err = sendGetCalibration();
    if (!err) err = sendSetLED(false, false);
    if (!err) err = sendSetProtocolLock(true, false);
    if (!err && !deferEnqueueSendGetReading) err = enqueueSendGetReading();

    if (err) _release(timer);

    return err;
}

bool AtlasSensor::isForcedValueEnabled(double *forcedValue) {
    lock();

    bool isEnabled = isForcedValue;

    if (forcedValue) *forcedValue = this->forcedValue;

    unlock();

    return isEnabled;
}

err_t AtlasSensor::readResponse(Command *command, char *responseBuffer, size_t responseBufferSize) {
    // _logi("in readResponse, command is %s", command->commandString);

    uint8_t buffer[EZO_BUFFER_SIZE] = {0};
    err_t err;

    if (responseBuffer == nullptr || responseBufferSize < sizeof(buffer)) return EINVAL;

#if !ENABLE_ATLAS_SIMULATOR
    // _logi("calling i2c.read(), buffer size is %d", sizeof(buffer));
    err = i2c.read(i2cDevice, buffer, sizeof(buffer));
#else
    if (!isSimulatorEnabled) {
        err = i2c.read(i2cDevice, buffer, sizeof(buffer));
    } else if (command->responseSimulator) {
        err = command->responseSimulator(this, buffer, sizeof(buffer));
    } else {
        buffer[0] = 1;
        buffer[1] = 0;
        err = 0;
    }
#endif
    if (!err) {
        // we're expecting a response byte here
        switch (buffer[0]) {
            case 1: {
                _logv("%s sensor returned successful request", getName());
            } break;

            case 2: {
                _loge("%s sensor returned syntax error", getName());
                err = EINVAL;
            } break;

            case 254: {
                _logw("%s sensor returned still processing, not ready", getName());
                err = EBUSY;
            } break;

            case 255: {
                // I think "no data to send" happens if there is no active command.
                // This shouldn't be able to happen given the design of this class.
                // If it does it's an error and there's nothing to do but
                // return a command failure.
                _loge("%s sensor returned no data to send", getName());
                err = ENODATA;
            } break;

            default: {
                _loge("%s sensor returned unexpected response byte %d, aborting", getName(), buffer[0]);
                err = EBADMSG;
            } break;
        }

        if (err || isDumpResponseBufferEnabled) dump(buffer, sizeof(buffer));
    }
    if (!err) {
        // copy data out of the buffer to the response string
        int i = 0;
        int n = sizeof(buffer) - 1;

        while (++i < n && (*responseBuffer++ = char(buffer[i]))) ;

        if (i == n) err = ENOSPC;
    }

    return err;
}

err_t AtlasSensor::send(bool synchronous) {
    Command *command;
    err_t err = 0;

    lock();

    if ((command = pendingCommand)) {
        if (command->hasSent) err = EBUSY;
    } else {
        if ((command = commands) == nullptr) err = ENOENT;
        if (!err) {
            commands = command->next;
            command->next = nullptr;
            pendingCommand = command;
        }
    }

    if (!err && isStopped) {
        command->completionBehavior = CompletionBehavior::dequeue;
        err = EINTR;
    }
    if (!err) command->hasSent = true;

    unlock();

    if (err == EBUSY || err == ENOENT) {
        // EBUSY/ENOENT are flags to stop processing, not errors
        return 0;
    }

    bool fireTimerImmediately = err != 0;

    if (!err && command->sendCallback) {
        err = command->sendCallback(this, command);
    }

    command->response->err = err;

    if (synchronous) {
        command->err = &err;
        command->taskToWake = xTaskGetCurrentTaskHandle();
    }
    if (!err) {
        if (isLogSentCommandsEnabled) _logi("%s -> %s", getName(), command->commandString);
#if !ENABLE_ATLAS_SIMULATOR
        err = i2c.write(i2cDevice, command->commandString);
#else
        if (!isSimulatorEnabled) err = i2c.write(i2cDevice, command->commandString);
#endif
    }
    if (!err) {
        _logv("wrote '%s' to I2C slave @ 0x%x)", command->commandString, i2cDevice->address);
        
        if (command->responseWaitMs) {
            err = timer->startOnce(command->responseWaitMs * 1000);
        } else {
            fireTimerImmediately = true;
        }
    }

    if (err) command->response->err = err;
    if (err || fireTimerImmediately) {
        // here we don't expect to receive a response from the ezo device.
        // this is either because of an error or because the command doesn't
        // support it. either way we consider the command finished and
        // need to trigger the timer so resources can be cleaned up.

        // if there's an error and we aren't 
        timer->dispatchEvent();
    }

    if (synchronous) while (!ulTaskNotifyTake(pdTRUE, portMAX_DELAY));

    return err;
}

err_t AtlasSensor::sendBaud(Baud baud, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "baud,%d", context, callback, nullptr, 0, Priority::defaultPriority, CompletionBehavior::dequeue, int(baud));
}

err_t AtlasSensor::sendClearCalibration(bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "cal,clear", context, callback);
}

err_t AtlasSensor::sendExport(bool synchronous, void *context, ExportResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            ExportResponse &exportResponse = static_cast<ExportResponse &>(response);

            if (exportResponse.err) return;

            const char *name = sensor->getName();

            logi("%s sensor exported %ld of %ld strings", name, exportResponse.numberOfStringsReceived, exportResponse.numberOfStringsToExport);

            for (int i = 0; i < exportResponse.numberOfStringsReceived; ++i) {
                char *string = exportResponse.strings + i * exportResponse.stringSize;
                logi("%s export string %d: '%s'", name, i, string);
            }
        };
    }
    err = makeCommand<ExportResponse>(command, "export,?", context, callback, "?export,");
    if (!err) {
        command->completionBehavior = CompletionBehavior::resend;
        command->processingCallback = [](AtlasSensor *sensor, Command *command) {
            ExportResponse *response = static_cast<ExportResponse *>(command->response);

            if (response->err) return;
            if (response->strings && !response->numberOfStringsReceived) {
                command->commandString[6] = 0;    // "export,?" -> "export"
            }
            if (response->isDone) {
                command->completionBehavior = CompletionBehavior::dequeue;
            }
        };

        enqueueCommand(command);
        send(synchronous);
    }

    return err;
}

err_t AtlasSensor::sendFactoryReset(bool synchronous, void *context, CommandCallback callback) {
    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            logi("factory reset sent to %s sensor", sensor->getName());
            // spin for a few seconds to give the device time to reboot
            delay(3 * 1000);
        };
    }
    return makeAndSendCommand<Response>(synchronous, "factory", context, callback, nullptr, 0);
}

err_t AtlasSensor::sendFind(bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "find", context, callback);
}

err_t AtlasSensor::sendGetCalibration(bool synchronous, void *context, IntResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            IntResponse &intResponse = static_cast<IntResponse &>(response);
            const char *name = sensor->getName();

            if (!intResponse.err) {
                // logi("%s sensor get calibration returned %d", name, intResponse.value);
            } else {
                loge("%s sensor get calibration error: %d", name, intResponse.err);
            }
        };
    }
    err = makeCommand<IntResponse>(command, "cal,?", context, callback, "?cal,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
    command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
        snprintf((char *) buffer, bufferSize, "\x01" "?CAL,%d", sensor->calibrationValue);
        return 0;
    };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasSensor::sendGetInfo(bool synchronous, void *context, InfoResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            err_t err;
            InfoResponse &infoResponse = static_cast<InfoResponse &>(response);

            if (!infoResponse.err) {
                sensor->firmwareMajorVersion = infoResponse.firmwareMajorVersion;
                sensor->firmwareMinorVersion = infoResponse.firmwareMinorVersion;
                setErr(sensor->setName(infoResponse.sensorType));

                logi("%s sensor at 0x%x has firmware version %s", infoResponse.sensorType, sensor->i2cDevice->address, infoResponse.firmwareVersion);
            }
        };
    }
    err = makeCommand<InfoResponse>(command, "i", context, callback, "?i,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            snprintf((char *) buffer, bufferSize, "\x01" "?i,%s,1.23", sensor->getName());
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasSensor::sendGetLED(bool synchronous, void *context, BoolResponseCallback callback) {
    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            BoolResponse &boolResponse = static_cast<BoolResponse &>(response);

            if (!boolResponse.err) logi("%s sensor LED is %senabled", sensor->getName(), boolResponse.isEnabled ? "" : "not ");
        };
    }
    return makeAndSendCommand<BoolResponse>(synchronous, "l,?", context, callback, "?l,");
}

err_t AtlasSensor::sendGetName(bool synchronous, void *context, CommandCallback callback) {
    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            if (!response.err) logi("%s sensor name is '%s'", sensor->getName(), response.responseString);
        };
    }
    return makeAndSendCommand<Response>(synchronous, "name,?", context, callback, "?name,");
}

err_t AtlasSensor::sendGetProtocolLock(bool synchronous, void *context, BoolResponseCallback callback) {
    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            BoolResponse &boolResponse = static_cast<BoolResponse &>(response);

            if (!boolResponse.err) logi("%s sensor protocol lock is %senabled", sensor->getName(), boolResponse.isEnabled ? "" : "not ");
        };
    }
    return makeAndSendCommand<BoolResponse>(synchronous, "plock,?", context, callback, "?plock,");
}

err_t AtlasSensor::sendGetReading(bool synchronous, void *context, CommandCallback callback) {
    return sendGetReading(synchronous, context, callback, Priority::defaultPriority, CompletionBehavior::dequeue);
}

err_t AtlasSensor::sendGetReading(bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            if (!response.err) sensor->handleReading(response);
        };
    }
    err = makeCommand<Response>(command, "r", context, callback, nullptr, getReadingResponseWaitMs(), priority, completionBehavior);
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            return sensor->getSimulatedReading((char *) buffer, bufferSize);
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasSensor::sendGetStatus(bool synchronous, void *context, StatusResponseCallback callback) {
    Command *command = nullptr;
    err_t err;

    if (callback == nullptr) {
        callback = [](AtlasSensor *sensor, void *context, Response &response) {
            StatusResponse &statusResponse = static_cast<StatusResponse &>(response);

            if (!statusResponse.err) logi("%s sensor at 0x%x restarted due to %s, voltage at Vcc %s", sensor->getName(), sensor->i2cDevice->address, statusResponse.restartReason, statusResponse.voltageAtVcc);
        };
    }
    err = makeCommand<StatusResponse>(command, "status", context, callback, "?status,");
    if (!err) {
#if ENABLE_ATLAS_SIMULATOR
        command->responseSimulator = [](AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize) -> err_t {
            snprintf((char *) buffer, bufferSize, "\x01" "?Status,P,1.234");
            return 0;
        };
#endif
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
}

err_t AtlasSensor::sendImport(const char **strings, int stringsCount, bool synchronous, void *context, ImportResponseCallback callback) {
    if (strings == nullptr || stringsCount < 1) return EINVAL;

    Command *command = nullptr;
    err_t err = 0;
    int i, j;
    char **stringsCopy = nullptr;

    if (--stringsCount) {
        if ((stringsCopy = (char **) calloc(stringsCount, sizeof(char *))) == nullptr) err = ENOMEM;
        for (i = 0, j = 1; !err && i < stringsCount; ++i, ++j) {
            if ((stringsCopy[i] = strdup(strings[j])) == nullptr) err = ENOMEM;
        }
    }
    if (!err) err = makeCommand<ImportResponse>(command, "import,xxxxxxxxxxxx", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::resend);
    if (!err) {
        ImportResponse *response = static_cast<ImportResponse *>(command->response);

        response->strings = stringsCopy;
        response->stringsCount = stringsCount;
        stringsCopy = nullptr;     // will be released by ~Import

        snprintf(command->commandString, ExportResponse::stringSize, "import,%s", strings[0]);

        command->processingCallback = [](AtlasSensor *sensor, Command *command) {
            ImportResponse *response = static_cast<ImportResponse *>(command->response);

            if (response->err) return;
            if (response->strings && response->stringsSent < response->stringsCount) {
                snprintf(command->commandString, ExportResponse::stringSize, "import,%s", response->strings[response->stringsSent++]);
            } else {
                command->completionBehavior = CompletionBehavior::dequeue;
            }
        };

        enqueueCommand(command);
        send(synchronous);
    } else {
        if (stringsCopy) {
            for (i = 0; i < stringsCount; ++i) _free(stringsCopy[i]);
            free(stringsCopy);
        }
    }

    return err;
}

err_t AtlasSensor::sendSetI2CAddress(uint8_t i2cSlaveAddress, bool synchronous, void *context, CommandCallback callback) {
    if (i2cSlaveAddress < 1 || i2cSlaveAddress > 127) return ERANGE;

    return makeAndSendCommand<Response>(synchronous, "i2c,%u", context, callback, nullptr, 0, Priority::defaultPriority, CompletionBehavior::dequeue, i2cSlaveAddress);
}

err_t AtlasSensor::sendSetLED(bool isEnabled, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "l,%u", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, int(isEnabled));
}

err_t AtlasSensor::sendSetName(const char *name, bool synchronous, void *context, CommandCallback callback) {
    static const int maxNameLength = 16;

    if (name == nullptr || !*name || strlen(name) > maxNameLength) return EINVAL;

    return makeAndSendCommand<Response>(synchronous, "name,%s", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, name);
}

err_t AtlasSensor::sendSetProtocolLock(bool isEnabled, bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "plock,%u", context, callback, nullptr, defaultResponseWaitMs, Priority::defaultPriority, CompletionBehavior::dequeue, int(isEnabled));
}

err_t AtlasSensor::sendSleep(bool synchronous, void *context, CommandCallback callback) {
    return makeAndSendCommand<Response>(synchronous, "sleep", context, callback, nullptr, 0);
}

void AtlasSensor::setForcedValue(bool isEnabled, double forcedValue) {
    lock();

    this->isForcedValue = isEnabled;
    this->forcedValue = forcedValue;

    unlock();
}

void AtlasSensor::stop() {
    lock();
    isStopped = true;
    unlock();

    for (bool done = false;;) {
        lock();

        done = pendingCommand == nullptr && commands == nullptr;

        unlock();

        if (done) break;

        delay(1000);
    }

    timer->stop();
}

// --- AtlasSensor::BoolResponse ---

err_t AtlasSensor::BoolResponse::parse(char *response) {
    err_t err = Response::parse(response);
    if (!err) {
        switch (*this->responseString) {
            case '0':   isEnabled = false;      break;
            case '1':   isEnabled = true;       break;
            default:    err = EBADMSG;          break;
        }
    }
    return err;
}

// --- AtlasSensor::Command ---

AtlasSensor::Command::~Command() {
    _free(commandString);
    if (shouldFreeCompletionContext) _free(completionContext);
    delete response;
}

void AtlasSensor::Command::prepareForReuse() {
    response->err = ENODATA;
    hasSent = false;
}

// --- AtlasSensor::DoubleResponse ---

err_t AtlasSensor::DoubleResponse::parse(char *response) {
    err_t err = Response::parse(response);

    if (!err && sscanf(responseString, "%lf", &value) != 1) err = EBADMSG;

    return err;
}

// --- AtlasSensor::ExportResponse ---

err_t AtlasSensor::ExportResponse::parse(char *response) {
    err_t err = 0;

    if (strings == nullptr) {
        uint32_t numberOfBytesToExport = 0;
        err = Response::parse(response);
        if (!err && sscanf(this->responseString, "%lu,%lu", &numberOfStringsToExport, &numberOfBytesToExport) != 2) err = EBADMSG;
        if (!err && !numberOfStringsToExport) err = EBADMSG;
        if (!err) {
            size_t size = numberOfStringsToExport * stringSize;
            if ((strings = (char *) calloc(size, sizeof(char))) == nullptr) err = ENOMEM;
        }
    } else if (numberOfStringsReceived < numberOfStringsToExport) {
        char *p = strings + numberOfStringsReceived++ * stringSize;
        strncpy(p, response, stringSize - 1);
    } else {
        if (strcasecmp(response, "*done")) err = EBADMSG;
        else isDone = true;
    }

    return err;
}

// --- AtlasSensor::ImportResponse ---

AtlasSensor::ImportResponse::~ImportResponse() {
    if (strings) {
        for (int i = 0; i < stringsCount; ++i) _free(strings[i]);
        free(strings);
    }
}

// --- AtlasSensor::InfoResponse ---

err_t AtlasSensor::InfoResponse::parse(char *response) {
    err_t err = Response::parse(response);
    if (!err && !(sensorType = field())) err = EBADMSG;
    if (!err && !(firmwareVersion = field())) err = EBADMSG;

    sscanf(firmwareVersion, "%d.%d", &firmwareMajorVersion, &firmwareMinorVersion);

    return err;
}

// --- AtlasSensor::IntResponse ---

err_t AtlasSensor::IntResponse::parse(char *response) {
    err_t err = Response::parse(response);

    if (!err && sscanf(responseString, "%d", &value) != 1) err = EBADMSG;

    return err;
}

// --- AtlasSensor::Response ---

const char *AtlasSensor::Response::field(const char *delimiter) {
    return strsep(&responseString, delimiter);
}

err_t AtlasSensor::Response::parse(char *response) {
    err_t err = 0;
    int prefixLength;

    if (responsePrefix != nullptr && (prefixLength = strlen(responsePrefix)) > 0) {
        if (strncasecmp(response, responsePrefix, prefixLength)) err = EBADMSG;
        if (!err) response += prefixLength;
    }

    this->responseString = response;

    return err;
}

// --- AtlasSensor::StatusResponse ---

err_t AtlasSensor::StatusResponse::parse(char *response) {
    err_t err = Response::parse(response);
    char *p = this->responseString;

    if (!err) {
        switch (*p | 0x20) {
            case 'b':   restartReason = "brownout";         break;
            case 'p':   restartReason = "power off";        break;
            case 's':   restartReason = "software reset";   break;
            case 'w':   restartReason = "watchdog";         break;
            case 'u':   restartReason = "unknown";          break;
            default:    err = EBADMSG;                      break;
        }
    }
    if (!err && (*++p != ',' || !*++p)) err = EBADMSG;
    if (!err) voltageAtVcc = p;

    return err;
}
