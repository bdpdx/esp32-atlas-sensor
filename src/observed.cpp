//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "observed.h"

Observed::Message::Message(uint32_t tag, UnixTime when) :
    tag(tag), when(when)
{ }

Observed::~Observed() {
    lock.lock();

    size_t i;

    for (i = 0; i < observersCount; ++i) {
        observers[i].release(observers[i].context);
    }   

    _free(observers);

    lock.unlock();
}

void Observed::notifyObservers(Message *message) {
    lock.lock();

    size_t i, n = observersCount;

    if (n == 0) {
        lock.unlock();
        if (message) message->release();
        return;
    }

    Observer cache[n];

    if (n > 0) {
        memcpy(cache, observers, sizeof(Observer) * n);

        for (i = 0; i < n; ++i) {
            cache[i].retain(cache[i].context);
        }
    }

    lock.unlock();

    // call the retained, cached versions' callbacks.
    // this allows the observer to remove and/or release
    // itself from the observers list during the callback.
    for (i = 0; i < n; ++i) {
        // Re-check membership under lock to avoid calling callbacks
        // for observers that were removed after we built the cache.
        bool stillPresent = false;

        lock.lock();
        for (size_t j = 0, m = observersCount; j < m; ++j) {
            if (observers[j].context == cache[i].context && observers[j].callback == cache[i].callback) {
                stillPresent = true;
                break;
            }
        }
        lock.unlock();

        if (stillPresent) {
            cache[i].callback(this, cache[i].context, message);
        }

        cache[i].release(cache[i].context);
    }

    if (message) message->release();
}
