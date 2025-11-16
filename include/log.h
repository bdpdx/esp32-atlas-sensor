//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <esp_log.h>
#include <cJSON.h>

#include "err_t.h"

#define dump(_buffer, _len)         ESP_LOG_BUFFER_HEXDUMP(__FILE_NAME__, _buffer, (uint16_t)(_len), ESP_LOG_INFO)

#define _logd(_format, ...)         ESP_LOGD(__FILE_NAME__, _format, ##__VA_ARGS__)
#define _loge(_format, ...)         ESP_LOGE(__FILE_NAME__, _format, ##__VA_ARGS__)
#define _logi(_format, ...)         ESP_LOGI(__FILE_NAME__, _format, ##__VA_ARGS__)
#define _logv(_format, ...)         ESP_LOGV(__FILE_NAME__, _format, ##__VA_ARGS__)
#define _logw(_format, ...)         ESP_LOGW(__FILE_NAME__, _format, ##__VA_ARGS__)

#ifdef __cplusplus

#include "color.h"
#include "lock.h"

#define logd(_format, ...)          logDebug(_format, ##__VA_ARGS__)    
#define loge(_format, ...)          logError(_format, ##__VA_ARGS__)
#define logi(_format, ...)          logInfo(_format, ##__VA_ARGS__)
#define logv(_format, ...)          logVerbose(_format, ##__VA_ARGS__)
#define logw(_format, ...)          logWarning(_format, ##__VA_ARGS__)

// these logging methods send the log over the wire to AWS in addition to logging to the console
#define logDebug(_format, ...)      do { _logd(_format, ##__VA_ARGS__); Log::shared().log(Log::Level::debug, __FILE_NAME__, _format, ##__VA_ARGS__); } while (0)
#define logError(_format, ...)      do { _loge(_format, ##__VA_ARGS__); Log::shared().log(Log::Level::error, __FILE_NAME__, _format, ##__VA_ARGS__); } while (0)
#define logInfo(_format, ...)       do { _logi(_format, ##__VA_ARGS__); Log::shared().log(Log::Level::info, __FILE_NAME__, _format, ##__VA_ARGS__); } while (0)
#define logVerbose(_format, ...)    do { _logv(_format, ##__VA_ARGS__); Log::shared().log(Log::Level::verbose, __FILE_NAME__, _format, ##__VA_ARGS__); } while (0)
#define logWarning(_format, ...)    do { _logw(_format, ##__VA_ARGS__); Log::shared().log(Log::Level::warning, __FILE_NAME__, _format, ##__VA_ARGS__); } while (0)

#define setErr(_expr)               do { if ((err = (_expr))) logError("error %d at %s():%d", err, __FUNCTION__, __LINE__); } while (0)
#define setErrFmt(_expr, _fmt, ...) do { if ((err = (_expr))) logError("error %d at %s():%d: " _fmt, err, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while (0)
#define setErrMsg(_expr, _msg)      do { if ((err = (_expr))) logError("error %d at %s():%d: %s", err, __FUNCTION__, __LINE__, (_msg)); } while (0)

class Log {

public:

    enum Level { verbose, debug, info, warning, error };

    Log(Log const &) = delete;
   ~Log();

    err_t                       dumpLogFiles();     // dump logs to AWS and delete the files
    err_t                       init();
    // 2020.12.08 bd: a design note here.  I'm abusing log such that levels >= info are published to
    // EventReporter. Anything with a level less than info will be ignored (though it can be output to
    // the console with log[dv]).
    void                        log(Level level, const char *tag, const char *format, ...);
    void                        log(const cJSON *root, Level level = Level::info, const char *tag = __FILE_NAME__);
    void                        operator=(Log const &) = delete;

    static Log &                shared();

private:

    Log() = default;

    cJSON *                     createJSON(Level level, const char *tag, const char *format, ...);
    cJSON *                     createJSON(Level level, const char *tag, const char *format, va_list args);
    void                        log(Level level, const char *tag, const char *format, va_list args);

    char *                      topic = nullptr;

#if ENABLE_LOGGING_TO_SPIFFS
    err_t                       findLogFiles(int &count, int &high, int &low, int &totalLogfilesBytesUsed);
    int                         logHandler(const char *format, va_list args);
    int                         logToSystemLog(const char *format, ...);
    err_t                       openLogFile(bool forceNextHighestLogFileNumber = false);
    err_t                       writeLogFile(const char *message);

    static int                  logOverride(const char *format, va_list args);

    int                         file = -1;
    bool                        isBootupLogFile = true;
    RecursiveLock               lock;
    vprintf_like_t              systemLogHandler = nullptr;
#endif

};

#endif // __cplusplus
