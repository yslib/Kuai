#pragma once

#include <cstddef>
#include <iterator>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

/// This iterator can be used directly in device code and benefit from polymorphism.
template <typename T>
struct KuPolymorphicIterator {
    KU_DEVICE_HOST virtual T operator[](std::size_t index) = 0;
    KU_DEVICE_HOST virtual ~KuPolymorphicIterator() = default;
};

template <typename Iterator>
struct KuPolymorphicIteratorAdaptor final
    : public KuPolymorphicIterator<typename std::iterator_traits<Iterator>::value_type> {
    Iterator m_iter;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    KU_DEVICE_HOST KuPolymorphicIteratorAdaptor(Iterator iter) : m_iter(iter) {
    }
    KU_DEVICE_HOST value_type operator[](std::size_t index) override final {
        return m_iter[index];
    }
};

} // namespace kuai
