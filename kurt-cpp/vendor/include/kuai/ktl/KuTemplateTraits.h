#pragma once

#include <type_traits>

namespace kuai {

template <typename T, template <typename...> class Template>
struct ku_is_instance_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct ku_is_instance_of<Template<Args...>, Template> : std::true_type {};

template <typename T, template <typename...> class Template>
inline constexpr bool ku_is_instance_of_v = ku_is_instance_of<T, Template>::value;

} // namespace kuai
