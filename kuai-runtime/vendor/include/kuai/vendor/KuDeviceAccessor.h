#pragma once

#include <cstddef>
#include <type_traits>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

template <class ElementType, class Vendor>
class KuDeviceAccessor {
public:
    using element_type = ElementType;
    using data_handle_type = element_type *;
    using reference = element_type &;
    using const_reference = const element_type &;
    using vendor_type = Vendor;

    KU_DEVICE_HOST constexpr KuDeviceAccessor() noexcept = default;

    KU_DEVICE_HOST constexpr reference access(data_handle_type p, std::size_t i) noexcept {
        return p[i];
    }

    KU_DEVICE_HOST constexpr const_reference access(data_handle_type p,
                                                    std::size_t      i) const noexcept {
        return p[i];
    }

    KU_DEVICE_HOST constexpr data_handle_type offset(data_handle_type p,
                                                     std::size_t      i) const noexcept {
        return p + i;
    }
};

template <typename Accessor>
struct ku_is_device_accessor : std::false_type {};

template <typename ElementType, typename Vendor>
struct ku_is_device_accessor<KuDeviceAccessor<ElementType, Vendor>> : std::true_type {};

template <typename Accessor>
inline constexpr bool ku_is_device_accessor_v =
    ku_is_device_accessor<std::remove_cv_t<Accessor>>::value;

} // namespace kuai
