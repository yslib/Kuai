#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include <kuai/bind/KuArgDescriptor.h>
#include <kuai/bind/KuCallableTraits.h>
#include <kuai/bind/KuObjectCaster.h>
#include <kuai/kuai_c/ku_builtin.h>

namespace kuai::bind {

template <typename... Args>
struct overload {};

namespace detail {

template <typename... Args>
struct ku_overload_cast_impl {
    template <typename Return>
    constexpr auto operator()(Return (*function)(Args...)) const noexcept -> Return (*)(Args...) {
        return function;
    }

    template <typename Return>
    constexpr auto operator()(Return (*function)(Args...) noexcept) const noexcept
        -> Return (*)(Args...) noexcept {
        return function;
    }
};

} // namespace detail

template <typename... Args>
inline constexpr detail::ku_overload_cast_impl<Args...> overload_cast{};

enum class KuArgumentShapeMatch : unsigned char {
    none,
    variadic,
    defaulted,
    exact,
};

namespace detail {

// Overload identity describes what a kuai frame can distinguish. Top-level cv/ref spelling is
// erased, so T, T &, and const T & share one key. Pointer form is retained because a nullable T *
// must remain distinct from a non-null T argument. Injected and binder wrapper types remain in the
// list after the same normalization, so their concrete types still distinguish overloads.
template <typename T>
using ku_normalized_overload_arg_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename... Args>
struct ku_overload_key {};

template <typename T>
struct ku_normalized_overload;

template <typename... Args>
struct ku_normalized_overload<overload<Args...>> {
    using type = ku_overload_key<ku_normalized_overload_arg_t<Args>...>;
};

template <typename T>
using ku_normalized_overload_t = typename ku_normalized_overload<std::remove_cv_t<T>>::type;

template <typename Fn, size_t... Is>
auto ku_handler_overload(std::index_sequence<Is...>)
    -> overload<ku_function_arg_t<Is, std::remove_cvref_t<Fn>>...>;

template <typename Fn>
using ku_handler_overload_t = decltype(ku_handler_overload<Fn>(
    std::make_index_sequence<ku_function_arity_v<std::remove_cvref_t<Fn>>>()));

// Writable storage keeps different signature tokens distinct under constant folding.
template <typename Key>
inline unsigned char ku_overload_key_token = 0;

template <typename Key>
constexpr const void *ku_overload_key_id() noexcept {
    return &ku_overload_key_token<Key>;
}

template <typename Signature>
constexpr const void *ku_overload_id() noexcept {
    return ku_overload_key_id<ku_normalized_overload_t<Signature>>();
}

template <typename Fn>
constexpr const void *ku_handler_overload_id() noexcept {
    return ku_overload_id<ku_handler_overload_t<Fn>>();
}

template <typename Arg>
bool ku_match_injected_parameter(const ku_frame_t *frame) noexcept {
    if constexpr (!ku_is_injected_parameter_v<Arg>) {
        return true;
    } else {
        using Caster = KuArgCaster<ku_frame_ctx_t, ku_injected_object_type_t<Arg> *>;
        Caster caster{};
        static_assert(std::is_same_v<decltype(caster.load(frame->ctx)), bool>,
                      "injected argument caster load() must return bool");
        static_assert(noexcept(caster.load(frame->ctx)),
                      "injected argument caster load() must be noexcept");
        return caster.load(frame->ctx);
    }
}

template <typename Fn, size_t... Is>
bool ku_match_handler_injected_parameters(const ku_frame_t *frame,
                                          std::index_sequence<Is...>) noexcept {
    return (...
            && ku_match_injected_parameter<ku_function_arg_t<Is, std::remove_cvref_t<Fn>>>(frame));
}

template <typename Fn>
bool ku_match_handler_injected_parameters(const ku_frame_t *frame) noexcept {
    return ku_match_handler_injected_parameters<Fn>(
        frame, std::make_index_sequence<ku_function_arity_v<std::remove_cvref_t<Fn>>>());
}

template <typename Fn, typename... Extra>
KuArgumentShapeMatch ku_match_handler_argument_shape(const ku_frame_t *frame) noexcept {
    using Plan = KuBindingPlan<std::remove_cvref_t<Fn>, std::remove_cvref_t<Extra>...>;

    size_t requiredArguments = 0;
    size_t requiredPositionalArguments = 0;
    size_t ordinaryArguments = 0;
    size_t positionalCapacity = 0;
    size_t keywordCapacity = 0;
    bool   hasVarargs = false;
    bool   hasKwargs = false;

    for (size_t i = 0; i < Plan::arity; ++i) {
        const auto kind = Plan::parameter_kinds[i];
        switch (kind) {
            case KuParameterKind::positional_only:
                ++ordinaryArguments;
                ++positionalCapacity;
                if (!Plan::has_defaults[i]) {
                    ++requiredArguments;
                    ++requiredPositionalArguments;
                }
                break;
            case KuParameterKind::positional_or_keyword:
                ++ordinaryArguments;
                ++positionalCapacity;
                ++keywordCapacity;
                if (!Plan::has_defaults[i]) {
                    ++requiredArguments;
                }
                break;
            case KuParameterKind::keyword_only:
                ++ordinaryArguments;
                ++keywordCapacity;
                if (!Plan::has_defaults[i]) {
                    ++requiredArguments;
                }
                break;
            case KuParameterKind::var_positional:
                hasVarargs = true;
                break;
            case KuParameterKind::var_keyword:
                hasKwargs = true;
                break;
            case KuParameterKind::injected:
                break;
        }
    }

    if (frame->pargn < requiredPositionalArguments
        || (!hasVarargs && frame->pargn > positionalCapacity)
        || (!hasKwargs && frame->kargn > keywordCapacity)) {
        return KuArgumentShapeMatch::none;
    }
    const auto argumentCount = frame->pargn + frame->kargn;
    if (argumentCount < requiredArguments) {
        return KuArgumentShapeMatch::none;
    }
    if (!hasVarargs && !hasKwargs && argumentCount > ordinaryArguments) {
        return KuArgumentShapeMatch::none;
    }
    if (hasVarargs || hasKwargs) {
        return KuArgumentShapeMatch::variadic;
    }
    if (argumentCount < ordinaryArguments) {
        return KuArgumentShapeMatch::defaulted;
    }
    return KuArgumentShapeMatch::exact;
}

template <typename Fn, size_t... Is>
constexpr size_t ku_injected_parameter_count(std::index_sequence<Is...>) noexcept {
    return (size_t{0} + ...
            + (ku_is_injected_parameter_v<ku_function_arg_t<Is, std::remove_cvref_t<Fn>>>
                   ? size_t{1}
                   : size_t{0}));
}

template <typename Fn>
inline constexpr size_t ku_injected_parameter_count_v = ku_injected_parameter_count<Fn>(
    std::make_index_sequence<ku_function_arity_v<std::remove_cvref_t<Fn>>>());

template <typename Fn, typename... Extra>
int ku_match_handler(void *, const ku_frame_t *frame) noexcept {
    if (frame == nullptr || !ku_match_handler_injected_parameters<Fn>(frame)) {
        return -1;
    }

    const auto shape = ku_match_handler_argument_shape<Fn, Extra...>(frame);
    if (shape == KuArgumentShapeMatch::none) {
        return -1;
    }

    constexpr auto ShapeRankCount = static_cast<int>(KuArgumentShapeMatch::exact) + 1;
    constexpr auto InjectedRank = static_cast<int>(ku_injected_parameter_count_v<Fn>);
    return InjectedRank * ShapeRankCount + static_cast<int>(shape);
}

} // namespace detail

} // namespace kuai::bind
