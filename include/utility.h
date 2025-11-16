//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <stdint.h>

#include <esp_intr_alloc.h>
#include <soc/gpio_num.h>

#include "err_t.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_INTR_FLAG_NONE                  0   // see esp_intr_alloc.h
#define ESP_INTR_FLAG_DEFAULT               ESP_INTR_FLAG_NONE
#define ESP_TIMER_DELTA_SECONDS(now, then)  (double((now) - (then)) / 1000000.0)
#define MICROSECONDS_PER_SECOND             1000000

#if PLATFORM_WROVER_KIT
void blinkError(int err);
#endif

// GPIO-related
err_t configureOutputGPIO(gpio_num_t gpio, bool initialValue);

// time-related
typedef double                              TimeInterval;   // seconds
typedef double                              UnixTime;       // seconds UTC

void delay(uint64_t milliseconds);

// busy wait!
void delayMicroseconds(unsigned int microseconds);

UnixTime getCurrentTime();    // return value is in seconds UTC
char *getLocalTime(char buffer[32], time_t *when = nullptr);
char *getLocalTimeFromUnixTime(char buffer[32], UnixTime when);
// esp32 doesn't support timezone decoding in strptime so iso8601 must be Zulu
err_t getUnixTimeFromISO8601Zulu(const char *iso8601, UnixTime *outTime);
// How long has the system been up?
TimeInterval getUptime(); 
time_t midnightForTime(time_t when);
// 2024.04.16 esp32 doesn't have timegm
time_t timegm(struct tm *t);

void hexify(char byte, char &highChar, char &lowChar);

void logAllStacks();

int modFloor(int a, int n);
double roundToPrecision(double number, int decimalPlaces);

// string utilities
char *mallocStringFromPointers(const char *start, const char *end);
char *stringWithFormat(const char *format, ...);
char *vStringWithFormat(const char *format, va_list args);

void *reallocf(void *ptr, size_t size);

// todo: remove
void waitForOkToProceed();

// misc
void marchShiftRegisters(int numberOfOutputs);


#ifdef __cplusplus
}
#endif
