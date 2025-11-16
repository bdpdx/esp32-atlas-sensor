//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

// Class providing a simple locking mechanism (aka mutex).
//
// Intended use case is for mutually exclusive access of shared resources. For any type
// of signaling, message passing etc. please use a different mechanism (e.g. task
// notifications or event flags).

#include <errno.h>
#include <esp_err.h>

#include "err_t.h"
#include "lock.h"

Lock::Lock() {
    err_t err = 0;

    if ((semaphore = xSemaphoreCreateMutex()) == nullptr) err = ENOMEM;

    // locks are fundamental, assume a failure to obtain one means the system is screwed
    ESP_ERROR_CHECK(err);
}

Lock::~Lock() {
    if (semaphore) vSemaphoreDelete(semaphore);
}

bool Lock::isLockHeldByCurrentTask() {
    return xSemaphoreGetMutexHolder(semaphore) == xTaskGetCurrentTaskHandle();
}

void Lock::lock() {
    while (xSemaphoreTake(semaphore, portMAX_DELAY) == pdFALSE) {}
}

BaseType_t Lock::lock(TickType_t ticksToWait) {
    return xSemaphoreTake(semaphore, ticksToWait);
}

void Lock::unlock() {
    xSemaphoreGive(semaphore);
}
