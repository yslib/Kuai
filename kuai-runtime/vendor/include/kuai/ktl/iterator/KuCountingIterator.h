#pragma once

#include <cstddef>
#include <iterator>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

template <typename T>
struct KuCountingIterator {
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference =
        T; // for the usage in generic algorithms, the refence type should be the same as
           // value_type. consider a temporary iterator in an expression, *(iter + i)
    using const_reference = const T;
    using const_pointer = const T *;
    using iterator_category = std::random_access_iterator_tag;
    using iterator = KuCountingIterator<T>;

    KU_DEVICE_HOST KuCountingIterator() = default;

    KU_DEVICE_HOST KuCountingIterator(T value) : m_value(value) {
    }

    KU_DEVICE_HOST KuCountingIterator(const KuCountingIterator &other) : m_value(other.m_value) {
    }

    KU_DEVICE_HOST KuCountingIterator &operator++() {
        ++m_value;
        return *this;
    }

    KU_DEVICE_HOST KuCountingIterator operator++(int) {
        KuCountingIterator temp = *this;
        ++(*this);
        return temp;
    }

    KU_DEVICE_HOST KuCountingIterator &operator--() {
        --m_value;
        return *this;
    }

    KU_DEVICE_HOST KuCountingIterator operator--(int) {
        KuCountingIterator temp = *this;
        --(*this);
        return temp;
    }
    KU_DEVICE_HOST KuCountingIterator &operator+=(difference_type n) {
        m_value += n;
        return *this;
    }
    KU_DEVICE_HOST KuCountingIterator &operator-=(difference_type n) {
        m_value -= n;
        return *this;
    }
    KU_DEVICE_HOST KuCountingIterator operator+(difference_type n) const {
        return KuCountingIterator(m_value + n);
    }
    KU_DEVICE_HOST KuCountingIterator operator-(difference_type n) const {
        return KuCountingIterator(m_value - n);
    }

    KU_DEVICE_HOST reference operator*() {
        return m_value;
    }

    KU_DEVICE_HOST const_reference operator*() const {
        return m_value;
    }

    KU_DEVICE_HOST pointer operator->() {
        return &m_value;
    }
    KU_DEVICE_HOST const_pointer operator->() const {
        return &m_value;
    }

    KU_DEVICE_HOST difference_type operator-(const KuCountingIterator &other) const {
        return m_value - other.m_value;
    }

    KU_DEVICE_HOST bool operator==(const KuCountingIterator &other) const {
        return m_value == other.m_value;
    }
    KU_DEVICE_HOST bool operator!=(const KuCountingIterator &other) const {
        return !(*this == other);
    }
    KU_DEVICE_HOST bool operator<(const KuCountingIterator &other) const {
        return m_value < other.m_value;
    }
    KU_DEVICE_HOST bool operator<=(const KuCountingIterator &other) const {
        return m_value <= other.m_value;
    }
    KU_DEVICE_HOST bool operator>(const KuCountingIterator &other) const {
        return m_value > other.m_value;
    }
    KU_DEVICE_HOST bool operator>=(const KuCountingIterator &other) const {
        return m_value >= other.m_value;
    }

    KU_DEVICE_HOST reference operator[](difference_type n) {
        return m_value + n;
    }

    KU_DEVICE_HOST const_reference operator[](difference_type n) const {
        return m_value + n;
    }
    KU_DEVICE_HOST KuCountingIterator<T> &operator=(const KuCountingIterator<T> &other) {
        m_value = other.m_value;
        return *this;
    }

    KU_DEVICE_HOST KuCountingIterator<T> &operator=(KuCountingIterator<T> &&other) noexcept {
        m_value = std::move(other.m_value);
        return *this;
    }

    KU_DEVICE_HOST KuCountingIterator<T> &operator=(T newValue) {
        m_value = newValue;
        return *this;
    }

private:
    T m_value;
};

template <typename Value>
KU_DEVICE_HOST auto makeCountingIterator(Value value) {
    return KuCountingIterator<Value>(value);
}

} // namespace kuai
