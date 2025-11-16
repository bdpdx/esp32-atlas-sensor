//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "common.h"
#include "eventReporter.h"
#include "gravityDisplay.h"
#include "spiffs.h"
#include "utility.h"

#define DUMPFILE_FORMAT             "dumpfile-%d.txt"
#define LOGFILE_FORMAT              "logfile-%d.txt"
#define LOGFILE_MAX_SIZE            2048
// using a bunch of data on the spiffs partition barfs wifi and causes other issues,
// see https://esp32.com/viewtopic.php?t=12166 so make 64KB (32 * 2048) of stored log
// files the high water mark.
//
// 2021.01.31 bd TODO: ideally logging to the spiffs partition should be turned off once I
// can figure out why the pico device at the barn keeps going offline.
#define LOGFILES_MAX                32

#define consoleD(format, ...)       logToSystemLog(LOG_BOLD(LOG_COLOR_RED) "! " LOG_COLOR_D format LOG_RESET_COLOR "\n", ##__VA_ARGS__)
#define consoleE(format, ...)       logToSystemLog(LOG_BOLD(LOG_COLOR_RED) "! " LOG_COLOR_E format LOG_RESET_COLOR "\n", ##__VA_ARGS__)
#define consoleI(format, ...)       logToSystemLog(LOG_BOLD(LOG_COLOR_RED) "! " LOG_COLOR_I format LOG_RESET_COLOR "\n", ##__VA_ARGS__)
#define consoleV(format, ...)       logToSystemLog(LOG_BOLD(LOG_COLOR_RED) "! " LOG_COLOR_V format LOG_RESET_COLOR "\n", ##__VA_ARGS__)
#define consoleW(format, ...)       logToSystemLog(LOG_BOLD(LOG_COLOR_RED) "! " LOG_COLOR_W format LOG_RESET_COLOR "\n", ##__VA_ARGS__)

Log::~Log() {
#if !ELIDE_DESTRUCTORS_FOR_SINGLETONS
    _free(topic);
#endif
}

cJSON *Log::createJSON(Level level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);

    cJSON *root = createJSON(level, tag, format, args);

    va_end(args);

    return root;
}

cJSON *Log::createJSON(Level level, const char *tag, const char *format, va_list args) {
    err_t err = 0;
    cJSON *log = nullptr, *root;
    char *message = nullptr;

    if ((root = cJSON_CreateObject()) == nullptr) err = ENOMEM;
    if (!err && (log = cJSON_CreateObject()) == nullptr) err = ENOMEM;
    if (!err) {
        const char *levelString;

        switch (level) {
            case debug:     levelString = "debug";      break;
            case error:     levelString = "error";      break;
            case info:      levelString = "info";       break;
            case verbose:   levelString = "verbose";    break;
            case warning:   levelString = "warning";    break;
            default:        levelString = "unknown";    break;
        }

        if (cJSON_AddStringToObject(log, "level", levelString) == nullptr) err = ENOMEM;
    }
    if (!err && vasprintf(&message, format, args) < 0) err = ENOMEM;
    if (!err && cJSON_AddStringToObject(log, "message", message) == nullptr) err = ENOMEM;

    _free(message);

    if (!err && tag != nullptr && cJSON_AddStringToObject(log, "tag", tag) == nullptr) err = ENOMEM;
    if (!err && cJSON_AddNumberToObject(log, "timestamp", getCurrentTime()) == nullptr) err = ENOMEM;
    if (!err) {
        cJSON_AddItemToObjectCS(root, "log", log);
        log = nullptr;
    }

    cJSON_Delete(log);

    if (err) {
        cJSON_Delete(root);
        root = nullptr;
        _loge("error creating json for log message '%s'", message ? message : "");
    }

    return root;
}

err_t Log::dumpLogFiles() {
    err_t err = 0;

#if ENABLE_LOGGING_TO_SPIFFS
    char *buffer = nullptr;
    off_t bufferSize = 3 * 1024;
    int count = 0, high = 0, low = 0, totalLogfilesBytesUsed = 0;
    char path[SPIFFS_PATH_BUFFER_SIZE];

    if ((buffer = (char *) malloc(bufferSize)) == nullptr) err = ENOMEM;

    lock.lock();

    if (!err) {
        if (file >= 0) {
            close(file);
            file = -1;
        }
        err = findLogFiles(count, high, low, totalLogfilesBytesUsed);
    }
    for (int i = 0; !err && i < count; ++i) {
        char destination[SPIFFS_PATH_BUFFER_SIZE];
        int logFileNumber = i + low;

        snprintf(path, sizeof(path), SPIFFS_PATH(LOGFILE_FORMAT), logFileNumber);
        snprintf(destination, sizeof(destination), SPIFFS_PATH(DUMPFILE_FORMAT), logFileNumber);
        
        if (rename(path, destination) < 0) err = errno;
    }

    lock.unlock();

    if (!err) {
        consoleI("dumping %d log files (" LOGFILE_FORMAT " - " LOGFILE_FORMAT ")", count, low, high);
    }

    bool isFirstMessage = true;

    for (int i = 0; !err && i < count; ++i) {
        int fd = -1;
        int logFileNumber = i + low;
        struct stat st;

        snprintf(path, sizeof(path), SPIFFS_PATH(DUMPFILE_FORMAT), logFileNumber);

        if ((fd = open(path, O_RDONLY)) < 0) err = errno;
        if (!err && fstat(fd, &st)) err = errno;

        while (!err && st.st_size > 0) {
            int bytesToRead = min(st.st_size, bufferSize - 1);
            int bytesRead = read(fd, buffer, bytesToRead);
            cJSON *log = nullptr, *root = nullptr;

            if (bytesRead < 0) err = errno;
            else {
                st.st_size -= bytesRead;

                buffer[bytesRead] = 0;

                if ((root = cJSON_CreateObject()) == nullptr) err = ENOMEM;
            }
            if (!err && (log = cJSON_CreateObject()) == nullptr) err = ENOMEM;
            if (!err && cJSON_AddStringToObject(log, "level", "info") == nullptr) err = ENOMEM;
            if (!err) {
                char message[32];
                const char *suffix;

                if (isFirstMessage) {
                    suffix = " start";
                    isFirstMessage = false;
                } else if (i == count - 1 && st.st_size == 0) {
                    suffix = " end";
                } else {
                    suffix = "";
                }

                snprintf(message, sizeof(message), "log dump%s", suffix);

                if (cJSON_AddStringToObject(log, "message", message) == nullptr) err = ENOMEM;
            }
            if (!err && cJSON_AddStringToObject(log, "dump", buffer) == nullptr) err = ENOMEM;
            if (!err && cJSON_AddNumberToObject(log, "timestamp", getCurrentTime()) == nullptr) err = ENOMEM;
            if (!err) {
                cJSON_AddItemToObjectCS(root, "log", log);
                log = nullptr;
            }

            cJSON_Delete(log);
            cJSON_Delete(root);
        }
        if (fd >= 0) close(fd);

        if (err) consoleE("error %d dumping %s", err, path);
    }

    _free(buffer);

    for (int i = 0; i < count; ++i) {
        int logFileNumber = i + low;

        snprintf(path, sizeof(path), SPIFFS_PATH(DUMPFILE_FORMAT), logFileNumber);

        unlink(path);
    }
#endif

    return err;
}

#if ENABLE_LOGGING_TO_SPIFFS
err_t Log::findLogFiles(int &count, int &high, int &low, int &totalLogfilesBytesUsed) {
    DIR *dir = nullptr;
    err_t err = 0;
    struct dirent *entry = nullptr;
    char path[SPIFFS_PATH_BUFFER_SIZE];

    count = 0;
    high = INT_MIN;
    low = INT_MAX;
    totalLogfilesBytesUsed = 0;

    if ((dir = opendir(SPIFFS_PATH())) == nullptr) err = errno;
    if (!err) {
        while ((entry = readdir(dir)) != nullptr) {
            int logNumber = 0;
            struct stat st;

            if (sscanf(entry->d_name, LOGFILE_FORMAT, &logNumber) != 1) continue;

            snprintf(path, sizeof(path), SPIFFS_PATH_FORMAT, (const char *) entry->d_name);
            
            if (!stat(path, &st)) {
                totalLogfilesBytesUsed += st.st_size;
            }

            ++count;

            if (logNumber < low) low = logNumber;
            if (logNumber > high) high = logNumber;
        }

        closedir(dir);
    }

    return err;
}
#endif

err_t Log::init() {
#if ENABLE_LOGGING_TO_SPIFFS
    _logw("logging to spiffs is enabled");

    systemLogHandler = esp_log_set_vprintf(logOverride);
#endif

    return 0;
}

void Log::log(Level level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);

    log(level, tag, format, args);

    va_end(args);
}

void Log::log(Level level, const char *tag, const char *format, va_list args) {
    if (format == nullptr || level < info) return;

    Color color;

    switch (level) {
        case debug:     color = Color::white;   break;
        case error:     color = Color::red;     break;
        case info:      color = Color::green;   break;
        case verbose:   color = Color::purple;  break;
        case warning:   color = Color::yellow;  break;
        default:        color = Color::white;   break;
    }

    GravityDisplay::shared().vprint(color, format, args);

    err_t err = 0;
    cJSON *root;

    if ((root = createJSON(level, tag, format, args)) == nullptr) err = ENOMEM;
    if (!err) {
        cJSON *log = cJSON_GetObjectItem(root, "log");
        EventReporter::shared().sendLog(log);
    }

    cJSON_Delete(root);
}

void Log::log(const cJSON *root, Level level, const char *tag) {
    err_t err = 0;
    char *message;

    if ((message = cJSON_PrintUnformatted(root)) == nullptr) err = ENOMEM;

    if (!err) log(level, tag, message);

    _cJSON_free(message);
}

#if ENABLE_LOGGING_TO_SPIFFS
int Log::logHandler(const char *format, va_list args) {
    err_t err = 0;
    char *message = nullptr;
    int result;

    if ((result = vasprintf(&message, format, args)) < 0) err = ENOMEM;
    if (!err) {
        if (lock.isLockHeldByCurrentTask()) {
            consoleE("longHandler lock is held by current task");
            errno = EBUSY;
            return -1;
        }

        lock.lock();

        logToSystemLog(message);
        writeLogFile(message);
        free(message);

        lock.unlock();
    } else {
        result = systemLogHandler(format, args);
    }

    return result;
}
#endif

#if ENABLE_LOGGING_TO_SPIFFS
int Log::logOverride(const char *format, va_list args) {
    return Log::shared().logHandler(format, args);
}
#endif


#if ENABLE_LOGGING_TO_SPIFFS
int Log::logToSystemLog(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int result = systemLogHandler(format, args);

    va_end(args);

    return result;
}
#endif

#if ENABLE_LOGGING_TO_SPIFFS
err_t Log::openLogFile(bool forceNextHighestLogFileNumber) {
    if (isBootupLogFile) forceNextHighestLogFileNumber = true;

    lock.lock();

    if (file >= 0) {
        if (!forceNextHighestLogFileNumber) {
            lock.unlock();
            return 0;
        }

        close(file);
        file = -1;
    }

    int count, high, low, totalLogfilesBytesUsed;
    err_t err = 0;
    int logfilesMax = LOGFILES_MAX - (forceNextHighestLogFileNumber ? 1 : 0);
    int maxAllowedLogfilesBytesUsed = LOGFILES_MAX * LOGFILE_MAX_SIZE;
    char path[SPIFFS_PATH_BUFFER_SIZE];
    
    while (!(err = findLogFiles(count, high, low, totalLogfilesBytesUsed))) {
        if (count < logfilesMax && totalLogfilesBytesUsed < maxAllowedLogfilesBytesUsed) break;

        snprintf(path, sizeof(path), SPIFFS_PATH(LOGFILE_FORMAT), low);
        unlink(path);
    }

    if (!err) {
        int logNumber = 0;

        if (count < 1) {
            logNumber = 0;
        } else if (forceNextHighestLogFileNumber) {
            logNumber = high + 1;
        } else {
            off_t size = INT_MAX;
            struct stat st;

            snprintf(path, sizeof(path), SPIFFS_PATH(LOGFILE_FORMAT), high);
            
            if (!stat(path, &st)) size = st.st_size;

            logNumber = size < LOGFILE_MAX_SIZE ? high : high + 1;
        }

        for (;;) {
            snprintf(path, sizeof(path), SPIFFS_PATH(LOGFILE_FORMAT), logNumber);

            if ((file = open(path, O_APPEND | O_CREAT | O_WRONLY)) < 0) err = errno;
            if (!err || count < 1) break;

            snprintf(path, sizeof(path), SPIFFS_PATH(LOGFILE_FORMAT), low);
            unlink(path);

            --count;
            ++low;

            err = 0;
        }
    }
    if (!err) {
        isBootupLogFile = false;
    }

    lock.unlock();

    if (err) consoleE("openLogFile() error %d", err);

    return err;
}
#endif

Log &Log::shared() {
    static Log singleton;

    return singleton;
}

#if ENABLE_LOGGING_TO_SPIFFS
err_t Log::writeLogFile(const char *message) {
    if (message == nullptr) return 0;

    int length = strlen(message);

    if (length < 1) return 0;

    err_t err = openLogFile(false);
    struct stat st;

    if (!err && write(file, message, length) < 0) err = errno;
    if (file >= 0 && !fstat(file, &st) && st.st_size >= LOGFILE_MAX_SIZE) {
        close(file);
        file = -1;
    }

    return err;
}
#endif
