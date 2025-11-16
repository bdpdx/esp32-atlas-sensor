//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#include "cj.h"

#include "err_t.h"
#include "log.h"

#define SPIFFS_BASE_PATH            "/a"
#define SPIFFS_BASE_PATH_LENGTH     2
// 29: CONFIG_SPIFFS_OBJ_NAME_LEN - strlen(SPIFFS_BASE_PATH) - strlen("/")
#define SPIFFS_FILENAME_MAX_LENGTH  (CONFIG_SPIFFS_OBJ_NAME_LEN - SPIFFS_BASE_PATH_LENGTH - 1)
#ifndef SPIFFS_PARTITION_LABEL
    // setting the partition label to anything other than nullptr doesn't work
    #define SPIFFS_PARTITION_LABEL  nullptr
#endif
#define SPIFFS_PATH(filename)       (SPIFFS_BASE_PATH "/" filename)
#define SPIFFS_PATH_BUFFER_SIZE     ((SPIFFS_BASE_PATH_LENGTH + 1 + SPIFFS_FILENAME_MAX_LENGTH + sizeof(int) - 1) / sizeof(int) * sizeof(int))
#define SPIFFS_PATH_FORMAT          (SPIFFS_BASE_PATH "/%s")

class Spiffs {

public:

    // if an error is returned directory processing stops and iterateDirectory()
    // returns the error returned by the callback.
    typedef err_t (*DirectoryEntryCallback)(const char *filename, void *context);

    // Note: Spiffs operations cannot be performed from tasks allocated in SPIRAM
    Spiffs(Spiffs const &) = delete;
   ~Spiffs();

    bool                        fileExists(const char *filename);
    FILE *                      fopen(const char *filename, const char *mode);
    size_t                      freeSpace();
    // if getRootDirectory returns noErr then caller must cJSON_Delete(json);
    err_t                       getRootDirectory(cJSON *&json);
    err_t                       init();
    template<typename T> err_t  iterateDirectory(const char *path, T &&callback);
    void                        logFiles();
    int                         open(const char *filename, int flags, mode_t mode = 0);
    // caller must free() result buffer.
    // if nullTerminate is true, *length (if passed) will be strlen(buffer) whereas buffer length is actually strlen(buffer) + 1
    err_t                       read(const char *filename, void *&buffer, int *length = nullptr, bool nullTerminate = true);
    err_t                       readJson(const char *filename, CJ &cj);
    err_t                       size(const char *filename, off_t &size);
    err_t                       unlink(const char *filename);
    // see warning above - if this crashes look to see if task is in SPIRAM
    err_t                       write(const char *filename, const void *buffer, size_t length);
    err_t                       write(const char *filename, CJ &cj);

    static Spiffs &             shared();

private:

    Spiffs() = default;

    err_t                       makePath(const char *filename, char *buffer);
};

template<typename T>
err_t Spiffs::iterateDirectory(const char *path, T &&callback) {
    if (path == nullptr) return EINVAL;

    DIR *dir = nullptr;
    char directory[SPIFFS_PATH_BUFFER_SIZE];
    struct dirent *entry;
    err_t err;
    int n;

    setErr(makePath(path, directory));
    if (!err) {
        n = strlen(directory);
        if (n > 0 && directory[n - 1] == '/') directory[n - 1] = 0;
        if ((dir = opendir(directory)) == nullptr) setErr(errno);
    }

    while (!err && (entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            err = callback(entry->d_name);
        }
    }

    if (dir) closedir(dir);

    return err;
}
