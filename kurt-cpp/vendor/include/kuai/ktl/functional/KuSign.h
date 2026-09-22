#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_number_v<T>
KU_DEVICE_HOST constexpr KuI32 sign(const T &t1) noexcept {
    if (ku_value_traits<decltype(t1)>::isNull(t1))
        return ku_value_traits<KuI32>::null();
    return static_cast<KuI32>(static_cast<KuI32>(t1 > 0) - static_cast<KuI32>(t1 < 0));
}

} // namespace functional_detail

struct ku_sign {
    template <typename T1>
    KU_DEVICE_HOST constexpr auto operator()(const T1 &t1) const
        noexcept(noexcept(functional_detail::sign(t1))) -> decltype(functional_detail::sign(t1)) {
        return functional_detail::sign(t1);
    }
};
} // namespace kuai
