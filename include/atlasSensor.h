#pragma once

#include "common.h"
#include "i2c.h"
#include "named.h"
#include "observed.h"

// 2023.06.05 talked to Dmitry @ Atlas Scientific

class AtlasSensor :
    public Named,
    public Observed
{

public:

    enum class Baud {
        baud300 = 300,
        baud1200 = 1200,
        baud2400 = 2400,
        baud9600 = 9600,
        baud19200 = 19200,
        baud38400 = 38400,
        baud57600 = 57600,
        baud115200 = 115200,
    };

    enum class MessageTag { read = 0 };

    struct Reading {
        double                  value = DBL_MIN;
        double                  when = DBL_MIN;
    };

    struct ReadingMessage : public Observed::Message {
        ReadingMessage(double value, UnixTime when);

        double                  value;
    };

    struct Response {
        virtual ~Response() = default;
        
        const char *            field(const char *delimiter = ",");
        virtual err_t           parse(char *response);

        err_t                   err = ENODATA;
        const char *            responsePrefix = nullptr;
        char *                  responseString = nullptr;
    };

    struct BoolResponse : public Response {
        virtual err_t           parse(char *response);

        bool                    isEnabled = false;
    };

    struct DoubleResponse : public Response {
        virtual err_t           parse(char *response);

        double                  value = DBL_MIN;
    };

    struct IntResponse : public Response {
        virtual err_t           parse(char *response);

        int                     value = INT_MIN;
    };
    
    struct ExportResponse : public Response {
        virtual ~ExportResponse() { _free(strings); }
        virtual err_t           parse(char *response);

        bool                    isDone = false;
        uint32_t                numberOfStringsReceived = 0;
        uint32_t                numberOfStringsToExport = 0;
        char *                  strings = nullptr;

        static const size_t     stringSize = 12 + 1;
    };

    struct ImportResponse : public Response {
        virtual ~ImportResponse();

        char **                 strings = nullptr;
        int                     stringsCount = 0;
        int                     stringsSent = 0;
    };

    struct InfoResponse : public Response {
        virtual err_t           parse(char *response);

        int                     firmwareMajorVersion = 0;
        int                     firmwareMinorVersion = 0;
        const char *            firmwareVersion = nullptr;
        const char *            sensorType = nullptr;
    };

    struct StatusResponse : public Response {
        virtual err_t           parse(char *response);

        const char *            restartReason = nullptr;
        const char *            voltageAtVcc = nullptr;
    };

    // subclasses note that the lock is not held during callbacks
    using CommandCallback = void (*)(AtlasSensor *sensor, void *context, Response &response);
    using BoolResponseCallback = CommandCallback;       // response will downcast to BoolResponse &
    using DoubleResponseCallback = CommandCallback;     // response will downcast to DoubleResponse &
    using ExportResponseCallback = CommandCallback;     // response will downcast to ExportResponse &
    using ImportResponseCallback = CommandCallback;     // response will downcast to ImportResponse &
    using InfoResponseCallback = CommandCallback;       // response will downcast to InfoResponse &
    using IntResponseCallback = CommandCallback;        // response will downcast to IntResponse &
    using StatusResponseCallback = CommandCallback;     // response will downcast to StatusResponse &

    AtlasSensor();
    AtlasSensor(AtlasSensor const &) = delete;
   ~AtlasSensor() override;

    void                        operator=(AtlasSensor const &) = delete;

    virtual Reading             getLastReading();
    virtual double              getLastValue();
    virtual uint32_t            getReadingResponseWaitMs();
#if ENABLE_ATLAS_SIMULATOR
    virtual err_t               getSimulatedReading(char *buffer, size_t bufferSize) = 0;
#endif
    virtual err_t               init(const char *name, uint8_t i2cSlaveAddress, DispatchTask *task = nullptr);
    bool                        isForcedValueEnabled(double *forcedValue);
    virtual err_t               sendBaud(Baud baud, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendClearCalibration(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendExport(bool synchronous = true, void *context = nullptr, ExportResponseCallback callback = nullptr);
    virtual err_t               sendFactoryReset(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendFind(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendGetCalibration(bool synchronous = true, void *context = nullptr, IntResponseCallback callback = nullptr);
    virtual err_t               sendGetInfo(bool synchronous = true, void *context = nullptr, InfoResponseCallback callback = nullptr);
    virtual err_t               sendGetLED(bool synchronous = true, void *context = nullptr, BoolResponseCallback callback = nullptr);
    virtual err_t               sendGetName(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendGetProtocolLock(bool synchronous = true, void *context = nullptr, BoolResponseCallback callback = nullptr);
    virtual err_t               sendGetReading(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendGetStatus(bool synchronous = true, void *context = nullptr, StatusResponseCallback callback = nullptr);
    virtual err_t               sendImport(const char **strings, int stringsCount, bool synchronous = true, void *context = nullptr, ImportResponseCallback callback = nullptr);
    virtual err_t               sendSetI2CAddress(uint8_t i2cSlaveAddress, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendSetLED(bool isEnabled, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendSetName(const char *name, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendSetProtocolLock(bool isEnabled, bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    virtual err_t               sendSleep(bool synchronous = true, void *context = nullptr, CommandCallback callback = nullptr);
    void                        setForcedValue(bool isEnabled, double forcedValue = 0);
    virtual void                stop(); // stops recording and clears the command queue

#if ENABLE_ATLAS_SIMULATOR
    bool                        isSimulatorEnabled = false;
#endif

protected:

    enum class CompletionBehavior {
        // dequeue calls the completionCallback then dequeues and deletes the pendingCommand
        dequeue,
        // reenqueue calls the completionCallback then adds the pendingCommand by priority back to the commands queue
        reenqueue,
        // resend does not call the completionCallback and immediately reissues the pendingCommand
        resend
    };

    enum class Priority {
        defaultPriority = 0,
        import = 1,
        read = -1,
    };

    struct Command;

    // subclasses note that the lock is not held during callbacks
    using ProcessingCallback = void (*)(AtlasSensor *sensor, Command *command);
    using SendCallback = err_t (*)(AtlasSensor *sensor, Command *command);

    struct Command {
        ~Command();

        void                    prepareForReuse();

        char *                  commandString = nullptr;
        CompletionBehavior      completionBehavior;
        CommandCallback         completionCallback;         // called with completion results
        void *                  completionContext;
        err_t *                 err = nullptr;
        bool                    hasSent = false;
        Command *               next;
        int                     priority;
        ProcessingCallback      processingCallback = nullptr;  // called after command execution but before handling completionBehavior
        Response *              response = nullptr;
        uint32_t                responseWaitMs;             // set to 0 if the command issues no response at all (i.e. not even a response byte)
        SendCallback            sendCallback = nullptr;        // called immediately before i2c.write()
        bool                    shouldFreeCompletionContext = false;
        TaskHandle_t            taskToWake = nullptr;

#if ENABLE_ATLAS_SIMULATOR
        using ResponseSimulator = err_t (*)(AtlasSensor *sensor, uint8_t *buffer, size_t bufferSize);

        ResponseSimulator       responseSimulator = nullptr;
#endif
    };

    virtual double              convertReadingResponseToDouble(char *response);
    virtual void                enqueueCommand(Command *node);
    err_t                       enqueueSendGetReading();
    virtual void                handleReading(Response &response);
    virtual err_t               init(const char *name, uint8_t i2cSlaveAddress, DispatchTask *task, bool deferEnqueueSendGetReading);
    void                        lock() { recursiveLock.lock(); }
    template<typename T> err_t  makeAndSendCommand(bool synchronous, const char *format, void *completionContext, CommandCallback completionCallback, const char *responsePrefix = nullptr, uint32_t responseWaitMs = defaultResponseWaitMs, Priority priority = Priority::defaultPriority, CompletionBehavior completionBehavior = CompletionBehavior::dequeue, ...);
    template<typename T> err_t  makeCommand(Command *&command, const char *format, void *completionContext, CommandCallback completionCallback, const char *responsePrefix = nullptr, uint32_t responseWaitMs = defaultResponseWaitMs, Priority priority = Priority::defaultPriority, CompletionBehavior completionBehavior = CompletionBehavior::dequeue, ...);
    template<typename T> err_t  makeCommand(Command *&command, const char *format, va_list args, void *completionContext, CommandCallback completionCallback, const char *responsePrefix, uint32_t responseWaitMs, Priority priority, CompletionBehavior completionBehavior);
    virtual err_t               send(bool synchronous);
    virtual err_t               sendGetReading(bool synchronous, void *context, CommandCallback callback, Priority priority, CompletionBehavior completionBehavior);
    void                        unlock() { recursiveLock.unlock(); }

    int                         firmwareMajorVersion = 0;
    int                         firmwareMinorVersion = 0;
    bool                        isDumpResponseBufferEnabled = false;
    bool                        isLogSentCommandsEnabled = false;

#if ENABLE_ATLAS_SIMULATOR
    int                         calibrationValue = 0;
#endif

    static const uint32_t       defaultResponseWaitMs = 300;

private:

    void                        handleEvent(DispatchEventSource *source);
    err_t                       readResponse(Command *command, char *responseBuffer, size_t responseBufferSize);

    static void                 eventHandler(void *context, DispatchEventSource *source);

    Command *                   commands = nullptr;
    double                      forcedValue = 0;
    I2C::DeviceHandle           i2cDevice = nullptr;
    bool                        isForcedValue = false;
    bool                        isGetReadingActive = false;
    bool                        isStopped = false;
    Reading                     lastReading;
    Command *                   pendingCommand = nullptr;
    RecursiveLock               recursiveLock;
    DispatchTimerSource *       timer = nullptr;

    static I2C &                i2c;
};

template<typename T> err_t AtlasSensor::makeAndSendCommand(bool synchronous, const char *format, void *completionContext, CommandCallback completionCallback, const char *responsePrefix, uint32_t responseWaitMs, Priority priority, CompletionBehavior completionBehavior, ...) {
    va_list args;
    Command *command = nullptr;

    va_start(args, completionBehavior);
    err_t err = makeCommand<T>(command, format, args, completionContext, completionCallback, responsePrefix, responseWaitMs, priority, completionBehavior);
    va_end(args);

    if (!err) {
        enqueueCommand(command);
        err = send(synchronous);
    }

    return err;
};

template<typename T> err_t AtlasSensor::makeCommand(Command *&command, const char *format, void *completionContext, CommandCallback completionCallback, const char *responsePrefix, uint32_t responseWaitMs, Priority priority, CompletionBehavior completionBehavior, ...) {
    va_list args;

    va_start(args, completionBehavior);
    err_t err = makeCommand<T>(command, format, args, completionContext, completionCallback, responsePrefix, responseWaitMs, priority, completionBehavior);
    va_end(args);

    return err;
};

template<typename T> err_t AtlasSensor::makeCommand(Command *&command, const char *format, va_list args, void *completionContext, CommandCallback completionCallback, const char *responsePrefix, uint32_t responseWaitMs, Priority priority, CompletionBehavior completionBehavior) {
    err_t err = 0;
    char *commandString = nullptr;
    T *response = nullptr;

    if (isStopped) err = EINTR;
    if (!err && vasprintf(&commandString, format, args) < 0) err = ENOMEM;
    if (!err && (response = new T) == nullptr) err = ENOMEM;
    if (!err && (command = new Command) == nullptr) err = ENOMEM;
    if (!err) {
        command->commandString = commandString;
        command->completionBehavior = completionBehavior;
        command->completionCallback = completionCallback;
        command->completionContext = completionContext;
        command->priority = int(priority);
        command->response = response;
        command->response->responsePrefix = responsePrefix;
        command->responseWaitMs = responseWaitMs;
    } else {
        delete response;
        _free(commandString);
    }

    return err;
};

using AtlasMessage = AtlasSensor::ReadingMessage;
using AtlasReading = AtlasSensor::Reading;
