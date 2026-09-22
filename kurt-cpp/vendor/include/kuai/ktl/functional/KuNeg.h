#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_number_v<T>
KU_DEVICE_HOST constexpr T neg(const T &t1) noexcept {
    if (ku_value_traits<decltype(t1)>::isNull(t1)) {
        return t1;
    }
    return -t1;
}

} // namespace functional_detail

struct ku_neg {
    template <class T1>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1) const
        noexcept(noexcept(functional_detail::neg(t1))) -> decltype(functional_detail::neg(t1)) {
        return functional_detail::neg(t1);
    }
};
} // namespace kuai
