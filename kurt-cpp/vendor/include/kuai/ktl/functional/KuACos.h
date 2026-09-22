#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {
template <typename T>
    requires ku_is_primitive_type_v<T>
KU_DEVICE_HOST constexpr auto acos(const T &a) noexcept -> ku_floating_result_type_t<T> {
    if constexpr (std::is_same_v<T, KuVoid8>) {
        return ku_value_traits<KuF64>::null();
    } else {
        if constexpr (std::is_same_v<T, KuF32>) {
            if (ku_value_traits<decltype(a)>::isNull(a) || (a < -1.f || a > 1.f))
                return ku_value_traits<KuF32>::null();
            return ::acosf(a); // KuF32
        } else {
            auto val = cast<KuF64>(a);
            if (ku_value_traits<decltype(val)>::isNull(val) || (val < -1.0 || val > 1.0))
                return ku_value_traits<KuF64>::null();
            return ::acos(val); // KuF64
        }
    }
}

} // namespace functional_detail

struct ku_acos {
    template <typename T1>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1) const
        noexcept(noexcept(functional_detail::acos(t1))) -> decltype(functional_detail::acos(t1)) {
        return functional_detail::acos(t1);
    }
};
} // namespace kuai
