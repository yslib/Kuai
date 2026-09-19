#pragma once

#include <cstddef>
#include <functional>
#include <tuple>

namespace kuai {

template <typename T>
struct ku_function_traits;

template <typename R, typename... Args>
struct ku_function_traits<R(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;

    template <std::size_t I>
    using arg_t = std::tuple_element_t<I, args_tuple>;

    static constexpr std::size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct ku_function_traits<R (*)(Args...)> : ku_function_traits<R(Args...)> {};

template <typename R, typename... Args>
struct ku_function_traits<std::function<R(Args...)>> : ku_function_traits<R(Args...)> {};

template <typename C, typename R, typename... Args>
struct ku_function_traits<R (C::*)(Args...)> : ku_function_traits<R(Args...)> {};

template <typename C, typename R, typename... Args>
struct ku_function_traits<R (C::*)(Args...) const> : ku_function_traits<R(Args...)> {};

template <typename T>
struct ku_function_traits : ku_function_traits<decltype(&T::operator())> {};

template <typename Fn>
using ku_function_return_t = typename ku_function_traits<Fn>::return_type;

template <std::size_t I, typename Fn>
using ku_function_arg_t = typename ku_function_traits<Fn>::template arg_t<I>;

template <typename Fn>
inline constexpr std::size_t ku_function_arity_v = ku_function_traits<Fn>::arity;

} // namespace kuai
