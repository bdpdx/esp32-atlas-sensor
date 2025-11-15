#include "common.h"
#include "task.h"

Task::Task(uint32_t stackSize, bool shouldAllocateTaskInSPIRAM) :
    isTaskAllocatedInSPIRAM(shouldAllocateTaskInSPIRAM),
    stackSize(stackSize)
{
#if !CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY
    if (shouldAllocateTaskInSPIRAM) {
        _loge("task allocation in SPIRAM is disabled via sdkconfig");
        isTaskAllocatedInSPIRAM = false;
    }
#endif
}

Task::~Task() {
    _free(taskBuffer);
    if (taskSemaphore) vSemaphoreDelete(taskSemaphore);
    _free(stack);
}

BaseType_t Task::getTaskCreationCoreID() {
    return tskNO_AFFINITY;
}

const char *Task::getTaskName() {
    const char *name = nullptr;

    if (task) name = pcTaskGetName(task);
    if (name == nullptr) name = "";

    return name;
}

UBaseType_t Task::getTaskPriority() {
    return 1;
}

SemaphoreHandle_t Task::getTaskSemaphore() {
    if (!isTaskActive() && !taskSemaphore) taskSemaphore = xSemaphoreCreateBinary();

    return taskSemaphore;
}

uint32_t Task::getTaskStackSize() {
    return stackSize;
}

bool Task::isExecutingOnTask() {
    return xTaskGetCurrentTaskHandle() == task;
}

bool Task::isTaskActive() {
    return !(isTaskFinished && task == nullptr);
}

void IRAM_ATTR Task::notify(bool fromISR) {
    BaseType_t higherPriorityTaskWoken = 0;

    if (taskSemaphore) {
        ++taskSemaphoreCounter;

        if (fromISR) {
            xSemaphoreGiveFromISR(taskSemaphore, &higherPriorityTaskWoken);
        } else {
            xSemaphoreGive(taskSemaphore);
        }
    } else {
        if (fromISR) {
            vTaskNotifyGiveFromISR(task, &higherPriorityTaskWoken);
        } else {
            xTaskNotifyGive(task);
        }
    }
}

void Task::reportMemoryUsage(const char *taskName, bool force, uint32_t stackSize) {
    uint32_t freeHeapSize = esp_get_free_heap_size();
    uint32_t largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint32_t minimumFreeHeapSize = esp_get_minimum_free_heap_size();
    UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(nullptr);
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    char *name = pcTaskGetName(task);
    
    if (force) { 
        logi("%s task %p/%s free heap %lu, largest free heap block %lu, smallest free heap size %lu, smallest free stack %u/%lu", taskName, task, name, freeHeapSize, largestFreeBlock, minimumFreeHeapSize, stackHighWaterMark, stackSize);
    } else if (freeHeapSize < 8192 || minimumFreeHeapSize < 2048 || stackHighWaterMark < 256) {
        loge("%s task %p/%s free heap %lu, largest free heap block %lu, smallest free heap size %lu, smallest free stack %u/%lu", taskName, task, name, freeHeapSize, largestFreeBlock, minimumFreeHeapSize, stackHighWaterMark, stackSize);
    }
}

void Task::reportTaskMemoryUsage() {
    if (isTaskReportingDisabled) return;
    
    reportMemoryUsage(getTaskName(), isTaskReportingForced, stackSize);
}

void Task::run() { }

err_t Task::startTask(const char *name) {
    if (isTaskActive()) return 0;

    err_t err = 0;

    isTaskFinished = true;
    hasRunTaskEntered = false;
    taskEnteredErr = 0;

    _logi("create task '%s', stackSize: %lu, priority: %u, coreId: %d, in SPIRAM: %s", name, stackSize, getTaskPriority(), getTaskCreationCoreID(), isTaskAllocatedInSPIRAM ? "true" : "false");

    BaseType_t coreID = getTaskCreationCoreID();
    UBaseType_t priority = getTaskPriority();
    TaskHandle_t taskHandle = nullptr;

    if (isTaskAllocatedInSPIRAM) {
        if (stack == nullptr && (stack = (StackType_t *) heap_caps_malloc(stackSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_32BIT)) == nullptr) setErr(ENOMEM);
        if (!err && taskBuffer == nullptr && (taskBuffer = (StaticTask_t *) heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT | MALLOC_CAP_32BIT)) == nullptr) setErr(ENOMEM);
        if (!err && xTaskCreateStaticPinnedToCore((void (*)(void *)) taskEntry, name, stackSize, this, priority, stack, taskBuffer, coreID) == nullptr) setErr(EIO);
        if (err) {
            _free(taskBuffer);
            _free(stack);
        }
    } else {
        err = xTaskCreatePinnedToCore((void (*)(void *)) taskEntry, name, stackSize, this, priority, &taskHandle, coreID) == pdPASS ? 0 : ENOMEM;
    }

    if (!err) while (!hasRunTaskEntered) delay(100);
    if (!err) err = taskEnteredErr;

    if (err) loge("failed to create %s task, err: %d", name, err);

    return err;
}

void Task::stopTask(bool blockUntilTaskExits) {
    isTaskFinished = true;
    notify();

    if (blockUntilTaskExits && !isExecutingOnTask()) {
        while (isTaskActive()) vTaskDelay(10);
    }
}

err_t Task::taskEntered() {
    return 0;
}

void Task::taskEntry(Task *task) {
    task->taskRunloop();
}

void Task::taskExited() { }

void Task::taskRunloop() {
    task = xTaskGetCurrentTaskHandle();
    initialTaskStackHighWaterMark = uxTaskGetStackHighWaterMark(nullptr);

    if (!isTaskEntryReportingDisabled) {
        _logi("%s task %p running on core %d, watchdog is %sabled, free stack %u/%lu", getTaskName(), task, xPortGetCoreID(), isTaskWatchdogEnabled ? "en" : "dis", initialTaskStackHighWaterMark, stackSize);
    }

    taskEnteredErr = taskEntered();

    if (!taskEnteredErr) {
        isTaskFinished = false;
        hasRunTaskEntered = true;

        if (isTaskWatchdogEnabled) esp_task_wdt_add(task);

        do {
            run();
            reportTaskMemoryUsage();
            if (isTaskWatchdogEnabled) esp_task_wdt_reset();
        } while (!isTaskFinished);

        if (isTaskWatchdogEnabled) esp_task_wdt_delete(task);

        taskExited();
    } else {
        taskExited();
        hasRunTaskEntered = true;
    }

    task = nullptr;

    vTaskDelete(nullptr);  // https://www.freertos.org/implementing-a-FreeRTOS-task.html
}

uint32_t Task::wait(TickType_t timeout) {
    uint32_t value;

    if (taskSemaphore) {
        for (value = 0;;) {
            // Atomically get the current taskSemaphoreCounter value and
            // clear it to zero.
            value += taskSemaphoreCounter.fetchAndSet(0);

            // Giving the taskSemaphore in notify() is a two step process
            // and therefore timing sensitive. The taskSemaphoreCounter is
            // incremented first, then the binary semaphore is given.
            //
            // In the ideal case that process happens fully some number
            // of times before wait() is called. If that's the case then
            // value reflects the active notify() count and the binary
            // semaphore is ready for one take operation.
            //
            // However, it is possible to get here with a non-zero value
            // but the semaphore hasn't yet been given. If the user specified
            // a timeout of zero (a poll), then the xSemaphoreTake() call
            // below can return immediately and set didTimeout to true even
            // though value is non-zero (which indicates a pending give).
            //
            // Additionally, it is also possible to get here and before
            // xSemaphoreTake() can be called multiple invocations of
            // notify() could occur. In this case even though we're
            // guaranteed to be able to take the semaphore, the value
            // we just read will be short and we don't want to lose
            // events.
            //
            // So we need to be able to handle all three of these cases.

            // Try to take the semaphore.
            bool didTimeout = xSemaphoreTake(taskSemaphore, timeout) == pdFALSE;

            if (didTimeout) {
                // If we timed out and value is zero then we know definitively
                // the semaphore hasn't been given. If the timeout parameter is
                // not equal to portMAX_DELAY then the user doesn't want us to
                // block indefinitely. Consequently, break out of the loop and
                // return 0 to indicate an actual timeout.
                if (value == 0 && timeout != portMAX_DELAY) break;
            } else {
                // If we didn't timeout then we took the semaphore. Now we
                // have to handle the potential timing case above where the
                // value we initially read is short. We determine this by
                // checking the current value of taskSemaphoreCounter. If it's
                // still zero, then we know no calls to notify() were made
                // before we took the semaphore and we can safelyreturn the
                // current value.
                if (taskSemaphoreCounter == 0) break;
            }

            // If we get here we know either:
            //
            // 1) A timeout occurred and the user wants us to block until
            //    notify() is called, or
            // 2) notify() ran (or is running) at least once between our
            //    calls to update value and take the semaphore. In this
            //    case we know a give is imminent or has already happened
            //    and we need to consume it before we can return the correct
            //    notify() count.
            //
            // In either case, run the loop again to consume the notify()
            // event(s).
        }
    } else {
        do {
            value = ulTaskNotifyTake(pdTRUE, timeout);
        } while (!value && timeout == portMAX_DELAY);
    }

    return value;
}
