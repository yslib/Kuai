#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2> && !ku_is_float_v<T1>
             && !ku_is_float_v<T2>)
KU_DEVICE_HOST constexpr auto mod(const T1 &t1, const T2 &t2) noexcept
    -> ku_promotion_type_t<T1, T2> {
    using R = ku_promotion_type_t<T1, T2>;
    if constexpr (ku_has_void_type_v<T1, T2>) {
        return ku_value_traits<R>::null();
    } else if constexpr (std::is_same_v<R, KuBool8>) {
        if (ku_value_traits<decltype(t1)>::isNull(t1) || ku_value_traits<decltype(t2)>::isNull(t2)
            || ku_value_traits<decltype(t2)>::repr(t2) == 0) {
            return ku_value_traits<R>::null();
        }
        auto v1 = ku_value_traits<decltype(t1)>::repr(t1);
        auto v2 = ku_value_traits<decltype(t2)>::repr(t2);
        auto ans = v1 % v2;
        if (ans != 0 && ((v1 > 0 && v2 < 0) || (v1 < 0 && v2 > 0))) {
            ans += v2;
        }
        return ku_value_traits<KuBool8>::fromRepr(ans);
    } else {
        if (ku_value_traits<decltype(t1)>::isNull(t1) || ku_value_traits<decltype(t2)>::isNull(t2)
            || ku_value_traits<decltype(t2)>::repr(t2) == 0) {
            return ku_value_traits<R>::null();
        }
        auto v1 = ku_value_traits<decltype(t1)>::repr(t1);
        auto v2 = ku_value_traits<decltype(t2)>::repr(t2);
        R    ans = v1 % v2;
        if (ans != 0 && ((v1 > 0 && v2 < 0) || (v1 < 0 && v2 > 0))) {
            ans += v2;
        }
        return R(ans);
    }
}

} // namespace functional_detail

struct ku_mod {
    template <class T1, class T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &a, const T2 &b) const
        noexcept(noexcept(functional_detail::mod(a, b))) -> decltype(functional_detail::mod(a, b)) {
        return functional_detail::mod(a, b);
    }
};
} // namespace kuai
