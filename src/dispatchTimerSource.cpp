//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "dispatchTimerSource.h"

DispatchTimerSource::~DispatchTimerSource() {
    if (timer) {
        esp_timer_stop(timer);
        esp_timer_delete(timer);
    }
}

err_t DispatchTimerSource::init(EventHandler eventHandler, void *context, const char *timerName, DispatchTask *task) {
    esp_timer_create_args_t config = {
        .callback = eventCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = timerName,
        .skip_unhandled_events = true
    };

    err_t err = esp_timer_create(&config, &timer);

    if (!err) err = DispatchEventSource::init(eventHandler, context, task);
    if (err && timer) {
        esp_timer_delete(timer);
        timer = nullptr;
    }

    return err;
}

void DispatchTimerSource::removeFromDispatchTask() {
    stop();

    DispatchEventSource::removeFromDispatchTask();
}

err_t DispatchTimerSource::startOnce(uint64_t timeoutMicroseconds) {
    return timer ? esp_timer_start_once(timer, timeoutMicroseconds) : EINVAL;
}

err_t DispatchTimerSource::startPeriodic(uint64_t periodMicroseconds) {
    return timer ? esp_timer_start_periodic(timer, periodMicroseconds) : EINVAL;
}

void DispatchTimerSource::stop() {
    if (timer) esp_timer_stop(timer);
}
