//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "dispatchTask.h"

void DispatchTask::add(DispatchEventSource *eventSource) {
    sourcesLock.lock();
    sources.append(eventSource);
    sourcesLock.unlock();
}

err_t DispatchTask::init(const char *taskName) {
    return startTask(taskName);
}

void DispatchTask::remove(DispatchEventSource *eventSource) {
    sourcesLock.lock();
    sources.remove(eventSource);
    sourcesLock.unlock();
}

void DispatchTask::run() {
    size_t eventCount, sourcesCount;
    DispatchEventSource *eventSource = nullptr;

    if (!(eventCount = wait(TASK_MAX_WAIT_TICKS))) return;

    do {
        // find and process pending events
        sourcesLock.lock();

        eventSource = nullptr;
        sourcesCount = sources.count();

        sources.iterate([&](DispatchEventSource *source) {
            if (source->eventCount.decrement()) {
                eventSource = source;
                return false;
            }
            return true;
        });

        if (eventSource) {
            eventSource->retain();
            // Move the event source to the end of the sources list
            // to provide fair event processing for other sources.
            sources.splice(sources, eventSource, sourcesCount);
        }

        sourcesLock.unlock();

        if (eventSource) {
            if (eventSource->eventHandler) {
                eventSource->eventHandler(eventSource->context, eventSource);
            }
            eventSource->release();

            --eventCount;
        }
    } while (eventCount > 0 && eventSource);
}

DispatchTask &DispatchTask::shared() {
    static DispatchTask singleton(false);

    return singleton;
}
