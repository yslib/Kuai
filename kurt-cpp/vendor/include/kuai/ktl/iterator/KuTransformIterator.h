#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

template <typename Iterator, typename UnaryFunction>
class KuTransformIterator {
public:
    using iterator_type = Iterator;
    using function_type = std::decay_t<UnaryFunction>;
    using source_reference = typename std::iterator_traits<Iterator>::reference;
    using reference = std::invoke_result_t<function_type &, source_reference>;
    using value_type = std::remove_cvref_t<reference>;
    using const_reference = reference;

    using pointer = value_type *;
    using const_pointer = const value_type *;
    using difference_type = typename std::iterator_traits<Iterator>::difference_type;
    using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;

    KU_DEVICE_HOST KuTransformIterator() = default;

    KU_DEVICE_HOST KuTransformIterator(Iterator it, const function_type &func)
        : m_iterator(it), m_fn(func) {
    }

    KU_DEVICE_HOST KuTransformIterator(const KuTransformIterator &other)
        : m_iterator(other.m_iterator), m_fn(other.m_fn) {
    }

    KU_DEVICE_HOST KuTransformIterator &operator=(const KuTransformIterator &other) {
        if (this != &other) {
            m_iterator = other.m_iterator;
            m_fn = other.m_fn;
        }
        return *this;
    }

    KU_DEVICE_HOST reference operator*() const {
        return m_fn(*m_iterator);
    }

    KU_DEVICE_HOST KuTransformIterator &operator++() {
        ++m_iterator;
        return *this;
    }

    KU_DEVICE_HOST KuTransformIterator operator++(int) {
        KuTransformIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    KU_DEVICE_HOST KuTransformIterator &operator--() {
        --m_iterator;
        return *this;
    }

    KU_DEVICE_HOST KuTransformIterator operator--(int) {
        KuTransformIterator tmp = *this;
        --(*this);
        return tmp;
    }

    KU_DEVICE_HOST KuTransformIterator operator+(difference_type n) const {
        return KuTransformIterator(m_iterator + n, m_fn);
    }

    KU_DEVICE_HOST KuTransformIterator &operator+=(difference_type n) {
        m_iterator += n;
        return *this;
    }

    KU_DEVICE_HOST KuTransformIterator operator-(difference_type n) const {
        return KuTransformIterator(m_iterator - n, m_fn);
    }

    KU_DEVICE_HOST KuTransformIterator &operator-=(difference_type n) {
        m_iterator -= n;
        return *this;
    }

    KU_DEVICE_HOST reference operator[](difference_type n) const {
        return m_fn(m_iterator[n]);
    }

    KU_DEVICE_HOST difference_type operator-(const KuTransformIterator &other) const {
        return m_iterator - other.m_iterator;
    }

    KU_DEVICE_HOST bool operator==(const KuTransformIterator &other) const {
        return m_iterator == other.m_iterator;
    }

    KU_DEVICE_HOST bool operator!=(const KuTransformIterator &other) const {
        return !(*this == other);
    }

private:
    Iterator              m_iterator;
    mutable function_type m_fn;
};

template <typename Iterator, typename UnaryFunc, typename... UnaryFuncs>
KU_DEVICE_HOST auto makeTransformIteratorImpl(Iterator it, UnaryFunc &&fn, UnaryFuncs &&...func) {
    if constexpr (sizeof...(func) == 0) {
        return KuTransformIterator<Iterator, UnaryFunc>(it, std::forward<UnaryFunc>(fn));
    } else {
        return makeTransformIteratorImpl(
            KuTransformIterator<Iterator, UnaryFunc>(it, std::forward<UnaryFunc>(fn)),
            std::forward<UnaryFuncs>(func)...);
    }
}

template <typename Iterator, typename... UnaryFunction>
KU_DEVICE_HOST auto makeTransformIterator(Iterator it, UnaryFunction &&...func) {
    return makeTransformIteratorImpl(it, std::forward<UnaryFunction>(func)...);
}
} // namespace kuai
