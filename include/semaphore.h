#pragma once

#include <limits.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "err_t.h"

class Semaphore {

public:

    Semaphore() = default;
    Semaphore(Semaphore const &) = delete;

    virtual ~Semaphore() = default;

    void                signal();
    void                signalFromISR();
    virtual err_t       wait(uint32_t timeoutMs = UINT_MAX);

    void                operator=(Semaphore const &) = delete;

private:

    TaskHandle_t        task = nullptr;

};
