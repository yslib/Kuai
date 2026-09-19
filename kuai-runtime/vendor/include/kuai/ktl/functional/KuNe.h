#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/functional/KuEq.h>
#include <kuai/ktl/functional/KuNot.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>)
KU_DEVICE_HOST constexpr KuBool8 ne(const T1 &t1, const T2 &t2) noexcept {
    return logical_not(eq(t1, t2));
}

} // namespace functional_detail

struct ku_ne {
    template <typename T1, typename T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::ne(t1, t2)))
            -> decltype(functional_detail::ne(t1, t2)) {
        return functional_detail::ne(t1, t2);
    }
};
} // namespace kuai
