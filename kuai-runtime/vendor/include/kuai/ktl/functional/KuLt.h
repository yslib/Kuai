#pragma once

#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>)
KU_DEVICE_HOST constexpr KuBool8 lt(const T1 &t1, const T2 &t2) noexcept {
    using R = ku_promotion_type_t<T1, T2>;
    if constexpr (ku_both_void_type_v<T1, T2>) {
        return ku_value_traits<KuBool8>::fromBool(false);
    } else if constexpr (std::is_same_v<T1, KuVoid>) {
        return ku_value_traits<KuBool8>::fromBool(true);
    } else if constexpr (std::is_same_v<T2, KuVoid>) {
        return ku_value_traits<KuBool8>::fromBool(false);
    } else {
        return ku_value_traits<KuBool8>::fromBool(ku_value_traits<R>::repr(cast<R>(t1))
                                                  < ku_value_traits<R>::repr(cast<R>(t2)));
    }
}

} // namespace functional_detail

struct ku_lt {
    template <typename T1, typename T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::lt(t1, t2)))
            -> decltype(functional_detail::lt(t1, t2)) {
        return functional_detail::lt(t1, t2);
    }
};
} // namespace kuai
