#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>

namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>
             && !std::is_same_v<T1, KuBool8> && !std::is_same_v<T2, KuBool8>)
KU_DEVICE_HOST constexpr auto ratio(const T1 &t1, const T2 &t2) noexcept
    -> ku_ratio_result_type_t<T1, T2> {
    using R = ku_ratio_result_type_t<T1, T2>;
    if constexpr (ku_has_void_type_v<T1, T2>) {
        return ku_value_traits<R>::null();
    } else {
        if (ku_value_traits<decltype(t1)>::isNull(t1) || ku_value_traits<decltype(t2)>::isNull(t2)
            || ku_value_traits<decltype(t2)>::repr(t2) == 0) {
            return ku_value_traits<R>::null();
        }
        return static_cast<R>(ku_value_traits<decltype(t1)>::repr(t1))
               / static_cast<R>(ku_value_traits<decltype(t2)>::repr(t2));
    }
}

} // namespace functional_detail

struct ku_ratio {
    template <class T1, class T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::ratio(t1, t2)))
            -> decltype(functional_detail::ratio(t1, t2)) {
        return functional_detail::ratio(t1, t2);
    }
};
} // namespace kuai
