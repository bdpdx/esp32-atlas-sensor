#pragma once

#include "listBase.h"
#include "retainable.h"

// MARK: - RetainedPolicy wrapper

// Deallocation calls release() on the element.
// For non-intrusive lists this also deallocates the node.
template<typename Element, typename BasePolicy>
struct RetainedPolicy : BasePolicy {
    using NodePtr = typename BasePolicy::NodePtr;

    static constexpr bool       isIntrusive = BasePolicy::isIntrusive;

    static void                 destroy(NodePtr node) noexcept;
};

template<typename Element, typename BasePolicy>
void RetainedPolicy<Element, BasePolicy>::destroy(NodePtr node) noexcept {
    Element *e = BasePolicy::extract(node);

    if (e) e->release();

    if constexpr (!BasePolicy::isIntrusive) delete node;
}

// MARK: - RetainedList

// RetainedList manages elements solely via retain()/release().
template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
class RetainedList : public ListBase<Element, Policy> {

public:

    using Base = ListBase<Element, Policy>;
    using NodePtr = typename Policy::NodePtr;

    RetainedList() noexcept = default;
    RetainedList(RetainedList &&) noexcept = default;
   ~RetainedList() noexcept override;

    // For intrusive lists, append() and insert() return void.
    // For non-intrusive lists, they return err_t (0 on success, ENOMEM on allocation failure).
    auto                        append(Element *e) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t>;
    auto                        insert(Element *e, size_t toIndex = 0) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t>;

    void                        clear() noexcept override;
    void                        remove(const Element *e) noexcept;
    void                        splice(size_t fromIndex, size_t toIndex) noexcept;
    void                        splice(RetainedList &source, Element *e, size_t toIndex) noexcept;

    RetainedList &              operator=(RetainedList &&) noexcept = default;

};

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
RetainedList<Element, Policy>::~RetainedList() noexcept {
    clear();
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
auto RetainedList<Element, Policy>::append(Element *e) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t> {
    if (!e) {
        if constexpr (!Policy::isIntrusive) return 0;
        else return;
    }

    e->retain();

    NodePtr newNode = Policy::createNode(e);
    NodePtr* p;

    if constexpr (!Policy::isIntrusive) {
        if (!newNode) {
            e->release();
            return ENOMEM;
        }
    }

    for (p = &this->head; *p; p = &Policy::next(*p)) ;

    *p = newNode;
    ++this->length;
    
    if constexpr (!Policy::isIntrusive) return 0;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
void RetainedList<Element, Policy>::clear() noexcept {
    NodePtr p, q;

    for (p = this->head; p; p = q) {
        q = Policy::next(p);
        Policy::destroy(p);
    }

    this->head = nullptr;
    this->length = 0;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
auto RetainedList<Element, Policy>::insert(Element *e, size_t toIndex) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t> {
    if (!e) {
        if constexpr (!Policy::isIntrusive) return 0;
        else return;
    }

    if (toIndex > this->length) toIndex = this->length;

    e->retain();
    
    NodePtr newNode = Policy::createNode(e);
    NodePtr *p;

    if constexpr (!Policy::isIntrusive) {
        if (!newNode) {
            e->release();
            return ENOMEM;
        }
    }
    
    for (p = &this->head; toIndex--; p = &Policy::next(*p)) ;

    Policy::next(newNode) = *p;
    *p = newNode;
    ++this->length;
    
    if constexpr (!Policy::isIntrusive) return 0;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
void RetainedList<Element, Policy>::remove(const Element *e) noexcept {
    if (!e) return;

    NodePtr *p, q;

    for (p = &this->head; *p; p = &Policy::next(*p)) {
        if (Policy::extract(*p) == e) {
            q = *p;
            *p = Policy::next(*p);
            Policy::destroy(q);
            --this->length;
            return;
        }
    }
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
void RetainedList<Element, Policy>::splice(size_t fromIndex, size_t toIndex) noexcept {
    if (fromIndex >= this->length || toIndex > this->length || fromIndex == toIndex) return;
    
    size_t i;
    NodePtr *from = nullptr, *p, q, *to = nullptr;

    for (i = 0, p = &this->head; *p; ++i, p = &Policy::next(*p)) {
        if (i == fromIndex) from = p;
        if (i == toIndex) to = p;

        if (from && to) {
            q = *from;
            *from = Policy::next(q);
            Policy::next(q) = *to;
            *to = q;
            break;
        }
    }
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Retainable<Element>
void RetainedList<Element, Policy>::splice(RetainedList &source, Element *e, size_t toIndex) noexcept {
    if (!e || toIndex > this->length) return;

    // Adjust toIndex if moving to the end of the same list
    if (&source == this && this->length && toIndex == this->length) --toIndex;

    e->retain();

    source.remove(e);
    insert(e, toIndex);

    e->release();
}

// MARK: - Aliases

template<typename Element>
using IntrusiveRetainedPolicy = RetainedPolicy<Element, IntrusivePolicy<Element>>;

template<typename Element>
using NonIntrusiveRetainedPolicy = RetainedPolicy<Element, NonIntrusivePolicy<Element>>;

/**
 * @brief IntrusiveRetainedList<Element> — an intrusive, reference-counted list
 *
 * IntrusiveRetainedList<Element> is an alias for:
 *   RetainedList<Element, IntrusiveRetainedPolicy<Element>>
 *
 * - **Intrusive**: Element must derive from `ListElement<Element>` (provides
 *   a `next` pointer), and thus may only belong to one intrusive list at a time.  
 * - **Reference-counted**: on `append()`/`insert()`, calls `e->retain()`; on
 *   `remove()`/`clear()`, calls `e->release()` (via `RetainedPolicy`)
 * - **Non-owning**: the list never calls `delete e;`, only adjusts its refcount,
 *   however as with any reference-counted object any operation such as remove()
 *   that decrements the retain count to zero will result in immediate deletion
 *   of the object. So be sure not to use a removed object unless you're certain
 *   it is still retained somewhere.
 * - **Containment**: Elements must never be added to more than one list simultaneously.
 * - **Append**: append() returns void and always succeeds.
 *
 * @tparam Element
 *   Must satisfy the `Retainable<Element>` concept (provide
 *   `size_t retain()` and `size_t release()`), and
 *   the `IntrusiveNode<Element>` concept (derive from `ListElement<Element>`).
 *
 * @code{.cpp}
 * struct Object :
 *  public ListElement<Object>,
 *  public ReferenceCounted<Object>
 * {
 *   int value;
 * };
 *
 * using IRList = IntrusiveRetainedList<Object>;
 *
 * auto *o1 = new Object();      // refcount automatically 1
 * auto *o2 = new Object();      // refcount automatically 1
 *
 * IRList a;
 * a.append(o1);                // refcount += 1 -> 2
 * a.append(o2);                // refcount += 1 -> 2
 * 
 * // transfer ownership of o2 to list a:
 * o2->release();               // refcount -= 1 -> 1
 *
 * // Safe to remove inside iterate():
 * a.iterate([&](Object *e) {
 *   if (shouldRemove(e)) {
 *     // calls e->release(), refcount -= 1
 *     // if o2 were removed() here it would be deleted
 *     a.remove(e);
 *   }
 *   return true;
 * });
 *
 * // Transfer nodes & their refcounts to another list:
 * IRList b = std::move(a);
 * // a is now empty; b holds the same elements (no additional retains).
 *
 * b.clear();                  // calls release() on remaining elements
 *                             // o2 (if still in list) sees refcount -> 0 and is deleted
 * 
 * o1->release();              // initial constructor retain must be released to delete o1
 * @endcode
 *
 * @note
 *   - `remove(const Element *e)` takes a `const` pointer because only
 *     `release()` is invoked, leaving all other Element state unchanged.  
 *   - Elements must not be stack-allocated (their destructor may run
 *     with nonzero refcounts).  
 */
template<typename Element>
using IntrusiveRetainedList = RetainedList<Element, IntrusiveRetainedPolicy<Element>>;

/**
 * @brief NonIntrusiveRetainedList<Element> — a non-intrusive, reference-counted list
 *
 * NonIntrusiveRetainedList<Element> is an alias for:
 *   RetainedList<Element, NonIntrusiveRetainedPolicy<Element>>
 *
 * - **Non-intrusive**: each `append()`/`insert()` allocates a small internal
 *   `Node` wrapper (so the same Element can appear in multiple lists).
 * - **Reference-counted**: on `append()`/`insert()`, calls `e->retain()`; on
 *   `remove()`/`clear()`, calls `e->release()` and frees only the wrapper.
 * - **Non-owning**: the list never calls `delete e;`, only adjusts its refcount,
 *   however as with any reference-counted object any operation such as remove()
 *   that decrements the retain count to zero will result in immediate deletion
 *   of the object. So be sure not to use a removed object unless you're certain
 *   it is still retained somewhere.
 * - **Containment**: Elements may be added to more than one list simultaneously.
 * - **Append**: append() returns err_t and fails if memory allocation fails.
 *   If successful, 0 is returned, otherwise ENOMEM is returned.
 *
 * @tparam Element
 *   - Must satisfy the `Retainable<Element>` concept.
 *   - Derive from both ListElement<T> and ReferenceCounted<T>
 *     to meet these requirements.
 *
 * @code{.cpp}
 * struct Object :
 *  public ListElement<Object>,
 *  public ReferenceCounted<Object>
 * {
 *   int value;
 * };
 *
 * using NIRList = NonIntrusiveRetainedList<Object>;
 *
 * auto *o = new Object();      // refcount == 1
 *
 * NIRList a, b;
 * // verify err == 0 after append()
 * err = a.append(o);           // refcount == 2, wrapper A created
 * err = b.append(o);           // refcount == 3, wrapper B created
 *
 * // Move wrappers (but not refcounts) from a to c:
 * NIRList c = std::move(a);
 * // a is empty; c has wrapper A, b still has wrapper B
 *
 * b.remove(o);                // calls release(): refcount -= 1 -> 2
 *
 * c.clear();                  // calls release(): refcount -= 1 -> 1
 *
 * o->release();               // refcount -> 0 (o is deleted)
 * @endcode
 *
 * @note
 *   - `remove(const Element *e)` takes a `const` pointer because only
 *     `release()` is invoked, leaving all other Element state unchanged.  
 *   - Elements must not be stack-allocated (their destructor may run
 *     with nonzero refcounts).
 */
template<typename Element>
using NonIntrusiveRetainedList = RetainedList<Element, NonIntrusiveRetainedPolicy<Element>>;
