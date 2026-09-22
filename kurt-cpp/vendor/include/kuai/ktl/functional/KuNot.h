#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_primitive_type_v<T>
KU_DEVICE_HOST constexpr T logical_not(const T &t1) noexcept {
    if constexpr (std::is_same_v<T, KuVoid8>) {
        return ku_value_traits<T>::null();
    } else if constexpr (std::is_same_v<T, KuBool8>) {
        return ku_value_traits<decltype(t1)>::isNull(t1)
                   ? ku_value_traits<T>::null()
                   : ku_value_traits<T>::fromBool(ku_value_traits<decltype(t1)>::repr(t1)
                                                  == KU_BOOL_FALSE);
    } else if (ku_value_traits<decltype(t1)>::isNull(t1)) {
        return ku_value_traits<T>::null();
    } else {
        return ku_value_traits<T>::repr(t1) == 0 ? T(1) : ku_value_traits<T>::dflt();
    }
}

} // namespace functional_detail

struct ku_not {
    template <typename T1>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1) const
        noexcept(noexcept(functional_detail::logical_not(t1)))
            -> decltype(functional_detail::logical_not(t1)) {
        return functional_detail::logical_not(t1);
    }
};

} // namespace kuai
