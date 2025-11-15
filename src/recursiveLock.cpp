#include <errno.h>
#include <esp_err.h>

#include "err_t.h"
#include "recursiveLock.h"

RecursiveLock::RecursiveLock() {
    err_t err = 0;

    if ((semaphore = xSemaphoreCreateRecursiveMutex()) == nullptr) err = ENOMEM;

    // locks are fundamental, assume a failure to obtain one means the system is screwed
    ESP_ERROR_CHECK(err);
}

RecursiveLock::~RecursiveLock() {
    if (semaphore) vSemaphoreDelete(semaphore);
}

bool RecursiveLock::isLockHeldByCurrentTask() {
    return xSemaphoreGetMutexHolder(semaphore) == xTaskGetCurrentTaskHandle();
}

void RecursiveLock::lock() {
    while (xSemaphoreTakeRecursive(semaphore, portMAX_DELAY) == pdFALSE) {}
}

BaseType_t RecursiveLock::lock(TickType_t ticksToWait) {
    return xSemaphoreTakeRecursive(semaphore, ticksToWait);
}

void RecursiveLock::unlock() {
    xSemaphoreGiveRecursive(semaphore);
}
