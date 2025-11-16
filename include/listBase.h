//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <concepts>
#include <cstddef>

// For intrusive lists, the element must provide a 'next' member.
template<typename Element>
concept IntrusiveNode = requires(Element e) {
    { e.next } -> std::convertible_to<Element *>;
};

// MARK: - Policies

template<typename Policy, typename T>
concept ListNodePolicy = requires(T* element, typename Policy::NodePtr node) {
    // Wrap an element pointer into a node.
    { Policy::createNode(element) } -> std::same_as<typename Policy::NodePtr>;
    // Destroy (deallocate) the node.
    { Policy::destroy(node) } -> std::same_as<void>;
    // Extract the element pointer from the node.
    { Policy::extract(node) } -> std::same_as<T *>;
    // Get a reference to the node’s "next" pointer.
    { Policy::next(node) } -> std::same_as<typename Policy::NodePtr &>;
};

template<typename Element> struct ListElement {
    Element *                   next = nullptr;
};

// MARK: - ListBase
// Common list functionality that does not impose deletion constraints.

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
class ListBase {

public:

    using NodePtr = typename Policy::NodePtr;

    ListBase() noexcept = default;
    // disallow copy-construction since the list owns elements and
    // it would mean ownership of the same elements in two places.
    ListBase(const ListBase &) noexcept = delete;
    ListBase(ListBase &&other) noexcept;
    virtual ~ListBase() noexcept = default;

    virtual void                clear() noexcept = 0;

    // Returns the number of elements in the list.
    size_t                      count() const noexcept;

    // Predicate: (Element *, size_t index) -> bool
    // Iterates elements using the provided predicate.
    // Iteration stops when predicate returns true.
    template<typename Predicate>
    Element *                   find(Predicate predicate) const noexcept;

    // Callback: (Element *e) -> bool
    // Calls callback for each element.
    // Stops if callback returns false.
    template<typename Callback>
    void                        iterate(Callback &&callback) const noexcept;

    // Compare: (Element *a, Element *b) -> bool
    // Sorts the list using the provided comparison function.
    // Compare returns true if a is before b.
    template<typename Compare>
    void                        sort(Compare compare) noexcept;

    // Swaps the contents of this list with another.
    void                        swap(ListBase &other) noexcept;

    // Access element by index.
    Element *                   operator[](size_t index) const noexcept;

    // disallow copy-assignment since the list owns elements and
    // it would mean ownership of the same elements in two places.
    ListBase &                  operator=(ListBase &) noexcept = delete;
    ListBase &                  operator=(ListBase &&other) noexcept;

protected:

    NodePtr                     head = nullptr;
    size_t                      length = 0;

};

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
ListBase<Element, Policy>::ListBase(ListBase &&other) noexcept
    : head(other.head),
      length(other.length)
{
    other.head = nullptr;
    other.length = 0;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
size_t ListBase<Element, Policy>::count() const noexcept {
    return length;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
template<typename Predicate>
Element *ListBase<Element, Policy>::find(Predicate predicate) const noexcept {
    size_t i;
    NodePtr p;

    for (i = 0, p = head; p; ++i, p = Policy::next(p)) {
        Element *e = Policy::extract(p);

        if (predicate(e, i)) return e;
    }

    return nullptr;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
template<typename Callback>
void ListBase<Element, Policy>::iterate(Callback &&callback) const noexcept {
    NodePtr p, q;

    for (p = head; p; p = q) {
        q = Policy::next(p);
        if (!callback(Policy::extract(p))) return;
    }
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
ListBase<Element, Policy> &ListBase<Element, Policy>::operator=(ListBase &&other) noexcept {
    if (this != &other) {
        clear();

        head = other.head;
        length = other.length;

        other.head = nullptr;
        other.length = 0;
    }

    return *this;
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
Element *ListBase<Element, Policy>::operator[](size_t index) const noexcept {
    if (index >= length) return nullptr;

    NodePtr p = head;

    for (p = head; index--; p = Policy::next(p)) ;

    return Policy::extract(p);
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
template<typename Compare>
void ListBase<Element, Policy>::sort(Compare compare) noexcept {
    NodePtr *min, *p, *q, t;

    // selection sort for simplicity
    for (p = &head; *p; p = &Policy::next(*p)) {
        // Find the pointer-to-pointer holding the minimal node.
        min = p;
        
        for (q = &Policy::next(*p); *q; q = &Policy::next(*q)) {
            if (compare(Policy::extract(*q), Policy::extract(*min))) min = q;
        }

        if (min != p) {
            t = *p;
            *p = *min;
            *min = t;
        }
    }
}

template<typename Element, typename Policy>
requires ListNodePolicy<Policy, Element>
void ListBase<Element, Policy>::swap(ListBase &other) noexcept {
    NodePtr h = head;
    head = other.head;
    other.head = h;
    
    size_t l = length;
    length = other.length;
    other.length = l;
}
