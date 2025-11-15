#pragma once

#include <concepts>
#include <utility>

#include "atomicCounter.h"
#include "log.h"

template<typename Derived> class ReferenceCounted {

public:

    // ReferenceCounted instances are constructed with a retain
    // count of 1 and therefore should not be retained by the
    // instantiator. The instantiator is still responsible for
    // a single call to release() at some point.
    ReferenceCounted();

    // Even though retain() and release() modify counter, we
    // mark them const so const subclasses can be used and still
    // be reference counted.
    size_t                      release() const;
    size_t                      retain() const;

protected:

    // NEVER allow a ReferenceCounted object to be created on
    // the stack or used as a direct member of another object.
    // ReferenceCounted objects must always be heap allocated.
    //
    // Subclasses implementing a destructor must ensure it keeps
    // the protected (or private) visibility to enforce this.
    virtual ~ReferenceCounted() = default;

private:

    AtomicCounter               counter;

};

template<typename Derived>
ReferenceCounted<Derived>::ReferenceCounted()
    : counter(1)
{ }

template<typename Derived>
size_t ReferenceCounted<Derived>::release() const {
    size_t value = --const_cast<ReferenceCounted *>(this)->counter;

    if (value == 0) delete this;

    return value;
}

template<typename Derived>
size_t ReferenceCounted<Derived>::retain() const {
    size_t value = ++const_cast<ReferenceCounted *>(this)->counter;

    return value;
}
