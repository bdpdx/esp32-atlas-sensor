//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <concepts>
#include <cstddef>
#include <errno.h>
#include <new>

#include "err_t.h"
#include "listBase.h"

// List elements must be deletable
template<typename T>
concept Deletable = requires(T *t) {
    delete t;
};

// MARK: - List

// a List that takes ownership and requires its elements are Deletable
template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
class List : public ListBase<Element, Policy> {

public:

    using Base = ListBase<Element, Policy>;
    using NodePtr = typename Policy::NodePtr;

    /// @param ownsElements
    ///   - true  => List will `delete element;` on remove()/clear()/destructor
    ///   - false => List only unlinks (never frees your objects)
    explicit List(bool ownsElements = false) noexcept : ownsElements(ownsElements) {}
    List(List &&) noexcept = default;
   ~List() noexcept override;

    // For intrusive lists, append() and insert() return void.
    // For non-intrusive lists, they return err_t (0 on success, ENOMEM on allocation failure).
    auto                        append(Element *e) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t>;
    auto                        insert(Element *e, size_t toIndex = 0) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t>;

    void                        clear() noexcept override;
    void                        remove(const Element *e) noexcept;
    void                        splice(size_t fromIndex, size_t toIndex) noexcept;
    void                        splice(List &source, Element *e, size_t toIndex) noexcept;

    List &                      operator=(List &&) noexcept = default;

private:

    bool                        ownsElements;

};

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
List<Element, Policy>::~List() noexcept {
    clear();
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
auto List<Element, Policy>::append(Element *e) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t> {
    if (!e) {
        if constexpr (!Policy::isIntrusive) return 0;
        else return;
    }
    
    NodePtr newNode = Policy::createNode(e);
    NodePtr *p;

    if constexpr (!Policy::isIntrusive) {
        if (!newNode) return ENOMEM;
    }
    
    for (p = &this->head; *p; p = &Policy::next(*p)) ;

    *p = newNode;
    ++this->length;
    
    if constexpr (!Policy::isIntrusive) return 0;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
void List<Element, Policy>::clear() noexcept {
    NodePtr p, q;

    for (p = this->head; p; p = q) {
        if (ownsElements) delete Policy::extract(p);
        q = Policy::next(p);
        Policy::destroy(p);
    }

    this->head = nullptr;
    this->length = 0;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
auto List<Element, Policy>::insert(Element *e, size_t toIndex) noexcept -> std::conditional_t<Policy::isIntrusive, void, err_t> {
    if (!e) {
        if constexpr (!Policy::isIntrusive) return 0;
        else return;
    }

    if (toIndex > this->length) toIndex = this->length;
    
    NodePtr newNode = Policy::createNode(e);
    NodePtr *p;

    if constexpr (!Policy::isIntrusive) {
        if (!newNode) return ENOMEM;
    }
    
    for (p = &this->head; toIndex--; p = &Policy::next(*p)) ;

    Policy::next(newNode) = *p;
    *p = newNode;
    ++this->length;
    
    if constexpr (!Policy::isIntrusive) return 0;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
void List<Element, Policy>::remove(const Element *e) noexcept {
    if (!e) return;

    NodePtr *p, q;

    for (p = &this->head; *p; p = &Policy::next(*p)) {
        if (Policy::extract(*p) == e) {
            q = *p;
            *p = Policy::next(*p);
            if (ownsElements) delete e;
            Policy::destroy(q);
            --this->length;
            return;
        }
    }
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
void List<Element, Policy>::splice(size_t fromIndex, size_t toIndex) noexcept {
    if (fromIndex >= this->length || toIndex > this->length || fromIndex == toIndex) return;
    
    size_t i;
    NodePtr *from = nullptr, *p, q, *to = nullptr;

    for (i = 0, p = &this->head; *p; ++i, p = &Policy::next(*p)) {
        if (i == fromIndex) from = p;
        if (i == toIndex) to = p;

        if (from && to) {
            // remove
            q = *from;
            *from = Policy::next(q);

            // insert
            Policy::next(q) = *to;
            *to = q;

            break;
        }
    }
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element> && Deletable<Element>
void List<Element, Policy>::splice(List &source, Element *e, size_t toIndex) noexcept {
    if (!e || toIndex > this->length) return;

    // Adjust toIndex if moving to the end of the same list
    if (&source == this && this->length && toIndex == this->length) --toIndex;

    source.remove(e);
    insert(e, toIndex);
}

// MARK: - Intrusive policy

template<typename Element>
requires IntrusiveNode<Element>
struct IntrusivePolicy {
    using NodePtr = Element *;

    static NodePtr              createNode(Element *e) noexcept;
    static void                 destroy(NodePtr node) noexcept;
    static Element *            extract(NodePtr node) noexcept;
    static NodePtr &            next(NodePtr node) noexcept;

    static constexpr bool       isIntrusive = true;
};

// MARK: - IntrusivePolicy definitions

template<typename Element>
requires IntrusiveNode<Element>
typename IntrusivePolicy<Element>::NodePtr 
IntrusivePolicy<Element>::createNode(Element *e) noexcept {
    return e;
}

template<typename Element>
requires IntrusiveNode<Element>
void IntrusivePolicy<Element>::destroy(NodePtr node) noexcept { }

template<typename Element>
requires IntrusiveNode<Element>
Element* IntrusivePolicy<Element>::extract(NodePtr node) noexcept {
    return node;
}

template<typename Element>
requires IntrusiveNode<Element>
typename IntrusivePolicy<Element>::NodePtr& 
IntrusivePolicy<Element>::next(NodePtr node) noexcept {
    return node->next;
}

// MARK: - Non-intrusive policy

template<typename Element>
struct NonIntrusivePolicy {
    struct Node;

    using NodePtr = Node *;

    static NodePtr              createNode(Element *e) noexcept;
    static void                 destroy(NodePtr node) noexcept;
    static Element *            extract(NodePtr node) noexcept;
    static NodePtr &            next(NodePtr node) noexcept;

    static constexpr bool       isIntrusive = false;
};

template<typename Element>
struct NonIntrusivePolicy<Element>::Node :
    public ListElement<NonIntrusivePolicy<Element>::Node>
{
    Node(Element *e) noexcept;

    Element *                   element;
};

// MARK: NonIntrusivePolicy definitions

template<typename Element>
NonIntrusivePolicy<Element>::Node::Node(Element *e) noexcept
    : element(e) { }

template<typename Element>
void NonIntrusivePolicy<Element>::destroy(NodePtr node) noexcept {
    delete node;
}

template<typename Element>
Element* NonIntrusivePolicy<Element>::extract(NodePtr node) noexcept {
    return node->element;
}

template<typename Element>
typename NonIntrusivePolicy<Element>::NodePtr 
NonIntrusivePolicy<Element>::createNode(Element *e) noexcept {
    return new (std::nothrow) Node(e);
}

template<typename Element>
typename NonIntrusivePolicy<Element>::NodePtr& 
NonIntrusivePolicy<Element>::next(NodePtr node) noexcept {
    return node->next;
}

// MARK: - Aliases

/**
 * @brief IntrusiveList<Element> — a non-owning, intrusive linked-list
 *
 * IntrusiveList<Element> is an alias for:
 *   List<Element, IntrusivePolicy<Element>>
 *
 * - **Intrusive**: Element must inherit from ListElement<Element>, which
 *   provides an `Element* next;` pointer.
 * - **Non-owning**: by default, `ownsElements==false`, so the list will never
 *   delete your Element*.  You must manage element lifetime yourself.
 * - **Containment**: Elements must never be added to more than one list simultaneously.
 * - **Append**: append() returns void and always succeeds.
 *
 * @tparam Element
 *   - Must satisfy `IntrusiveNode<Element>` (i.e. have a `.next`).
 *     This is met by inheriting from ListElement<T>.
 *
 * @code{.cpp}
 *   struct Object : public ListElement<Object> { int value; };
 *   using ObjectList = IntrusiveList<Object>;
 *
 *   auto *o1 = new Object{.value = 10};
 *   auto *o2 = new Object{.value = 20};
 *
 *   ObjectList list1;
 *   list1.append(o1);
 *   list1.append(o2);
 *
 *   // Safe to remove during iteration:
 *   list1.iterate([&](Object* o) {
 *     if (o->value == 10) list1.remove(o);
 *     return true;
 *   });
 *
 *   // Move the contents into another list:
 *   ObjectList list2 = std::move(list1);
 *   // now list1.size() == 0; list2 contains o2
 *
 *   // When done:
 *   delete o1;  // you must delete your own objects
 *   delete o2;
 * @endcode
 *
 * @note
 *   - Destructor of Element must be public.
 *     If a private destructor is needed declare List as a friend:
 *     friend class List<Object, IntrusivePolicy<Object>>;
 *   - Elements may live on stack or heap, but you must not delete them
 *     while they remain in the list.
 */
template<typename Element>
using IntrusiveList = List<Element, IntrusivePolicy<Element>>;

/**
 * @brief IntrusiveOwningList<Element> — an owning, intrusive linked-list
 *
 * IntrusiveOwningList<Element> inherits from
 *   List<Element, IntrusivePolicy<Element>>
 * with `ownsElements = true` forced in its constructor.
 *
 * - **Intrusive**: Element must inherit from ListElement<Element>.
 * - **Owning**: the list will call `delete element;` on every `remove()`,
 *   `clear()`, or on destruction.
 * - **Containment**: Elements must never be added to more than one list simultaneously.
 * - **Append**: append() returns void and always succeeds.
 *
 * @tparam Element
 *   - Must satisfy `IntrusiveNode<Element>` (i.e. have a `.next`).
 *     This is met by inheriting from ListElement<T>.
 *   - Must be safely deletable.
 *
 * @code{.cpp}
 *   struct Object : public ListElement<Object> {
 *     int value;
 *   };
 *   using OwningList = IntrusiveOwningList<Object>;
 *
 *   auto *o = new Object{.value = 99};
 *   OwningList a;
 *   a.append(o);
 *
 *   // Safe to remove during iteration:
 *   a.iterate([&](Object* o) {
 *     if (o->value == 10) a.remove(o);
 *     return true;
 *   });
 *
 *   // transfer ownership via move assignment:
 *   OwningList b;
 *   b = std::move(a);
 *   // a.size() == 0; b now owns and will delete `o`
 *
 *   // on clear(), remove(), or destruction of b, `delete o;` is called automatically.
 * @endcode
 *
 * @note
 *   - Destructor of Element must be public.
 *     If a private destructor is needed declare List as a friend:
 *     friend class List<Object, IntrusivePolicy<Object>>;
 *   - Do not use stack-allocated Elements here; they will be deleted.
 *   - After move, the source list is empty and safe to destroy.
 */
template<typename Element>
class IntrusiveOwningList : public List<Element, IntrusivePolicy<Element>> {
public:
    IntrusiveOwningList() noexcept
      : List<Element,IntrusivePolicy<Element>>(/*ownsElements=*/true)
    {}
};

/**
 * @brief NonIntrusiveList<Element> — a non-owning, non-intrusive linked-list
 *
 * NonIntrusiveList<Element> is an alias for:
 *   List<Element, NonIntrusivePolicy<Element>>
 *
 * - **Non-intrusive**: your Element need not derive from anything.
 *   A private Node wrapper is allocated for each append/insert.
 * - **Non-owning**: the list never calls `delete Element*`; it only deletes
 *   its internal Node wrappers.
 * - **Containment**: Elements may be added to more than one list simultaneously.
 * - **Append**: append() returns err_t and fails if memory allocation fails.
 *   If successful, 0 is returned, otherwise ENOMEM is returned.
 *
 * @tparam Element
 *   - Must be safely deletable by you; the list will not delete elements.
 *
 * @code{.cpp}
 *   struct Object { int value; };
 *   using NIList = NonIntrusiveList<Object>;
 *
 *   err_t err;
 *   auto *o = new Object{.value = 7};
 *   NIList list1;
 *   err = list1.append(o); // err == 0 indicates success
 *
 *   // the same element can live in multiple lists:
 *   NIList list2;
 *   err = list2.append(o);
 * 
 *   NIList list3 = std::move(list1);
 *   // now list1.size() == 0; list3 contains `o`
 *
 *   // on clear(), remove(), or destruction, only the wrappers are freed.
 *   list2.clear();
 *   list3.clear();
 *   delete o;  // you must delete your element exactly once
 * @endcode
 *
 * @note
 *   - Destructor of Element must be public.
 *     If a private destructor is needed declare List as a friend:
 *     friend class List<Object, NonIntrusivePolicy<Object>>;
 *   - Element lifetime is your responsibility.
 *   - Safe to call `remove(e)` on the current element in `iterate()`.
 */
template<typename Element>
using NonIntrusiveList = List<Element, NonIntrusivePolicy<Element>>;
