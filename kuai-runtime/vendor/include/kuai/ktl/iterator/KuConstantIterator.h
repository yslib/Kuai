#pragma once

#include <cstddef>
#include <iterator>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

template <typename T>
struct KuConstantIterator {
    using value_type = T;
    using reference = T &;
    using pointer = T *;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;
    using const_reference = const T &;
    using const_pointer = const T *;

    using Index = difference_type;

    KU_DEVICE_HOST KuConstantIterator() = default;

    KU_DEVICE_HOST KuConstantIterator(T value) : m_value(value) {
    }

    KU_DEVICE_HOST KuConstantIterator(T value, Index index) : m_value(value), m_index(index) {
    }

    KU_DEVICE_HOST KuConstantIterator(const KuConstantIterator &other)
        : m_value(other.m_value), m_index(other.m_index) {
    }

    KU_DEVICE_HOST KuConstantIterator &operator=(const KuConstantIterator &other) {
        m_value = other.m_value;
        m_index = other.m_index;
        return *this;
    }
    KU_DEVICE_HOST KuConstantIterator &operator++() {
        ++m_index;
        return *this;
    }
    KU_DEVICE_HOST KuConstantIterator operator++(int) {
        KuConstantIterator temp = *this;
        ++m_index;
        return temp;
    }
    KU_DEVICE_HOST KuConstantIterator &operator--() {
        --m_index;
        return *this;
    }
    KU_DEVICE_HOST KuConstantIterator operator--(int) {
        KuConstantIterator temp = *this;
        --m_index;
        return temp;
    }
    KU_DEVICE_HOST KuConstantIterator &operator+=(difference_type diff) {
        m_index += diff;
        return *this;
    }
    KU_DEVICE_HOST KuConstantIterator &operator-=(difference_type diff) {
        m_index -= diff;
        return *this;
    }
    KU_DEVICE_HOST KuConstantIterator operator+(difference_type offset) const {
        return KuConstantIterator(m_value, m_index + offset);
    }

    KU_DEVICE_HOST KuConstantIterator operator-(difference_type offset) const {
        return KuConstantIterator(m_value, m_index - offset);
    }

    KU_DEVICE_HOST difference_type operator-(const KuConstantIterator &other) const {
        return m_index - other.m_index;
    }

    KU_DEVICE_HOST bool operator==(const KuConstantIterator &other) const {
        return m_index == other.m_index;
    }
    KU_DEVICE_HOST bool operator!=(const KuConstantIterator &other) const {
        return !(*this == other);
    }
    KU_DEVICE_HOST bool operator<(const KuConstantIterator &other) const {
        return m_index < other.m_index;
    }
    KU_DEVICE_HOST bool operator<=(const KuConstantIterator &other) const {
        return m_index <= other.m_index;
    }
    KU_DEVICE_HOST bool operator>(const KuConstantIterator &other) const {
        return m_index > other.m_index;
    }
    KU_DEVICE_HOST bool operator>=(const KuConstantIterator &other) const {
        return m_index >= other.m_index;
    }

    KU_DEVICE_HOST reference operator*() {
        return m_value;
    }

    KU_DEVICE_HOST pointer operator->() {
        return &m_value;
    }

    KU_DEVICE_HOST const_reference operator*() const {
        return m_value;
    }
    KU_DEVICE_HOST const_pointer operator->() const {
        return &m_value;
    }

    KU_DEVICE_HOST reference operator[](difference_type) {
        return m_value;
    }

    KU_DEVICE_HOST const_reference operator[](difference_type) const {
        return m_value;
    }

private:
    T     m_value;
    Index m_index = 0;
};

template <typename T>
KU_DEVICE_HOST auto makeConstantIterator(T value) {
    return KuConstantIterator<T>(value);
}

} // namespace kuai
