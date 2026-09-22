#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>)
KU_DEVICE_HOST constexpr KuBool8 eq(const T1 &t1, const T2 &t2) noexcept {
    if constexpr (ku_both_void_type_v<T1, T2>) {
        return ku_value_traits<KuBool8>::fromBool(true);
    } else if constexpr (ku_has_void_type_v<T1, T2>) {
        return ku_value_traits<KuBool8>::fromBool(false);
    } else if (ku_value_traits<decltype(t1)>::isNull(t1)
               && ku_value_traits<decltype(t2)>::isNull(t2)) {
        return ku_value_traits<KuBool8>::fromBool(true);
    } else if (ku_value_traits<decltype(t1)>::isNull(t1)
               || ku_value_traits<decltype(t2)>::isNull(t2)) {
        return ku_value_traits<KuBool8>::fromBool(false);
    } else {
        return ku_value_traits<KuBool8>::fromBool(ku_value_traits<decltype(t1)>::repr(t1)
                                                  == ku_value_traits<decltype(t2)>::repr(t2));
    }
}

} // namespace functional_detail

struct ku_eq {
    template <typename T1, typename T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::eq(t1, t2)))
            -> decltype(functional_detail::eq(t1, t2)) {
        return functional_detail::eq(t1, t2);
    }
};
} // namespace kuai
