#include "condition.h"

err_t Condition::wait(uint32_t timeoutMs) {
    unlock();
    err_t err = Semaphore::wait(timeoutMs);
    lock();

    return err;
}
