#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/ktl/functional/KuGt.h>
namespace kuai {

namespace functional_detail {

template <typename T1, typename T2>
    requires(ku_is_primitive_type_v<T1> && ku_is_primitive_type_v<T2>)
KU_DEVICE_HOST constexpr auto max(const T1 &t1, const T2 &t2) noexcept
    -> ku_promotion_type_t<T1, T2> {
    using R = ku_promotion_type_t<T1, T2>;
    return ku_value_traits<KuBool8>::repr(gt(t1, t2)) == KU_BOOL_TRUE ? cast<R>(t1) : cast<R>(t2);
}

} // namespace functional_detail

struct ku_max {
    template <typename T1, typename T2>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1, const T2 &t2) const
        noexcept(noexcept(functional_detail::max(t1, t2)))
            -> decltype(functional_detail::max(t1, t2)) {
        return functional_detail::max(t1, t2);
    }
};
} // namespace kuai
