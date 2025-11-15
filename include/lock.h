#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Lock {

public:

    Lock();
   ~Lock();

    bool                        isLockHeldByCurrentTask();
    void                        lock();
    BaseType_t                  lock(TickType_t ticksToWait);
    void                        unlock();

private:

    SemaphoreHandle_t           semaphore;
    
};
