//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "atomic.h"

bool atomicUInt32CompareExchange(AtomicUInt32 *object, uint32_t *expected, uint32_t desired) {
    return atomic_compare_exchange_strong_explicit(object, expected, desired, memory_order_relaxed, memory_order_relaxed);
}

void atomicUInt32Decrement(AtomicUInt32 *object) {
    atomic_fetch_sub_explicit(object, 1, memory_order_relaxed);
}

void atomicUInt32Increment(AtomicUInt32 *object) {
    atomic_fetch_add_explicit(object, 1, memory_order_relaxed);
}

uint32_t atomicUInt32Load(const AtomicUInt32 *object) {
    return atomic_load_explicit(object, memory_order_relaxed);
}

void atomicUInt32Store(AtomicUInt32 *object, uint32_t value) {
    atomic_store_explicit(object, value, memory_order_relaxed);
}
