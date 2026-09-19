#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_primitive_type_v<T>
KU_DEVICE_HOST constexpr auto tan(const T &a) noexcept -> ku_floating_result_type_t<T> {
    if constexpr (std::is_same_v<T, KuVoid8>) {
        return ku_value_traits<KuF64>::null();
    } else {
        if constexpr (std::is_same_v<T, KuF32>) {
            if (ku_value_traits<decltype(a)>::isNull(a))
                return ku_value_traits<KuF32>::null();
            auto r = ::tanf(a); // KuF32
            return std::isnan(r) ? ku_value_traits<KuF32>::null() : r;
        } else {
            KuF64 val = cast<KuF64>(a);
            if (ku_value_traits<decltype(a)>::isNull(a))
                return ku_value_traits<KuF64>::null();
            auto r = ::tan(val); // KuF64
            return std::isnan(r) ? ku_value_traits<KuF64>::null() : r;
        }
    }
}

} // namespace functional_detail

struct ku_tan {
    template <typename T1>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1) const
        noexcept(noexcept(functional_detail::tan(t1))) -> decltype(functional_detail::tan(t1)) {
        return functional_detail::tan(t1);
    }
};
} // namespace kuai
