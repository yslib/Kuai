#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>)
KU_DEVICE_HOST constexpr KuBool8 logical_xor(const T1 &t1, const T2 &t2) noexcept {
    if constexpr (ku_has_void_type_v<T1, T2>) {
        return ku_value_traits<KuBool8>::null();
    } else if (ku_value_traits<decltype(t1)>::isNull(t1)
               || ku_value_traits<decltype(t2)>::isNull(t2)) {
        return ku_value_traits<KuBool8>::null();
    } else {
        return ku_value_traits<KuBool8>::fromRepr(
            ku_value_traits<KuBool8>::repr(cast<KuBool8>(t1))
            ^ ku_value_traits<KuBool8>::repr(cast<KuBool8>(t2)));
    }
}

} // namespace functional_detail

struct ku_xor {
    template <typename T1, typename T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::logical_xor(t1, t2)))
            -> decltype(functional_detail::logical_xor(t1, t2)) {
        return functional_detail::logical_xor(t1, t2);
    }
};
} // namespace kuai
