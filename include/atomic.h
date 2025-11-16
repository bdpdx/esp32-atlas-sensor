//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef _Atomic(uint32_t) AtomicUInt32;

// these operations all use memory_order_relaxed, suitable only for simple counters.
bool atomicUInt32CompareExchange(AtomicUInt32 *object, uint32_t *expected, uint32_t desired);
void atomicUInt32Decrement(AtomicUInt32 *object);
void atomicUInt32Increment(AtomicUInt32 *object);
uint32_t atomicUInt32Load(const AtomicUInt32 *object);
void atomicUInt32Store(AtomicUInt32 *object, uint32_t value);

#ifdef __cplusplus
}
#endif
