#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

template <typename IndexType, typename BinaryFunc>
class KuTabulateOutputIteratorProxy {
public:
    using iterator_category = typename std::iterator_traits<size_t *>::iterator_category;
    KU_DEVICE_HOST KuTabulateOutputIteratorProxy(IndexType index, BinaryFunc fn)
        : m_index(index), m_fn(std::move(fn)) {
    }

    // proxy just provide the operator= to allow assignment
    template <typename T>
    KU_DEVICE_HOST KuTabulateOutputIteratorProxy &operator=(const T &value) {
        m_fn(m_index, value);
        return *this;
    }

private:
    IndexType  m_index;
    BinaryFunc m_fn;
};

template <typename BinaryFunc>
class KuTabulateOutputIterator {
public:
    // using iterator_type = size_t *;
    using function_type = std::decay_t<BinaryFunc>;
    using reference = KuTabulateOutputIteratorProxy<size_t, function_type>;
    using value_type = reference;

    using pointer = value_type *;
    using const_reference = const reference;
    using const_pointer = const pointer;

    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    KU_DEVICE_HOST KuTabulateOutputIterator() = default;

    KU_DEVICE_HOST KuTabulateOutputIterator(const function_type &func) : m_fn(func), m_index(0) {
    }

    KU_DEVICE_HOST KuTabulateOutputIterator(size_t index, const function_type &func)
        : m_fn(func), m_index(index) {
    }

    KU_DEVICE_HOST KuTabulateOutputIterator(const KuTabulateOutputIterator &other)
        : m_fn(other.m_fn), m_index(other.m_index) {
    }

    KU_DEVICE_HOST KuTabulateOutputIterator &operator=(const KuTabulateOutputIterator &other) {
        if (this != &other) {
            m_index = other.m_index;
            m_fn = other.m_fn;
        }
        return *this;
    }

    KU_DEVICE_HOST KuTabulateOutputIterator &operator++() {
        ++m_index;
        return *this;
    }

    KU_DEVICE_HOST KuTabulateOutputIterator operator++(int) {
        KuTabulateOutputIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    KU_DEVICE_HOST KuTabulateOutputIterator &operator--() {
        --m_index;
        return *this;
    }

    KU_DEVICE_HOST KuTabulateOutputIterator operator--(int) {
        KuTabulateOutputIterator tmp = *this;
        --(*this);
        return tmp;
    }

    KU_DEVICE_HOST KuTabulateOutputIterator operator+(difference_type n) const {
        return KuTabulateOutputIterator(m_index + n, m_fn);
    }

    KU_DEVICE_HOST KuTabulateOutputIterator &operator+=(difference_type n) {
        m_index += n;
        return *this;
    }

    KU_DEVICE_HOST KuTabulateOutputIterator operator-(difference_type n) const {
        return KuTabulateOutputIterator(m_index - n, m_fn);
    }

    KU_DEVICE_HOST KuTabulateOutputIterator &operator-=(difference_type n) {
        m_index -= n;
        return *this;
    }

    KU_DEVICE_HOST difference_type operator-(const KuTabulateOutputIterator &other) const {
        return m_index - other.m_index;
    }

    KU_DEVICE_HOST reference operator*() {
        return reference(m_index, m_fn);
    }

    KU_DEVICE_HOST reference operator[](difference_type n) {
        return reference(m_index + n, m_fn);
    }

    KU_DEVICE_HOST bool operator==(const KuTabulateOutputIterator &other) const {
        return m_index == other.m_index;
    }

    KU_DEVICE_HOST bool operator!=(const KuTabulateOutputIterator &other) const {
        return !(*this == other);
    }

private:
    mutable function_type m_fn;
    size_t                m_index{};
};

template <typename BinaryFunc>
KU_DEVICE_HOST auto makeKuTabulateOutputIterator(BinaryFunc fn) {
    return KuTabulateOutputIterator<BinaryFunc>(std::move(fn));
}

} // namespace kuai
