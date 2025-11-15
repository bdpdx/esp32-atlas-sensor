
#include <esp_timer.h>

#include "dispatchEventSource.h"

class DispatchTimerSource final :
    public DispatchEventSource
{

public:

    DispatchTimerSource() = default;

    // To use a DispatchTimerSource:
    //
    // 1. Create a DispatchTimerSource member.
    // 2. Call DispatchTimerSource::init() passing an EventHandler and context.
    //      - via DispatchEventSource::init() this adds the event source to the
    //        shared DispatchTask() and creates a non-ISR based esp_timer.
    //      - When the timer fires DispatchEventSource updates the DispatchTimerSource's
    //        eventCount and calls dispatchTask->notify().
    //      - The DispatchTask, on its runloop, calls the event handler. Since the
    //        event handler is called on the DispatchTask's runloop, do no work in the
    //        event handler. Just notice the event and perform work on another task.
    // 4. Call startOnce() or startPeriodic() to start the timer.

    err_t                       init(EventHandler eventHandler, void *context, const char *timerName = "DispatchTimerSource", DispatchTask *task = nullptr);
    void                        removeFromDispatchTask() override;
    err_t                       startOnce(uint64_t timeoutMicroseconds);
    err_t                       startPeriodic(uint64_t periodMicroseconds);
    void                        stop();

protected:

   ~DispatchTimerSource() override;

private:

    esp_timer_handle_t          timer = nullptr;

};
