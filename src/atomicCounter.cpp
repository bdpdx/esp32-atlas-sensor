//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include <sys/errno.h>

#include "atomicCounter.h"

AtomicCounter::AtomicCounter(uint32_t initialValue) {
    atomicUInt32Store(&count, initialValue);
}

AtomicCounter::AtomicCounter(const AtomicCounter &other) {
    atomicUInt32Store(&count, atomicUInt32Load(&other.count));
}

bool AtomicCounter::decrement() {
    uint32_t value = atomicUInt32Load(&count);

    while (value) {
        uint32_t expected = value;

        if (atomicUInt32CompareExchange(&count, &expected, value - 1)) return true;

        value = expected;   // refresh after compare-and-swap failure
    }

    return false;
}

uint32_t AtomicCounter::fetchAndSet(uint32_t newValue) {
    uint32_t oldValue = atomicUInt32Load(&count);

    while (true) {
        uint32_t expected = oldValue;

        if (atomicUInt32CompareExchange(&count, &expected, newValue)) return oldValue;

        oldValue = expected;
    }
}

AtomicCounter &AtomicCounter::operator++() {
    atomicUInt32Increment(&count);
    return *this;
}

AtomicCounter &AtomicCounter::operator--() {
    atomicUInt32Decrement(&count);
    return *this;
}

AtomicCounter &AtomicCounter::operator=(uint32_t newValue) {
    atomicUInt32Store(&count, newValue);
    return *this;
}

AtomicCounter::operator uint32_t() const {
    return atomicUInt32Load(&count);
}
