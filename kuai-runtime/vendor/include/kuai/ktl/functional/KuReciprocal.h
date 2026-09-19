#pragma once

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_primitive_type_v<T>
KU_DEVICE_HOST constexpr auto reciprocal(const T &t1) noexcept -> ku_floating_result_type_t<T> {
    if constexpr (std::is_same_v<T, KuVoid8>) {
        return ku_value_traits<KuF64>::null();
    } else if constexpr (std::is_same_v<T, KuF32>) {
        return (ku_value_traits<decltype(t1)>::isNull(t1)
                || ku_value_traits<decltype(t1)>::repr(t1) == 0.f)
                   ? ku_value_traits<KuF32>::null()
                   : KuF32(1.0 / t1);
    } else {
        return (ku_value_traits<decltype(t1)>::isNull(t1)
                || ku_value_traits<decltype(t1)>::repr(t1) == 0.0)
                   ? ku_value_traits<KuF64>::null()
                   : KuF64(1.0 / cast<KuF64>(t1));
    }
}

} // namespace functional_detail

struct ku_reciprocal {
    template <class T1>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1) const
        noexcept(noexcept(functional_detail::reciprocal(t1)))
            -> decltype(functional_detail::reciprocal(t1)) {
        return functional_detail::reciprocal(t1);
    }
};
} // namespace kuai
