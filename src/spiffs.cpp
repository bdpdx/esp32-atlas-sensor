#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <spiffs.h>

#include "common.h"
#include "esp_spiffs.h"

Spiffs::~Spiffs() {
#if !ELIDE_DESTRUCTORS_FOR_SINGLETONS
    esp_vfs_spiffs_unregister(nullptr);
#endif
}

bool Spiffs::fileExists(const char *filename) {
    err_t err;
    char path[SPIFFS_PATH_BUFFER_SIZE];
    struct stat st;

    err = makePath(filename, path);
    if (!err && stat(path, &st)) err = errno;

    return !err;
}

FILE *Spiffs::fopen(const char *filename, const char *mode) {
    err_t err;
    FILE *fd = nullptr;
    char path[SPIFFS_PATH_BUFFER_SIZE];

    setErr(makePath(filename, path));
    if (!err && (fd = ::fopen(path, mode)) == nullptr) setErr(errno);

    return fd;
}

size_t Spiffs::freeSpace() {
    size_t total = 0;
    size_t used = 0;

    err_t err = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);

    return !err && used < total ? total - used : 0;
}

err_t Spiffs::getRootDirectory(cJSON *&json) {
    cJSON *array = nullptr;
    DIR *dir = nullptr;
    err_t err = 0;
    cJSON *result = nullptr;

    if ((result = cJSON_CreateObject()) == nullptr) setErr(ENOMEM);
    if (!err && (array = cJSON_CreateArray()) == nullptr) setErr(ENOMEM);
    if (!err && (dir = opendir(SPIFFS_BASE_PATH)) == nullptr) setErr(errno);
    if (!err) {
        do {
            struct dirent *entry;
        
            if ((entry = readdir(dir)) == nullptr) break;

            setErr(!cJSON_AddStringToArray(entry->d_name, array));
        } while (!err);

        closedir(dir);
    }
    if (!err) setErr(!cJSON_AddItemToObject(result, "rootDirectory", array));
    if (!err) {
        array = nullptr;
        json = result;
        result = nullptr;
    }

    cJSON_Delete(array);
    cJSON_Delete(result);

    return err;
}

err_t Spiffs::init() {
    // 2020.01.16 bd: @Mike - config.max_files should go in the app configuration
    // 2020.01.16 bd: TODO: does FLASH_IN_PROJECT / SPIFFS_IMAGE_FLASH_IN_PROJECT work with platformio?
    // currently to flash the esp32/data directory I use a custom task in esp32/.vscode/tasks.json called
    // "PlatformIO: Upload App, Filesystem, and Monitor".
    esp_vfs_spiffs_conf_t config = {};

    config.base_path = SPIFFS_BASE_PATH;
    config.format_if_mount_failed = true;

    // minimum files needed:
    // 1 for uploader task
    // 3 for atlas sensor recordables
    // 1 for pressure sensor recordable
    // 1 for flow sensor recordable
    // 1 for log file (if enabled)
    // 1 spare

    config.max_files = 8;
    config.partition_label = SPIFFS_PARTITION_LABEL;

    bool checkSpiffs = false;
    err_t err;
    bool formatSpiffs = false;
    size_t total = 0, used = 0;

    err = esp_vfs_spiffs_register(&config);

#if FORMAT_SPIFFS_PARTITION_ON_BOOT
    #warning SPIFFS partition will be formatted on boot
    formatSpiffs = true;
#endif

    if (!err && !formatSpiffs) {
        err = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);

#if !TARGET_QEMU
        // never check spiffs partition on qemu, it is too slow
        // SPIFFS FAQ: https://github.com/pellepl/spiffs/wiki/FAQ

        if (err || used > total) checkSpiffs = true;
#endif

        if (checkSpiffs) {
            err = esp_spiffs_check(SPIFFS_PARTITION_LABEL);
            if (!err) {
                err = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
                if (!err && used > total) err = E2BIG;
            }
            
            if (err) {
                _loge("SPIFFS check failed, err is %d", err);
                formatSpiffs = true;
                err = 0;
            }
        } else {
            _logi("spiffs partition seems ok, skipping check");
        }
    }

    if (formatSpiffs) {
        _logi("formatting spiffs partition, on larger flash this may take a while...");
        err = esp_spiffs_format(SPIFFS_PARTITION_LABEL);
        _logi("formatted spiffs partition (err %d)", err);

        if (!err) err = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
    }

    if (!err) {
        _logi("spiffs partition size: %u, used: %u", total, used);
        logFiles();
    } else {
        _loge("spiffs partition failed to load, error is %s", esp_err_to_name(err));
    }

    return err;
}

void Spiffs::logFiles() {
    DIR *dir;
    struct dirent *entry;
    err_t err = 0;
    char *entries = nullptr;

    if ((dir = opendir(SPIFFS_BASE_PATH))) {
        while (!err && (entry = readdir(dir))) {
            char *s = nullptr;

            if (asprintf(&s, "%s/%s\n", entries ? entries : "", entry->d_name) < 0) err = ENOMEM;
            if (!err) {
                _free(entries);
                entries = s;
            } else {
                _loge("Spiffs::logFiles() err %d", err);
                _free(s);
            }
        }
        closedir(dir);
    }

    if (!err) {
        if (entries) logi("\n%s", entries);
    } else {
        loge("failed to allocate space for entries");
    }

    _free(entries);
}

err_t Spiffs::makePath(const char *filename, char *path) {
    int length = strlen(filename);

    if (length > SPIFFS_FILENAME_MAX_LENGTH) {
        errno = E2BIG;
        return E2BIG;
    }

    path[0] = SPIFFS_BASE_PATH[0];
    path[1] = SPIFFS_BASE_PATH[1];
    path[2] = '/';

    if (*filename == '/') ++filename;

    strncpy(&path[3], filename, SPIFFS_FILENAME_MAX_LENGTH);

    path[SPIFFS_PATH_BUFFER_SIZE - 1] = 0;

    return 0;
}

int Spiffs::open(const char *filename, int flags, mode_t mode) {
    int fd = -1;
    err_t err;
    char path[SPIFFS_PATH_BUFFER_SIZE];

    err = makePath(filename, path);
    if (!err && (fd = ::open(path, flags, mode)) < 0) setErr(errno);

    return fd;
}

err_t Spiffs::read(const char *filename, void *&buffer, int *length, bool nullTerminate) {
    void *buf = nullptr;
    err_t err;
    int fd = -1;
    ssize_t n = 0;
    char path[SPIFFS_PATH_BUFFER_SIZE];
    struct stat st;

    setErr(makePath(filename, path));

    _logi("reading %s", filename);

    if (!err && stat(path, &st)) setErr(errno);
    if (!err) {
        _logi("%s is %ld bytes, nullTerminate is %d", filename, st.st_size, nullTerminate);

        off_t size = st.st_size + (nullTerminate ? 1 : 0);
        if ((buf = malloc(size)) == nullptr) setErr(ENOMEM);
    }
    if (!err && (fd = ::open(path, O_RDONLY)) < 0) setErr(errno);
    if (!err) {
        if ((n = ::read(fd, buf, st.st_size)) < 0) setErr(errno);
        ::close(fd);
    }
    if (!err && n != st.st_size) setErr(EIO);
    if (!err) {
        _logi("read %s ok, %d bytes", filename, n);

        if (nullTerminate) ((uint8_t *)buf)[n] = 0;
        if (length) *length = n; // if null terminated this is strlen(buf), which is more useful

        buffer = buf;

        if (nullTerminate) {
            _logi("spiffs read file %s:\n%s\nEOF", filename, (const char *) buf);
        }
    } else {
        _free(buf);
        _loge("error reading file %s: %d", filename, err);
    }

    return err;
}

err_t Spiffs::readJson(const char *filename, CJ &json) {
    if (filename == nullptr || !*filename) return EINVAL;

    void *buffer = nullptr;
    cJSON *cjson = nullptr;
    err_t err;

    setErr(read(filename, buffer));
    if (!err && (cjson = cJSON_Parse((const char *) buffer)) == nullptr) {
        setErr(EINVAL);
        if (err) _loge("error parsing json from file %s:\n'%s'", filename, (const char *) buffer);
    }

    _free(buffer);

    if (!err) {
        json.setRoot(cjson, true);
    } else {
        cJSON_Delete(cjson);
    }

    return err;
}

Spiffs &Spiffs::shared() {
    static Spiffs singleton;

    return singleton;
}

err_t Spiffs::size(const char *filename, off_t &size) {
    err_t err;
    char path[SPIFFS_PATH_BUFFER_SIZE];
    struct stat st;

    err = makePath(filename, path);
    if (!err && stat(path, &st)) err = errno;
    if (!err) {
        size = st.st_size;
    } else {
        size = 0;
    }

    return err;
}

err_t Spiffs::unlink(const char *filename) {
    err_t err;
    char path[SPIFFS_PATH_BUFFER_SIZE];

    err = makePath(filename, path);
    if (!err && ::unlink(path)) setErr(errno);

    if (err) _loge("unlink %s failed: %d", path, err);

    return err;
}

err_t Spiffs::write(const char *filename, const void *buffer, size_t length) {
    if (buffer == nullptr || length == 0) return ESP_OK;

    err_t err;
    int fd = -1;
    ssize_t n = 0;

    // makePath isn't necessary here as open() calls it
    if (!err && (fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC)) < 0) setErr(ESP_FAIL);
    if (!err && (n = ::write(fd, buffer, length)) < 0) setErr(errno);
    if (!err && n != length) setErr(ESP_FAIL);
    if (err) {
        loge("failed to write file %s: %s", filename, strerror(errno));
    }

    if (fd >= 0) close(fd);

    return err;
}

err_t Spiffs::write(const char *filename, CJ &cj) {
    char *buffer = nullptr;
    err_t err = 0;

    if ((buffer = cJSON_PrintUnformatted(cj)) == nullptr) setErr(ENOMEM);
    if (!err) setErr(write(filename, buffer, strlen(buffer)));

    _cJSON_free(buffer);

    return err;
}
