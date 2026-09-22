#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_number_v<T>
KU_DEVICE_HOST constexpr T abs(const T &t1) noexcept {
    if (ku_value_traits<decltype(t1)>::isNull(t1))
        return t1;
    return ::abs(t1);
}

} // namespace functional_detail

struct ku_abs {
    template <class T>
    KU_DEVICE_HOST constexpr auto operator()(const T &a) const
        noexcept(noexcept(functional_detail::abs(a))) -> decltype(functional_detail::abs(a)) {
        return functional_detail::abs(a);
    }
};
} // namespace kuai
