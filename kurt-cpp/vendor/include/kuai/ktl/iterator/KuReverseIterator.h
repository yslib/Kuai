#pragma once

#include <iterator>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

template <typename Iterator>
class KuReverseIterator {
public:
    using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using difference_type = typename std::iterator_traits<Iterator>::difference_type;
    using pointer = typename std::iterator_traits<Iterator>::pointer;
    using const_pointer = const typename std::iterator_traits<Iterator>::pointer;
    using reference = typename std::iterator_traits<Iterator>::reference;
    using const_reference = const typename std::iterator_traits<Iterator>::reference;

    KU_DEVICE_HOST KuReverseIterator(Iterator iter) : m_current(iter) {
    }

    KU_DEVICE_HOST KuReverseIterator(const KuReverseIterator &other) : m_current(other.m_current) {
    }

    KU_DEVICE_HOST KuReverseIterator &operator++() {
        --m_current;
        return *this;
    }

    KU_DEVICE_HOST KuReverseIterator operator++(int) {
        KuReverseIterator tmp = *this;
        --m_current;
        return tmp;
    }

    KU_DEVICE_HOST KuReverseIterator &operator--() {
        ++m_current;
        return *this;
    }

    KU_DEVICE_HOST KuReverseIterator operator--(int) {
        KuReverseIterator tmp = *this;
        ++m_current;
        return tmp;
    }

    KU_DEVICE_HOST const_reference operator*() const {
        Iterator tmp = m_current;
        return *--tmp; // access prev
    }

    KU_DEVICE_HOST const_reference operator[](difference_type n) const {
        Iterator tmp = m_current - n;
        return *--tmp; // access prev
    }

    KU_DEVICE_HOST pointer operator->() const {
        return &**this;
    }

    KU_DEVICE_HOST KuReverseIterator &operator+=(difference_type n) {
        m_current -= n;
        return *this;
    }

    KU_DEVICE_HOST KuReverseIterator &operator-=(difference_type n) {
        m_current += n;
        return *this;
    }

    KU_DEVICE_HOST difference_type operator-(const KuReverseIterator &other) const {
        return other.m_current - m_current;
    }

    KU_DEVICE_HOST KuReverseIterator operator+(difference_type n) const {
        return KuReverseIterator(m_current - n);
    }

    KU_DEVICE_HOST KuReverseIterator operator-(difference_type n) const {
        return KuReverseIterator(m_current + n);
    }

    KU_DEVICE_HOST bool operator==(const KuReverseIterator &other) const {
        return m_current == other.m_current;
    }

    KU_DEVICE_HOST bool operator!=(const KuReverseIterator &other) const {
        return !(*this == other);
    }

private:
    Iterator m_current;
};

template <typename Iterator>
KU_DEVICE_HOST auto makeReverseIterator(Iterator iter) {
    return KuReverseIterator<Iterator>(iter);
}

} // namespace kuai
