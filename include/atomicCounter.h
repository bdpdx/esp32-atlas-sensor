//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include "atomic.h"

// 2025.01.02 bd: I use atomic counters so infrequently I thought I'd
// leave myself a little reminder about how they are used vs. locks
// to protect critical sections of code. A lock is used to wrap critical
// sections of code and prevents lock acquision from competing threads
// while the lock is held. Acquision prevention is accomplished by
// blocking the competing threads. When using a lock, it is imperative
// that no other lock is locked while the lock is held. When this happens
// it often leads to deadlocks. Those deadlocks tend to cause spurious
// reboots when the watchdog timer times out.
//
// An atomic counter prevents race conditions. On a non-atomic counter,
// say an increment operation is desired. Two competing threads could
// each read the value, both getting say 4, and then both could try to
// increment it by 1, and the value could end up 5 depending on the order
// the reads and writes occur. With an atomic counter, both threads
// increment successfully, and the value ends up 6 which is what is
// expected. Nothing is lost.
//
// The advantage of a counter is that it doesn't block any tasks. CPU-
// level blocking may happen, but only for a cycle or two. To use
// an atomic counter as a lock we'll also use it to protect critical
// sections of code. For my current purposes I want to protect
// objects from being deleted while they are in use by multiple
// competing tasks. For this, a retain-release model works very well.
// See the ReferenceCounted class for an example, but essentially
// when a ReferenceCounted object is created, it gets an automatic
// retain count of 1. Because an atomic counter is used, the retain
// and release counts will always be accurate. No race condition
// can occur. As a result, the retained object is implicitly locked
// into existence by the instantiator until the corresponding
// release is called. Any intervening retains essentially put a
// lock on the object for the duration its retainer needs the object.
//
// So while it's not a blocking lock, or even exactly a lock, it
// functions as a locking mechanism. When the final release is called
// that decrements the counter to zero, the ReferenceCounted object
// deletes itself. Since we know no other object is using the
// ReferenceCounted object, we know it's safe to delete it.
//
// So we get the protection of a lock without actually using a
// lock and we don't need to worry about blocking.
//
// Additionally, we're using the AtomicCounter to keep track of
// event occurrences. This usage does not mirror locking functionality,
// rather it is used purely to avoid race conditions so we don't
// lose track of any events.

class AtomicCounter {

public:

    AtomicCounter(uint32_t initialValue = 0);
    AtomicCounter(const AtomicCounter &other);

    // decrement() tries to atomically decrement the counter
    // by one. It returns true if the counter was greater
    // than zero and a decrement actually happened, or false
    // if the counter was or became zero prior to the decrement
    // operation completing.
    bool                        decrement();
    uint32_t                    fetchAndSet(uint32_t newValue);
    AtomicCounter &             operator++();
    AtomicCounter &             operator--();
    AtomicCounter &             operator=(uint32_t newValue);
    operator                    uint32_t() const;

private:

    AtomicUInt32                count;

};
