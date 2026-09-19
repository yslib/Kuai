#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

namespace detail {

template <std::size_t Index, typename T>
struct KuTupleElement {
    T m_value;

    KU_DEVICE_HOST KuTupleElement() = default;

    template <typename U>
    KU_DEVICE_HOST explicit KuTupleElement(U &&v) : m_value(std::forward<U>(v)) {
    }
};

template <std::size_t Index, typename... Types>
struct KuTupleImpl;

template <std::size_t Index>
struct KuTupleImpl<Index> {};

template <std::size_t Index, typename Head, typename... Rest>
struct KuTupleImpl<Index, Head, Rest...> : public KuTupleElement<Index, Head>,
                                           public KuTupleImpl<Index + 1, Rest...> {
    KU_DEVICE_HOST KuTupleImpl() = default;

    template <typename HeadArg, typename... RestArgs>
        requires(sizeof...(RestArgs) == sizeof...(Rest))
    KU_DEVICE_HOST explicit KuTupleImpl(HeadArg &&head, RestArgs &&...tails)
        : KuTupleElement<Index, Head>(std::forward<HeadArg>(head)),
          KuTupleImpl<Index + 1, Rest...>(std::forward<RestArgs>(tails)...) {
    }
};

} // namespace detail

template <typename... Types>
struct KuTuple : public detail::KuTupleImpl<0, Types...> {
    KU_DEVICE_HOST          KuTuple() = default;
    KU_DEVICE_HOST          KuTuple(const KuTuple &) = default;
    KU_DEVICE_HOST          KuTuple(KuTuple &&) = default;
    KU_DEVICE_HOST KuTuple &operator=(const KuTuple &) = default;
    KU_DEVICE_HOST KuTuple &operator=(KuTuple &&) = default;

    template <typename... Args>
        requires(sizeof...(Args) == sizeof...(Types))
    KU_DEVICE_HOST explicit KuTuple(Args &&...args)
        : detail::KuTupleImpl<0, Types...>(std::forward<Args>(args)...) {
    }
};

template <std::size_t Index, typename T>
KU_DEVICE_HOST constexpr T &get(detail::KuTupleElement<Index, T> &element) noexcept {
    return element.m_value;
}

template <std::size_t Index, typename T>
KU_DEVICE_HOST constexpr const T &get(const detail::KuTupleElement<Index, T> &element) noexcept {
    return element.m_value;
}

template <std::size_t Index, typename... Types>
KU_DEVICE_HOST constexpr decltype(auto) get(KuTuple<Types...> &tuple) noexcept {
    using Element = std::tuple_element_t<Index, std::tuple<Types...>>;
    return get<Index>(static_cast<detail::KuTupleElement<Index, Element> &>(tuple));
}

template <std::size_t Index, typename... Types>
KU_DEVICE_HOST constexpr decltype(auto) get(const KuTuple<Types...> &tuple) noexcept {
    using Element = std::tuple_element_t<Index, std::tuple<Types...>>;
    return get<Index>(static_cast<const detail::KuTupleElement<Index, Element> &>(tuple));
}

template <std::size_t Index, typename... Types>
KU_DEVICE_HOST constexpr decltype(auto) get(KuTuple<Types...> &&tuple) noexcept {
    return std::move(get<Index>(tuple));
}

template <std::size_t Index, typename... Types>
KU_DEVICE_HOST constexpr decltype(auto) get(const KuTuple<Types...> &&tuple) noexcept {
    return std::move(get<Index>(tuple));
}

template <typename... Args>
KU_DEVICE_HOST auto makeTuple(Args &&...args) {
    return KuTuple<std::decay_t<Args>...>(std::forward<Args>(args)...);
}

} // namespace kuai

namespace std {

template <typename... Types>
struct tuple_size<kuai::KuTuple<Types...>> : integral_constant<size_t, sizeof...(Types)> {};

template <size_t Index, typename... Types>
struct tuple_element<Index, kuai::KuTuple<Types...>> : tuple_element<Index, tuple<Types...>> {};

} // namespace std
