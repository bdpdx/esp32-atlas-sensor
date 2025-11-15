#pragma once

#include "common.h"

class Observed :
    public ReferenceCounted<Observed>
{

public:

    struct Message : public ReferenceCounted<Message> {
        Message(uint32_t tag, UnixTime when = getCurrentTime());

        uint32_t                tag;
        UnixTime                when;
    };

    // Observer callbacks should not block the caller. This is guidance for
    // performance and responsiveness: callbacks are invoked sequentially
    // by notifyObservers(). It is not a strict requirement for correctness.
    typedef void (*ObserverCallback)(Observed *observed, void *context, const Message *message);

    // An observer cannot add itself more than once to an Observed
    // instance. Calling addObserver() when the context is already
    // an observer will result in EALREADY being returned.
    //
    // Warning: Due to the way observers are cached, it's possible
    // for an ObserverCallback to be in flight when removeObserver()
    // is called, or potentially called after removeObserver() returns
    // if the observer is removed from somewhere other than the ObserverCallback.
    // Removing the observer from within the ObserverCallback guarantees the
    // callback will not be called again during the same notify pass.
    template<Retainable T>
    err_t                       addObserver(T *context, ObserverCallback callback);

    bool                        isObserved() const { return observersCount > 0; }

    template<Retainable T>
    void                        removeObserver(T *context);

protected:

    Observed() = default;
    virtual ~Observed();

    // As a convenience to the caller, notifyObservers() releases
    // but does not retain message.
    virtual void                notifyObservers(Message *message);

private:

    struct Observer {
        ObserverCallback        callback;
        void *                  context;
        ReleaseFunc             release;
        RetainFunc              retain;
    };

    Lock                        lock;
    Observer *                  observers = nullptr;
    size_t                      observersCount = 0;

};

template<Retainable T>
err_t Observed::addObserver(T *context, ObserverCallback callback) {
    if (context == nullptr || callback == nullptr) return EINVAL;

    err_t err = 0;

    lock.lock();

    size_t i, n = observersCount;
    Observer *newObservers;

    for (i = 0; i < n; ++i) {
        if (observers[i].context == context) {
            setErr(EALREADY);
            break;
        }
    }

    if (!err) {
        if ((newObservers = (Observer *) realloc(observers, sizeof(Observer) * (n + 1))) == nullptr) setErr(ENOMEM);
        if (!err) {
            context->retain();

            observers = newObservers;

            observers[n].callback = callback;
            observers[n].context = static_cast<void *>(context);
            observers[n].release = [](void *ctx) -> size_t {
                return static_cast<T *>(ctx)->release();
            };
            observers[n].retain = [](void *ctx) -> size_t {
                return static_cast<T *>(ctx)->retain();
            };

            ++observersCount;
        }
    }

    lock.unlock();

    return err;
}

template<Retainable T>
void Observed::removeObserver(T *context) {
    lock.lock();

    bool found = false;
    size_t i, n = observersCount;
    Observer *newObservers;

    for (i = 0; i < n; ++i) {
        if (observers[i].context == context) {
            observers[i].release(observers[i].context);
            found = true;
            break;
        }
    }

    if (found) {
        while (++i < n) observers[i - 1] = observers[i];
        
        if (--observersCount > 0) {
            if ((newObservers = (Observer *) realloc(observers, sizeof(Observer) * observersCount)) != nullptr) {
                observers = newObservers;
            }
        } else {
            _free(observers);
        }
    }

    lock.unlock();
}

using ObservedMessage = Observed::Message;
