#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_primitive_type_v<T>
KU_DEVICE_HOST constexpr auto log(const T &a) noexcept -> ku_floating_result_type_t<T> {
    if constexpr (std::is_same_v<T, KuVoid8>) {
        return ku_value_traits<KuF64>::null();
    } else {
        if constexpr (std::is_same_v<T, KuF32>) {
            return ku_value_traits<decltype(a)>::isNull(a) ? a : ::logf(a); // KuF32
        } else {
            return ku_value_traits<decltype(a)>::isNull(a) ? ku_value_traits<KuF64>::null()
                                                           : ::log(cast<KuF64>(a)); // KuF64
        }
    }
}

} // namespace functional_detail

struct ku_log {
    template <class T>
    KU_DEVICE_HOST constexpr auto operator()(const T &a) const
        noexcept(noexcept(functional_detail::log(a))) -> decltype(functional_detail::log(a)) {
        return functional_detail::log(a);
    }
};
} // namespace kuai
