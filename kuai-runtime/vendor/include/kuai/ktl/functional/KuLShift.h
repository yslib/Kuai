#pragma once

#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_integral_v<T1> && ku_is_integral_v<T2>)
KU_DEVICE_HOST constexpr auto lshift(const T1 &t1, const T2 &t2) noexcept -> decltype(t1 << t2) {
    if (ku_value_traits<decltype(t1)>::isNull(t1) || ku_value_traits<decltype(t2)>::isNull(t2)) {
        return ku_value_traits<T1>::null();
    }
    return t1 << t2;
}

} // namespace functional_detail

struct ku_lshift {
    template <class T1, class T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::lshift(t1, t2)))
            -> decltype(functional_detail::lshift(t1, t2)) {
        return functional_detail::lshift(t1, t2);
    }
};

} // namespace kuai
