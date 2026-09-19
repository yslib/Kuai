#pragma once

#include <cmath>
#include <utility>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>
namespace kuai {

namespace functional_detail {

template <typename T>
inline constexpr bool is_cast_value_v =
    ku_is_primitive_type_v<T> || (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>);

template <typename T, typename F>
    requires(is_cast_value_v<T> && is_cast_value_v<F>)
KU_DEVICE_HOST constexpr T cast(const F &a) noexcept {
    if constexpr (std::is_same_v<F, T>) {
        return a;
    } else if constexpr (ku_has_void_type_v<F, T>) {
        return ku_value_traits<T>::null();
    } else {
        if constexpr (!std::numeric_limits<F>::is_integer && !std::is_same_v<T, KuBool8>
                      && !std::is_same_v<F, KuBool8> && std::numeric_limits<T>::is_integer) {
            if (ku_value_traits<decltype(a)>::isNull(a)) {
                return ku_value_traits<T>::null();
            }
            if constexpr (std::is_same_v<F, KuF64>) {
                return ::llround(a);
            } else {
                return ::llroundf(a);
            }
        } else {
            if constexpr (std::is_same_v<T, KuBool8> && !std::is_same_v<F, KuBool8>) {
                if (ku_value_traits<decltype(a)>::isNull(a)) {
                    return ku_value_traits<T>::null();
                }
                return ku_value_traits<KuBool8>::fromBool(ku_value_traits<decltype(a)>::repr(a)
                                                          != 0);
            } else if constexpr (!std::is_same_v<T, KuBool8> && std::is_same_v<F, KuBool8>) {
                if (ku_value_traits<decltype(a)>::isNull(a)) {
                    return ku_value_traits<T>::null();
                } else {
                    return static_cast<T>(ku_value_traits<decltype(a)>::repr(a));
                }
            } else {
                if (ku_value_traits<decltype(a)>::isNull(a)) {
                    return ku_value_traits<T>::null();
                }
                return a;
            }
        }
    }
}

template <typename T>
using integer_cast_result_t =
    std::conditional_t<(KuTypeTraits<T>::value >= KuTypeTraits<KuI64>::value), KuI64, T>;

} // namespace functional_detail

template <class T>
struct ku_cast {
    template <class F>
    KU_DEVICE_HOST constexpr auto operator()(const F &a) const
        noexcept(noexcept(functional_detail::cast<T>(a)))
            -> decltype(functional_detail::cast<T>(a)) {
        return functional_detail::cast<T>(a);
    }
};

template <class T, class Fn>
struct ku_map_cast {
    Fn m_fn;
    ku_map_cast(Fn &&fn) : m_fn(std::move(fn)) {
    }
    template <typename... Args>
    KU_DEVICE_HOST constexpr auto operator()(Args &&...args) const
        noexcept(noexcept(functional_detail::cast<T>(m_fn(std::forward<Args>(args)...))))
            -> decltype(functional_detail::cast<T>(m_fn(std::forward<Args>(args)...))) {
        return functional_detail::cast<T>(m_fn(std::forward<Args>(args)...));
    }
};

struct ku_integer_cast {
    template <typename T>
        requires ku_is_primitive_type_v<T>
    KU_DEVICE_HOST constexpr auto operator()(const T &a) const noexcept
        -> functional_detail::integer_cast_result_t<T> {
        return functional_detail::cast<functional_detail::integer_cast_result_t<T>>(a);
    }
};
} // namespace kuai
