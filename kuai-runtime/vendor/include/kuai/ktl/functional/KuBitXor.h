#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires((ku_is_bool_v<T1> && ku_is_bool_v<T2>)
             || (ku_is_integral_v<T1> && ku_is_integral_v<T2>))
KU_DEVICE_HOST constexpr auto bit_xor(const T1 &t1, const T2 &t2) noexcept
    -> ku_bit_op_type_t<T1, T2> {
    using R = ku_bit_op_type_t<T1, T2>;
    if constexpr (ku_has_void_type_v<T1, T2>) {
        return ku_value_traits<R>::null();
    } else {
        if (ku_value_traits<decltype(t1)>::isNull(t1)
            || ku_value_traits<decltype(t2)>::isNull(t2)) {
            return ku_value_traits<R>::null();
        }
        return R(ku_value_traits<decltype(t1)>::repr(t1) ^ ku_value_traits<decltype(t2)>::repr(t2));
    }
}

} // namespace functional_detail

struct ku_bit_xor {
    template <typename T1, typename T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::bit_xor(t1, t2)))
            -> decltype(functional_detail::bit_xor(t1, t2)) {
        return functional_detail::bit_xor(t1, t2);
    }
};

} // namespace kuai
