//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include "dispatchEventSource.h"
#include "recursiveLock.h"
#include "retainedList.h"
#include "task.h"

// A DispatchTask manages objects that wait for events such as timers.
// Typical usage is to enqueue objects onto the shared instance.
class DispatchTask : public Task {

    friend class                DispatchEventSource;

    using SourceList = IntrusiveRetainedList<DispatchEventSource>;

public:
 
    DispatchTask(bool shouldAllocateTaskInSPIRAM = true) : Task(TASK_DEFAULT_STACK_SIZE, shouldAllocateTaskInSPIRAM) { }

    // the singleton's init() is called in Setup::setupDispatchTask(), which in turn is called very early
    // in the startup sequence.
    err_t                       init(const char *taskName = "DispatchTask");

    // The singleton is not allocated in SPIRAM
    static DispatchTask &       shared();

private:

    void                        add(DispatchEventSource *eventSource);
    void                        run() override;
    void                        remove(DispatchEventSource *eventSource);

    SourceList                  sources;
    RecursiveLock               sourcesLock;

};
