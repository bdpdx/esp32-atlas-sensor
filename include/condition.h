#pragma once

#include "lock.h"
#include "semaphore.h"

class Condition : public Lock, public Semaphore {

public:

    Condition() = default;
    Condition(Condition const &) = delete;
   ~Condition() override = default;

    err_t               wait(uint32_t timeoutMs = UINT_MAX) override;

    void                operator=(Condition const &) = delete;

};
