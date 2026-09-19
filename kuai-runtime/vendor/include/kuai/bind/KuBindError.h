#pragma once

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuResult.h>
#include <kuai/kuai_c/ku_runtime.h>

namespace kuai::bind::detail {

class ku_argument_error final : public std::runtime_error {
public:
    ku_argument_error() : std::runtime_error("parameter mismatch") {
    }
};

template <typename T, typename = void>
struct ku_has_member_to_string : std::false_type {};

template <typename T>
struct ku_has_member_to_string<T, std::void_t<decltype(std::declval<const T &>().toString())>>
    : std::bool_constant<
          std::is_constructible_v<std::string, decltype(std::declval<const T &>().toString())>> {};

template <typename T>
auto ku_adl_to_string(const T &value) -> decltype(toString(value)) {
    return toString(value);
}

template <typename T, typename = void>
struct ku_has_adl_to_string : std::false_type {};

template <typename T>
struct ku_has_adl_to_string<T, std::void_t<decltype(ku_adl_to_string(std::declval<const T &>()))>>
    : std::bool_constant<
          std::is_constructible_v<std::string,
                                  decltype(ku_adl_to_string(std::declval<const T &>()))>> {};

template <typename E>
std::string ku_result_error_message(const E &error) {
    using Error = std::remove_cvref_t<E>;
    if constexpr (std::is_base_of_v<std::exception, Error>) {
        return error.what();
    } else if constexpr (std::is_constructible_v<std::string, const E &>) {
        return std::string(error);
    } else if constexpr (ku_has_member_to_string<Error>::value) {
        return std::string(error.toString());
    } else if constexpr (ku_has_adl_to_string<Error>::value) {
        return std::string(ku_adl_to_string(error));
    } else {
        return "KuResult returned an error";
    }
}

template <typename E>
ku_status_t ku_result_error_status(const E &error) noexcept {
    if constexpr (requires { error.status(); }) {
        return error.status();
    } else {
        return KU_STATUS_BUILTIN_ERROR;
    }
}

template <typename T>
struct ku_unwrapped_return {
    using type = T;
};

template <typename T, typename E>
struct ku_unwrapped_return<KuResult<T, E>> {
    using type = T;
};

template <typename T>
using ku_unwrapped_return_t = typename ku_unwrapped_return<std::remove_cvref_t<T>>::type;

} // namespace kuai::bind::detail
