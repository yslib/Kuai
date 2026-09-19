#pragma once

#include <cmath>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuCast.h>
namespace kuai {

namespace functional_detail {

template <typename T>
    requires ku_is_number_v<T>
KU_DEVICE_HOST constexpr auto round(const T &a, double multiplier) noexcept
    -> ku_floating_result_type_t<T> {
    using ResultType = ku_floating_result_type_t<T>;
    if (ku_value_traits<T>::isNull(a))
        return cast<ResultType>(a);
    ResultType multi = multiplier; // double or float
    auto       tmp =
        ku_value_traits<decltype(a)>::repr(a) * ku_value_traits<decltype(multi)>::repr(multi);
    return (tmp < 0 ? static_cast<long long>(tmp - 0.5) : static_cast<long long>(tmp + 0.5))
           / multi;
}

} // namespace functional_detail

struct ku_round {
    template <typename T>
    KU_DEVICE_HOST constexpr auto operator()(const T &a) const
        noexcept(noexcept(functional_detail::round(a, double{})))
            -> decltype(functional_detail::round(a, double{})) {
        return functional_detail::round(a, m_multiplier);
    }
    ku_round(double multiplier) : m_multiplier(multiplier) {
    }
    double m_multiplier = 0;
};
} // namespace kuai
