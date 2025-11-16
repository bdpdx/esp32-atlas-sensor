//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "atomicCounter.h"
#include "err_t.h"

#define TASK_DEFAULT_STACK_SIZE             (8 * 1024)
#define TASK_WATCHDOG_TIMEOUT_SECONDS       120
#define TASK_MAX_WAIT_MS                    (TASK_WATCHDOG_TIMEOUT_SECONDS / 2 * 1000)
#define TASK_MAX_WAIT_TICKS                 pdMS_TO_TICKS(TASK_MAX_WAIT_MS)

class Task {

public:

    // When the task watchdog is enabled (the default), the esp32 will reboot if
    // esp_task_wdt_reset() is not called every TASK_WATCHDOG_TIMEOUT_SECONDS. This is
    // done automatically by taskRunloop(), but special care needs to be taken when calling
    // blocking functions such as wait() to ensure the block period isn't longer than the
    // watchdog timeout.
    //
    // When subclassing it's tempting to set isTaskWatchdogEnabled = false and then wait()
    // indefinitely. However, doing so discards the benefit of the task watchdog monitoring
    // that a task hasn't wandered into the weeds. When implementing run() it's better
    // to return from wait() periodically even if nothing has happened, for example in:
    //
    // uint32_t notificationsReceived;
    //
    // if (!(notificationsReceived = wait(TASK_MAX_WAIT_TICKS))) return;
    //
    // 2024.04.04 bd TODO: defaulting shouldAllocateTaskInSPIRAM to true here may have been a bit
    // ambitious on my part as the ESP32 is really finicky when it comes to what you can and
    // can't do with a stack in SPIRAM. For example, using SPIFFS will crash the app. Perhaps
    // it would be better to default this to false and only allow it explicitly, but I'm not
    // sure what the ramifications are of this across the codebase just yet and a deep dive is
    // needed.
    //
    // 2024.05.26 bd TODO: I've gone ahead and set the shouldAllocateTaskInSPIRAM to false,
    // but I still need to do a pass through the code and allow tasks that can use SPIRAM to
    // do so.
    Task(uint32_t stackSize = TASK_DEFAULT_STACK_SIZE, bool shouldAllocateTaskInSPIRAM = false);
    virtual ~Task();

    virtual BaseType_t          getTaskCreationCoreID();
    virtual UBaseType_t         getTaskPriority();
    virtual uint32_t            getTaskStackSize();
    // startTask() attempts to create a new thread. If it fails, an error is returned.
    // If it succeeds, it busy waits until taskEntered() is called from the new thread.
    //
    // If taskEntered() returns 0 then startTask() returns 0 and the thread is running.
    // The running thread calls run() regularly until stopTask() is called.
    // When stopTask() is called, the thread will eventually call taskExited() and exit.
    //
    // If taskEntered() returns an error, the thread immediately calls taskExited()
    // and exits. startTask() then returns the error returned from taskEntered().
    virtual err_t               startTask(const char *name = nullptr);
    // stopTask() prevents further execution of the runloop and calls notify().
    // taskExited() will be called soon after and the thread will exit.
    // If blockUntilTaskExits is true stopTask() will busy-wait until the
    // task thread exits. If stopTask() is called from the task runloop
    // stopTask() obviously can't block so in that case stopTask() ignores
    // the flag and returns immediately.
    virtual void                stopTask(bool blockUntilTaskExits = false);

    // The default implementation of getTaskCreationCoreID returns no affinity.
    // Override this if you want to force the task to be created on a specific core.
    const char *                getTaskName();
    bool                        isExecutingOnTask();
    bool                        isTaskActive();
    void                        operator=(Task const &) = delete;

    operator                    TaskHandle_t() { return xTaskGetCurrentTaskHandle(); }

    static void                 reportMemoryUsage(const char *taskName = "", bool force = true, uint32_t stackSize = CONFIG_MAIN_TASK_STACK_SIZE);

protected:

    // Task's default notify/wait mechanism uses FreeRTOS task notifications.
    // This is faster than using a semaphore but prohibits the user from
    // using more than one event source via a FreeRTOS QueueSet. Subclasses
    // that need this functionality should call getTaskSemaphore() once
    // prior to startTask(). This will create a binary semaphore to use
    // with notify/wait instead of a task notification. Only use this if
    // you need to also use a QueueSet. Once the task semaphore has been
    // created Task will use it automatically.
    SemaphoreHandle_t           getTaskSemaphore();
    // notify() increments the FreeRTOS task notification counter which will wake wait() if it is
    // waiting. I use this somewhat differently than the FreeRTOS docs which describe using task
    // notifications as either binary or counting semaphores. I treat the notify/wait system like
    // a binary semaphore in that wait() always clears the notification count to zero when it wakes
    // up, but like a counting semaphore in that we pay attention to the number of times notify()
    // is called and return that from wait(). This lets us know how many times an event occurred (such
    // as a hardware trigger on an input pin) without having the overhead of an event queue or having
    // to worry about how timely the operating system is in processing the fact an event occurred.
    void                        notify(bool fromISR = false);
    virtual void                reportTaskMemoryUsage(); 
    // run() is called repeatedly on the task thread.
    // Unless the task watchdog is disabled run()'s execution time must be limited to
    // TASK_WATCHDOG_TIMEOUT_SECONDS.
    //
    // Note here that run() doesn't return an error. This is to separate the functional
    // concern of subclasses having runtime errors from the task. If a Task subclass encounters
    // a fatal runtime error, it should clean itself up, notify the end user, etc., then
    // call stopTask() if necessary.
    virtual void                run() = 0;
    // taskEntered() is called once from the task thread prior to the thread starting the runloop.
    // Returning anything other than zero will bypass the runloop and cause the thread to exit.
    // The default implementation of taskEntered() does nothing, so it is not necessary to call it
    // from subclasses.
    virtual err_t               taskEntered();
    // taskExited() is called once from the task thread after the runloop exits and immediately prior
    // to vTaskDelete().
    virtual void                taskExited();
    // returns the number of times notify() was called since last wait() or 0 on timeout.
    virtual uint32_t            wait(TickType_t timeout = portMAX_DELAY);

    UBaseType_t                 initialTaskStackHighWaterMark = 0;
    bool                        isTaskEntryReportingDisabled = false;
    bool                        isTaskFinished = true;
    bool                        isTaskReportingDisabled = false;
    bool                        isTaskReportingForced = false;
    bool                        isTaskWatchdogEnabled = true;

private:

    // The task runloop calls run() and resets the task watchdog.
    void                        taskRunloop();

    static void                 taskEntry(Task *arg);

    err_t                       taskEnteredErr = 0;
    bool                        hasRunTaskEntered = false;
    bool                        isTaskAllocatedInSPIRAM;
    StackType_t *               stack = nullptr;
    uint32_t                    stackSize;
    TaskHandle_t                task = nullptr;
    StaticTask_t *              taskBuffer = nullptr;
    SemaphoreHandle_t           taskSemaphore = nullptr;
    AtomicCounter               taskSemaphoreCounter;

};
