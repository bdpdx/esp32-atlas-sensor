//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "condition.h"

err_t Condition::wait(uint32_t timeoutMs) {
    unlock();
    err_t err = Semaphore::wait(timeoutMs);
    lock();

    return err;
}
