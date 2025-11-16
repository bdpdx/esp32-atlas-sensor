//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include "atomicCounter.h"
#include "list.h"
#include "referenceCounted.h"

class DispatchTask;

// 2024.05.26 bd TODO: break this into producer and consumer classes?
//
// A DispatchEventSource is both a producer and consumer of events. Add the event source (via a subclass) to
// a DispatchTask and then either manually call dispatchEvent() or call one of the of the eventCallback*
// methods when an event occurs. The DispatchTask will call handleEvent(DispatchEventSource *source) from the DispatchTask's runloop
// at the next opportunity, where the DispatchEventSource can consume the event.
//
// This decouples the enqueueing of events (which should be non-blocking) from the handling of events.
// Note that handling an event in handleEvent(DispatchEventSource *source) does block the DispatchTask's runloop, so if the handler
// performs a long running operation it is best to use handleEvent(DispatchEventSource *source) to enqueue an operation onto a
// dedicated handler task.
class DispatchEventSource :
    public ListElement<DispatchEventSource>,
    public ReferenceCounted<DispatchEventSource>
{

    friend class                DispatchTask;

public:

    using EventHandler = void (*)(void *context, DispatchEventSource *source);

    // A DispatchEventSource can only be on one task at a time. If a DispatchEventSource is already
    // on a DispatchTask when addToDispatchTask() is called, the DispatchEventSource will be removed
    // from the old DispatchTask prior to being added to the new one.
    //
    // Passing nullptr for task will add the event source to the shared DispatchTask.
    virtual void                addToDispatchTask(DispatchTask *task = nullptr);
    virtual void                clearEvents();
    virtual void                dispatchEvent(bool fromISR = false);
    // init() calls addToDispatchTask(task) as a convenience.
    err_t                       init(EventHandler eventHandler, void *context, DispatchTask *task = nullptr);
    virtual void                removeFromDispatchTask();

protected:

    DispatchEventSource() = default;
   ~DispatchEventSource() override;

    static void                 eventCallback(void *context);
    static void                 eventCallbackFromISR(void *context);

private:

    void *                      context = nullptr;
    DispatchTask *              dispatchTask = nullptr;
    AtomicCounter               eventCount;
    EventHandler                eventHandler = nullptr;

};
