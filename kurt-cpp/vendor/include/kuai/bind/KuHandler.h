#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include <kuai/bind/KuArgDescriptor.h>

namespace kuai::bind {

namespace detail {
template <typename FnType, FnType Fn>
struct ku_nttp_handler_impl;

template <typename R, typename... Args, R (*Fn)(Args...)>
struct ku_nttp_handler_impl<R (*)(Args...), Fn> {
    R operator()(Args... args) const {
        return Fn(std::forward<Args>(args)...);
    }
};

template <typename R, typename... Args, R (*Fn)(Args...) noexcept>
struct ku_nttp_handler_impl<R (*)(Args...) noexcept, Fn> {
    R operator()(Args... args) const {
        return Fn(std::forward<Args>(args)...);
    }
};

template <auto Fn>
using ku_nttp_handler = ku_nttp_handler_impl<decltype(Fn), Fn>;

template <typename Fn>
inline constexpr bool ku_is_stateless_handler_v =
    std::is_empty_v<std::decay_t<Fn>> && std::is_trivially_default_constructible_v<std::decay_t<Fn>>
    && std::is_trivially_destructible_v<std::decay_t<Fn>>;

template <typename Fn, typename... Extra>
inline constexpr bool ku_uses_direct_ffi_v = [] {
    using FnType = std::decay_t<Fn>;
    using Plan = KuBindingPlan<FnType, std::remove_cvref_t<Extra>...>;
    if constexpr (!ku_is_stateless_handler_v<FnType>) {
        return false;
    }
    for (size_t i = 0; i < Plan::arity; ++i) {
        if (Plan::parameter_kinds[i] != KuParameterKind::injected
            && Plan::parameter_kinds[i] != KuParameterKind::positional_only) {
            return false;
        }
        if (Plan::has_defaults[i]) {
            return false;
        }
    }
    return true;
}();

template <typename Annotation>
struct KuAnnotationStorage {
    template <typename Value>
    explicit constexpr KuAnnotationStorage(Value &&) {
    }
};

template <typename T>
struct KuAnnotationStorage<arg_v<T>> {
    template <typename Annotation>
    explicit constexpr KuAnnotationStorage(Annotation &&annotation)
        : m_value(std::forward<Annotation>(annotation).m_value) {
    }

    T m_value;
};
} // namespace detail

template <typename Fn, typename... Extra>
struct KuBoundFunction {
    using function_type = Fn;
    using annotation_types = std::tuple<Extra...>;
    using annotations_type = std::tuple<detail::KuAnnotationStorage<Extra>...>;
    using binding_plan = detail::KuBindingPlan<Fn, Extra...>;

    [[no_unique_address]] Fn               m_fn;
    [[no_unique_address]] annotations_type m_annotations;
};

namespace detail {
template <typename Fn, typename... Extra>
auto makeKuBoundFunction(Fn &&fn, Extra &&...extra) {
    using Bound = KuBoundFunction<std::decay_t<Fn>, std::decay_t<Extra>...>;
    return Bound{std::forward<Fn>(fn),
                 typename Bound::annotations_type{
                     KuAnnotationStorage<std::decay_t<Extra>>(std::forward<Extra>(extra))...}};
}
} // namespace detail

} // namespace kuai::bind
