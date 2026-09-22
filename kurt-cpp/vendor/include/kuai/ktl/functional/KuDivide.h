#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>)
KU_DEVICE_HOST constexpr auto divide(const T1 &t1, const T2 &t2) noexcept
    -> ku_promotion_type_t<T1, T2> {
    using R = ku_promotion_type_t<T1, T2>;
    if constexpr (ku_has_void_type_v<T1, T2>) {
        return ku_value_traits<R>::null();
    } else {
        if (ku_value_traits<decltype(t1)>::isNull(t1) || ku_value_traits<decltype(t2)>::isNull(t2)
            || ku_value_traits<decltype(t2)>::repr(t2) == 0) {
            return ku_value_traits<R>::null();
        }
        if constexpr (std::is_same_v<R, KuBool8>) {
            return ku_value_traits<KuBool8>::fromRepr(ku_value_traits<decltype(t1)>::repr(t1)
                                                      / ku_value_traits<decltype(t2)>::repr(t2));
        } else {
            auto x = ku_value_traits<decltype(t1)>::repr(t1);
            auto y = ku_value_traits<decltype(t2)>::repr(t2);
            auto r = x / y;
            if constexpr (std::is_integral_v<R> || std::is_same_v<R, KuBool8>) {
                if (((x < 0 && y > 0) || (x > 0 && y < 0)) && (x % y != 0)) {
                    r -= 1;
                }
                return r;
            } else {
                return r;
            }
        }
    }
}

} // namespace functional_detail

struct ku_divide {
    template <class T1, class T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::divide(t1, t2)))
            -> decltype(functional_detail::divide(t1, t2)) {
        return functional_detail::divide(t1, t2);
    }
};
} // namespace kuai
