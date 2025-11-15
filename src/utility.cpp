#include "common.h"
#include "eventManager.h"
#include "shiftRegister74hc595.h"
#include "utility.h"

#if PLATFORM_WROVER_KIT
void blinkError(int err) {
   gpio_num_t blueLed = LED_BLUE_GPIO;
   gpio_num_t greenLed = LED_GREEN_GPIO;
   gpio_num_t redLed = LED_RED_GPIO;

    int blinkRed = err / 100;
    int blinkGreen = err % 100 / 10;
    int blinkBlue = err % 10;

    while (true) {
        int i;

        gpio_set_level(redLed, 0);
        gpio_set_level(greenLed, 0);
        gpio_set_level(blueLed, 0);

        delay(2000);

        for (i = 0; i < blinkRed; ++i) {
            gpio_set_level(redLed, 1);
            delay(500);
            gpio_set_level(redLed, 0);
            delay(500);
        }

        for (i = 0; i < blinkGreen; ++i) {
            gpio_set_level(greenLed, 1);
            delay(500);
            gpio_set_level(greenLed, 0);
            delay(500);
        }

        for (i = 0; i < blinkBlue; ++i) {
            gpio_set_level(blueLed, 1);
            delay(500);
            gpio_set_level(blueLed, 0);
            delay(500);
        }
    }
}
#endif

err_t configureOutputGPIO(gpio_num_t gpio, bool initialValue) {
    if (gpio == GPIO_NUM_NC) return 0;

    err_t err;

    gpio_set_level(gpio, initialValue);
    err = gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    if (!err) gpio_set_level(gpio, initialValue);

    return err;
}

void delay(uint64_t milliseconds) {
    uint64_t ticks = milliseconds / portTICK_PERIOD_MS;

    while (ticks > portMAX_DELAY) {
        vTaskDelay(portMAX_DELAY);
        ticks -= portMAX_DELAY;
    }

    if (ticks) vTaskDelay(uint32_t(ticks));
}

void delayMicroseconds(unsigned int microseconds) {
    uint64_t start = (uint64_t) esp_timer_get_time();
    if (microseconds) {
        while ((uint64_t) esp_timer_get_time() - start < (uint64_t) microseconds) ;
    }
}

UnixTime getCurrentTime() {
    double now = 0;
    struct timeval tv = {};

    if (!gettimeofday(&tv, nullptr)) {
        now = double(tv.tv_sec);
        now += double(tv.tv_usec) / 1000000.0;
    }

    return now;
}

char *getLocalTimeFromUnixTime(char buffer[32], UnixTime when) {
    time_t time = time_t(when);

    return getLocalTime(buffer, &time);
}

char *getLocalTime(char buffer[32], time_t *when) {
    time_t now;
    struct tm timeInfo = {};

    if (when) {
        now = *when;
    } else {
        now = 0;
        time(&now);
    }

    localtime_r(&now, &timeInfo);
    asctime_r(&timeInfo, buffer);

    for (int i = 32; --i >= 0;) {
        if (buffer[i] == '\n') {
            buffer[i] = 0;
            break;
        }
    }

    return buffer;
}

err_t getUnixTimeFromISO8601Zulu(const char *iso8601, UnixTime *outTime) {
    if (!iso8601) return EINVAL;

    err_t err = 0;
    // 2024.04.16 bd: ideally "%FT%T%z" but esp32 doesn't currently support %z or %Z
    // see https://github.com/espressif/esp-idf/issues/2219
    const char *format = "%FT%T";
    struct tm timeInfo = {};

    if (!strptime(iso8601, format, &timeInfo)) err = EINVAL;
    if (!err && outTime) *outTime = UnixTime(timegm(&timeInfo));

    if (err) loge("error %d converting iso8601 time \"%s\"", err, iso8601);

    return err;
}

TimeInterval getUptime() {
    double result = double(esp_timer_get_time()) / double(MICROSECONDS_PER_SECOND);
    return result;
}

void hexify(char byte, char &highChar, char &lowChar) {
    unsigned char b = (unsigned char) byte;
    unsigned char h = (b >> 4) & 0xf;
    unsigned char l = b & 0xf;

    h = (unsigned char)(h + (h < 10 ? '0' : 'a' - 10));
    l = (unsigned char)(l + (l < 10 ? '0' : 'a' - 10));

    highChar = h;
    lowChar = l;
}

void logAllStacks() {
    char *json = esp_backtrace_create_json_for_all_tasks(1024);

    if (json) {
        logi("tasks json:\n%s\n", json);
        free(json);
    } else {
        loge("failed to get tasks backtraces");
    }
}

time_t midnightForTime(time_t when) {
    struct tm localTime;

    localtime_r(&when, &localTime);
    localTime.tm_hour = 0;
    localTime.tm_min = 0;
    localTime.tm_sec = 0;

    time_t midnight = mktime(&localTime);

    return midnight;
}

// thread-safe timegm() per https://stackoverflow.com/a/11324281/312594
// struct tm to seconds since Unix epoch
time_t timegm(struct tm * t) {
    const long monthsPerYear = 12;
    time_t result;
    long year;

    static const int cumulativeDays[monthsPerYear] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    year = 1900 + t->tm_year + t->tm_mon / monthsPerYear;
    result = (year - 1970) * 365 + cumulativeDays[t->tm_mon % monthsPerYear];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;

    if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) && (t->tm_mon % monthsPerYear) < 2) {
        --result;
    }

    result += t->tm_mday - 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;

    if (t->tm_isdst == 1) result -= 3600;

    return result;
}

int modFloor(int a, int n) {
    return ((a % n) + n) % n;
}

void marchShiftRegisters(int numberOfOutputs) {
    ShiftRegister74HC595 sr = ShiftRegister74HC595();
    sr.init();
    int numberOfBytes = numberOfOutputs / 8;
    uint8_t data[numberOfBytes] = {};

    sr.set(data, numberOfOutputs);
    sr.enable();

    for (int i = 0; i < numberOfBytes; ++i) {
        for (int j = 0; j < 8; ++j) {
            logi("enable sr %d", i * 8 + j + 1);
            data[i] = 0x80 >> j;
            sr.set(data, numberOfOutputs);
            delay(500);
        }
        data[i] = 0;
    }

    sr.set(data, numberOfOutputs);
}

char *mallocStringFromPointers(const char *start, const char *end) {
    int length = end - start;
    char *p;

    if (length < 0) return nullptr;

    if ((p = (char *) malloc(length + 1)) == nullptr) return nullptr;

    if (length) memcpy(p, start, length);

    p[length] = 0;

    return p;
}

void *reallocf(void *ptr, size_t size) {
    void *result;
    
    if ((result = realloc(ptr, size)) == nullptr) {
        if (ptr) free(ptr);
    }

    return result;
}

double roundToPrecision(double number, int decimalPlaces) {
    double mod = pow(10.0, decimalPlaces);
    double result = round(number * mod) / mod;

    return result;
}

char *stringWithFormat(const char *format, ...) {
    va_list args;
    char *string;

    va_start(args, format);
    string = vStringWithFormat(format, args);
    va_end(args);

    return string;
}

char *vStringWithFormat(const char *format, va_list args) {
    char *string = nullptr;

    va_list tmp;
    va_copy(tmp, args);
    if (vasprintf(&string, format, args) < 0) string = nullptr;
    va_end(tmp);

    return string;
}

void waitForOkToProceed() {
    EventManager &eventManager = EventManager::shared();

    logi("waiting for ok to proceed");
    eventManager.waitForOkToProceed();
    eventManager.clearEvent(EventManager::Event::okToProceed);
    logi("received ok to proceed, continuing");
}
