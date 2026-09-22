#pragma once

#include <cmath>

#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>)
KU_DEVICE_HOST constexpr auto iif(const KuBool8 &cond, const T1 &t1, const T2 &t2) noexcept
    -> ku_promotion_type_t<T1, T2> {
    using result_type = ku_promotion_type_t<T1, T2>;
    ku_cast<result_type> caster;
    const auto           value = ku_value_traits<decltype(cond)>::repr(cond);
    if (value == KU_BOOL_NULL) {
        return ku_value_traits<result_type>::null();
    }
    return value != 0 ? caster(t1) : caster(t2);
}

} // namespace functional_detail

struct ku_iif {
    template <typename T1, typename T2>
    KU_DEVICE_HOST constexpr auto operator()(const KuBool8 &cond, const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::iif(cond, t1, t2)))
            -> decltype(functional_detail::iif(cond, t1, t2)) {
        return functional_detail::iif(cond, t1, t2);
    }
};
} // namespace kuai
