#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_number_v<T1> && ku_is_number_v<T2>)
KU_DEVICE_HOST KuF64 pow(const T1 &t1, const T2 &t2) noexcept {
    if (ku_value_traits<decltype(t1)>::isNull(t1) || ku_value_traits<decltype(t2)>::isNull(t2)) {
        return ku_value_traits<KuF64>::null();
    }
    KuF64 val = ::pow(t1, t2);
    if (std::isnan(val) || std::isinf(val)) {
        return ku_value_traits<KuF64>::null();
    } else {
        return val;
    }
}

} // namespace functional_detail

struct ku_pow {
    template <class T1, class T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::pow(t1, t2)))
            -> decltype(functional_detail::pow(t1, t2)) {
        return functional_detail::pow(t1, t2);
    }
};

} // namespace kuai
