//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class RecursiveLock {

public:

    RecursiveLock();
   ~RecursiveLock();

    bool                        isLockHeldByCurrentTask();
    void                        lock();
    BaseType_t                  lock(TickType_t ticksToWait);
    void                        unlock();

private:

    SemaphoreHandle_t           semaphore;
    
};
