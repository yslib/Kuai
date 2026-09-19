#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_primitive_type_v<T>
KU_DEVICE_HOST constexpr auto cbrt(const T &a) noexcept -> ku_floating_result_type_t<T> {
    if constexpr (std::is_same_v<T, KuVoid8>) {
        return ku_value_traits<KuF64>::null();
    } else {
        if constexpr (std::is_same_v<T, KuF32>) {
            if (ku_value_traits<decltype(a)>::isNull(a))
                return ku_value_traits<KuF32>::null();
            return ::cbrtf(a); // KuF32
        } else {
            KuF64 val = cast<KuF64>(a);
            if (ku_value_traits<decltype(a)>::isNull(a))
                return ku_value_traits<KuF64>::null();
            return ::cbrt(val); // KuF64
        }
    }
}

} // namespace functional_detail

struct ku_cbrt {
    template <class T>
    KU_DEVICE_HOST constexpr auto operator()(const T &a) const
        noexcept(noexcept(functional_detail::cbrt(a))) -> decltype(functional_detail::cbrt(a)) {
        return functional_detail::cbrt(a);
    }
};
} // namespace kuai
