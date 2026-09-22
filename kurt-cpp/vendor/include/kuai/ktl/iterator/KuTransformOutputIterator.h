#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

template <typename OutputIterator, typename UnaryFunction>
class KuTransformOutputIteratorProxy {
public:
    using iterator_category = typename std::iterator_traits<OutputIterator>::iterator_category;
    KU_DEVICE_HOST KuTransformOutputIteratorProxy(OutputIterator it, UnaryFunction fn)
        : m_iterator(it), m_fn(std::move(fn)) {
    }

    // proxy just provide the operator= to allow assignment
    template <typename T>
    KU_DEVICE_HOST KuTransformOutputIteratorProxy &operator=(const T &value) {
        *m_iterator = m_fn(value);
        return *this;
    }

private:
    OutputIterator m_iterator;
    UnaryFunction  m_fn;
};

template <typename OutputIterator, typename UnaryFunction>
class KuTransformOutputIterator {
public:
    using iterator_type = OutputIterator;
    using function_type = std::decay_t<UnaryFunction>;
    using reference = KuTransformOutputIteratorProxy<OutputIterator, function_type>;

    using difference_type = typename std::iterator_traits<OutputIterator>::difference_type;
    using iterator_category = typename std::iterator_traits<OutputIterator>::iterator_category;

    KU_DEVICE_HOST KuTransformOutputIterator() = default;

    KU_DEVICE_HOST KuTransformOutputIterator(OutputIterator it, const function_type &func)
        : m_iterator(it), m_fn(func) {
    }

    KU_DEVICE_HOST KuTransformOutputIterator(const KuTransformOutputIterator &other)
        : m_iterator(other.m_iterator), m_fn(other.m_fn) {
    }

    KU_DEVICE_HOST KuTransformOutputIterator &operator=(const KuTransformOutputIterator &other) {
        if (this != &other) {
            m_iterator = other.m_iterator;
            m_fn = other.m_fn;
        }
        return *this;
    }

    KU_DEVICE_HOST KuTransformOutputIterator &operator++() {
        ++m_iterator;
        return *this;
    }

    KU_DEVICE_HOST KuTransformOutputIterator operator++(int) {
        KuTransformOutputIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    KU_DEVICE_HOST KuTransformOutputIterator &operator--() {
        --m_iterator;
        return *this;
    }

    KU_DEVICE_HOST KuTransformOutputIterator operator--(int) {
        KuTransformOutputIterator tmp = *this;
        --(*this);
        return tmp;
    }

    KU_DEVICE_HOST KuTransformOutputIterator operator+(difference_type n) const {
        return KuTransformOutputIterator(m_iterator + n, m_fn);
    }

    KU_DEVICE_HOST KuTransformOutputIterator &operator+=(difference_type n) {
        m_iterator += n;
        return *this;
    }

    KU_DEVICE_HOST KuTransformOutputIterator operator-(difference_type n) const {
        return KuTransformOutputIterator(m_iterator - n, m_fn);
    }

    KU_DEVICE_HOST KuTransformOutputIterator &operator-=(difference_type n) {
        m_iterator -= n;
        return *this;
    }

    KU_DEVICE_HOST difference_type operator-(const KuTransformOutputIterator &other) const {
        return m_iterator - other.m_iterator;
    }

    KU_DEVICE_HOST reference operator*() {
        return reference(m_iterator, m_fn);
    }

    KU_DEVICE_HOST reference operator[](difference_type n) {
        return reference(m_iterator + n, m_fn);
    }

    KU_DEVICE_HOST bool operator==(const KuTransformOutputIterator &other) const {
        return m_iterator == other.m_iterator;
    }

    KU_DEVICE_HOST bool operator!=(const KuTransformOutputIterator &other) const {
        return !(*this == other);
    }

private:
    OutputIterator m_iterator;
    function_type  m_fn;
};

template <typename OutputIterator, typename UnaryFunction>
KU_DEVICE_HOST auto makeKuTransformOutputIterator(OutputIterator it, UnaryFunction fn) {
    return KuTransformOutputIterator<OutputIterator, UnaryFunction>(it, std::move(fn));
}

} // namespace kuai
