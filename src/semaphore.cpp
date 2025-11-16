//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include <errno.h>

#include "semaphore.h"

void Semaphore::signal() {
    xTaskNotifyGive(task);
    task = nullptr;
}

void Semaphore::signalFromISR() {
    if (task == nullptr) return;

    vTaskNotifyGiveFromISR(task, nullptr);
    task = nullptr;
}

err_t Semaphore::wait(uint32_t timeoutMs) {
    uint32_t result;
    TickType_t timeoutTicks = pdMS_TO_TICKS(timeoutMs);

    if (task != nullptr) return EBUSY;

    task = xTaskGetCurrentTaskHandle();
    result = ulTaskNotifyTake(pdTRUE, timeoutTicks);

    return result == pdTRUE ? 0 : ETIMEDOUT;
}
