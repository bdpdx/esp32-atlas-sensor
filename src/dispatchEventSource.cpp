#include "dispatchEventSource.h"
#include "dispatchTask.h"

DispatchEventSource::~DispatchEventSource() {
    removeFromDispatchTask();
}

void DispatchEventSource::addToDispatchTask(DispatchTask *task) {
    bool needsRelease = false;

    if (!task) task = &DispatchTask::shared();

    if (dispatchTask) {
        if (dispatchTask == task) return;

        retain();
        needsRelease = true;

        dispatchTask->remove(this);
    }

    task->add(this);

    dispatchTask = task;

    if (needsRelease) release();
}

void DispatchEventSource::clearEvents() {
    eventCount = 0;
}

void DispatchEventSource::dispatchEvent(bool fromISR) {
    ++eventCount;
    dispatchTask->notify(fromISR);
}

void DispatchEventSource::eventCallback(void *context) {
    DispatchEventSource *eventSource = static_cast<DispatchEventSource *>(context);
    eventSource->dispatchEvent();
}

void DispatchEventSource::eventCallbackFromISR(void *context) {
    DispatchEventSource *eventSource = static_cast<DispatchEventSource *>(context);
    eventSource->dispatchEvent(true);
}

err_t DispatchEventSource::init(EventHandler eventHandler, void *context, DispatchTask *task) {
    this->context = context;
    this->eventHandler = eventHandler;

    addToDispatchTask(task);

    return 0;
}

void DispatchEventSource::removeFromDispatchTask() {
    clearEvents();

    if (dispatchTask) {
        dispatchTask->remove(this);
        dispatchTask = nullptr;
    }
}
