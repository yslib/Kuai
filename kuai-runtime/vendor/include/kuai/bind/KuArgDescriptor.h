#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <kuai/bind/KuCallableTraits.h>

namespace kuai {
class KuObject;
class KuFrameContext;
class KuContext;
} // namespace kuai

namespace kuai::bind {

namespace detail {
struct KuKwsCaster;
} // namespace detail

using slice = std::span<KuObject *const>;

struct kw {
    std::string_view m_name;
    KuObject        *m_value;
};

class kws {
public:
    using const_iterator = const kw *;

    KuObject *const *find(std::string_view name) const noexcept {
        for (const auto &entry : m_entries) {
            if (entry.m_name == name) {
                return &entry.m_value;
            }
        }
        return nullptr;
    }

    size_t size() const noexcept {
        return m_entries.size();
    }

    bool empty() const noexcept {
        return m_entries.empty();
    }

    const_iterator begin() const noexcept {
        return m_entries.data();
    }

    const_iterator end() const noexcept {
        return m_entries.data() + m_entries.size();
    }

private:
    explicit kws(std::span<const kw> entries) noexcept : m_entries(entries) {
    }

    std::span<const kw> m_entries;

    friend struct detail::KuKwsCaster;
};

enum class KuParameterKind {
    positional_only,
    positional_or_keyword,
    var_positional,
    keyword_only,
    var_keyword,
    injected,
};

struct KuParameterRecord {
    std::string     m_name;
    KuParameterKind m_kind;
    bool            m_has_default{false};
};

template <typename T>
struct arg_v;

struct arg {
    constexpr explicit arg(const char *name = nullptr) : m_name(name) {
    }

    template <typename T>
    constexpr arg_v<std::decay_t<T>> operator=(T &&value) const;

    static constexpr KuParameterKind kind = KuParameterKind::positional_or_keyword;
    static constexpr bool            has_default = false;
    const char                      *m_name;
};

template <typename T>
struct arg_v : arg {
    using value_type = T;

    template <typename U>
    constexpr arg_v(const arg &base, U &&value) : arg(base), m_value(std::forward<U>(value)) {
    }

    static constexpr bool has_default = true;
    T                     m_value;
};

template <typename T>
constexpr arg_v<std::decay_t<T>> arg::operator=(T &&value) const {
    return {*this, std::forward<T>(value)};
}

// Marks all preceding arguments as positional-only, like '/' in a Python signature.
struct pos_only {};

// Marks all following ordinary arguments as keyword-only, like '*' in a Python signature.
struct kw_only {};

struct varargs : arg {
    using arg::arg;
    static constexpr KuParameterKind kind = KuParameterKind::var_positional;
};

struct kwargs : arg {
    using arg::arg;
    static constexpr KuParameterKind kind = KuParameterKind::var_keyword;
};

// Example: mod.def("f", &f, arg("x"), pos_only(), arg("y"), varargs("rest"),
//                  kw_only(), arg("scale") = 1, kwargs("options"));

namespace detail {
template <typename T>
struct ku_is_function_annotation : std::false_type {};

template <>
struct ku_is_function_annotation<arg> : std::true_type {};

template <typename T>
struct ku_is_function_annotation<arg_v<T>> : std::true_type {};

template <>
struct ku_is_function_annotation<pos_only> : std::true_type {};

template <>
struct ku_is_function_annotation<kw_only> : std::true_type {};

template <>
struct ku_is_function_annotation<varargs> : std::true_type {};

template <>
struct ku_is_function_annotation<kwargs> : std::true_type {};

template <typename T>
inline constexpr bool ku_is_function_annotation_v =
    ku_is_function_annotation<std::remove_cvref_t<T>>::value;

template <typename T>
struct ku_is_parameter_annotation : std::false_type {};

template <>
struct ku_is_parameter_annotation<arg> : std::true_type {};

template <typename T>
struct ku_is_parameter_annotation<arg_v<T>> : std::true_type {};

template <>
struct ku_is_parameter_annotation<varargs> : std::true_type {};

template <>
struct ku_is_parameter_annotation<kwargs> : std::true_type {};

template <typename T>
inline constexpr bool ku_is_parameter_annotation_v =
    ku_is_parameter_annotation<std::remove_cvref_t<T>>::value;

template <typename T>
struct ku_is_default_annotation : std::false_type {};

template <typename T>
struct ku_is_default_annotation<arg_v<T>> : std::true_type {};

template <typename T>
inline constexpr bool ku_is_default_annotation_v =
    ku_is_default_annotation<std::remove_cvref_t<T>>::value;

inline constexpr size_t KuMissingAnnotation = static_cast<size_t>(-1);

template <typename T>
struct ku_is_injected_context : std::false_type {};

template <>
struct ku_is_injected_context<KuFrameContext> : std::true_type {};

template <>
struct ku_is_injected_context<KuContext> : std::true_type {};

template <typename T>
using ku_injected_object_type_t =
    std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

template <typename T>
inline constexpr bool ku_is_injected_parameter_v =
    (std::is_lvalue_reference_v<T> || std::is_pointer_v<std::remove_reference_t<T>>)
    && ku_is_injected_context<ku_injected_object_type_t<T>>::value;

template <typename T>
inline constexpr bool ku_is_slice_parameter_v = std::is_same_v<T, slice>;

template <typename T>
inline constexpr bool ku_is_kws_parameter_v = std::is_same_v<T, const kws &>;

template <typename T>
inline constexpr bool ku_is_variadic_parameter_v =
    std::is_same_v<std::remove_cvref_t<T>, slice> || std::is_same_v<std::remove_cvref_t<T>, kws>;

template <typename Fn, size_t I>
using ku_function_parameter_type_t = ku_function_arg_t<I, std::remove_cvref_t<Fn>>;

class KuParameterRecordBuilder {
public:
    void add(const arg &annotation) {
        addNamed(annotation, false);
    }

    template <typename T>
    void add(const arg_v<T> &annotation) {
        addNamed(annotation, true);
    }

    void add(pos_only) {
        if (m_records.empty() || m_sawPosOnly || m_keywordOnly || m_sawKwargs) {
            throw std::invalid_argument("pos_only must follow the leading positional parameters");
        }
        for (auto &record : m_records) {
            if (record.m_kind == KuParameterKind::positional_or_keyword) {
                record.m_kind = KuParameterKind::positional_only;
            }
        }
        m_sawPosOnly = true;
    }

    void add(kw_only) {
        if (m_sawKwOnly || m_sawKwargs) {
            throw std::invalid_argument("kw_only may appear at most once and before kwargs");
        }
        m_sawKwOnly = true;
        m_keywordOnly = true;
    }

    void add(const varargs &annotation) {
        ensureParameterMayFollow();
        if (m_sawVarargs || m_keywordOnly) {
            throw std::invalid_argument("varargs must appear before keyword-only parameters");
        }
        m_records.push_back(KuParameterRecord{annotation.m_name == nullptr ? "" : annotation.m_name,
                                              KuParameterKind::var_positional, false});
        m_sawVarargs = true;
        m_keywordOnly = true;
    }

    void add(const kwargs &annotation) {
        ensureParameterMayFollow();
        m_records.push_back(KuParameterRecord{annotation.m_name == nullptr ? "" : annotation.m_name,
                                              KuParameterKind::var_keyword, false});
        m_sawKwargs = true;
    }

    std::vector<KuParameterRecord> finish() && {
        return std::move(m_records);
    }

private:
    void ensureParameterMayFollow() const {
        if (m_sawKwargs) {
            throw std::invalid_argument("no parameter may follow kwargs");
        }
    }

    void addNamed(const arg &annotation, bool hasDefault) {
        ensureParameterMayFollow();
        m_records.push_back(KuParameterRecord{
            annotation.m_name == nullptr ? "" : annotation.m_name,
            m_keywordOnly ? KuParameterKind::keyword_only : KuParameterKind::positional_or_keyword,
            hasDefault});
    }

    std::vector<KuParameterRecord> m_records;
    bool                           m_sawPosOnly{false};
    bool                           m_sawKwOnly{false};
    bool                           m_sawVarargs{false};
    bool                           m_sawKwargs{false};
    bool                           m_keywordOnly{false};
};

template <typename Fn, size_t... Is>
constexpr size_t ku_visible_parameter_count(std::index_sequence<Is...>) {
    return (size_t{0} + ...
            + (ku_is_injected_parameter_v<ku_function_parameter_type_t<Fn, Is>> ? 0 : 1));
}

template <typename Fn>
inline constexpr size_t ku_visible_parameter_count_v = ku_visible_parameter_count<Fn>(
    std::make_index_sequence<ku_function_arity_v<std::remove_cvref_t<Fn>>>());

template <size_t Arity>
struct KuStaticBindingPlan {
    std::array<KuParameterKind, Arity> m_parameter_kinds{};
    std::array<size_t, Arity>          m_annotation_indices{};
    std::array<bool, Arity>            m_has_defaults{};
};

template <typename Fn, typename Annotations, size_t... Is>
constexpr auto makeVisibleBindingPlan(std::index_sequence<Is...>) {
    constexpr size_t visibleCount = ku_visible_parameter_count_v<Fn>;
    constexpr size_t descriptorCount =
        (size_t{0} + ...
         + (ku_is_parameter_annotation_v<std::tuple_element_t<Is, Annotations>> ? size_t{1}
                                                                                : size_t{0}));
    static_assert(sizeof...(Is) == 0 || descriptorCount == visibleCount,
                  "argument annotation count must match the non-injected handler parameter count");

    KuStaticBindingPlan<visibleCount> plan{};
    for (auto &annotationIndex : plan.m_annotation_indices) {
        annotationIndex = KuMissingAnnotation;
    }

    if constexpr (sizeof...(Is) == 0) {
        for (auto &kind : plan.m_parameter_kinds) {
            kind = KuParameterKind::positional_only;
        }
    } else {
        size_t parameterIndex = 0;
        bool   keywordOnly = false;
        auto   add = [&]<size_t I>() {
            using Annotation = std::tuple_element_t<I, Annotations>;
            if constexpr (std::is_same_v<Annotation, pos_only>) {
                for (size_t i = 0; i < parameterIndex; ++i) {
                    if (plan.m_parameter_kinds[i] == KuParameterKind::positional_or_keyword) {
                        plan.m_parameter_kinds[i] = KuParameterKind::positional_only;
                    }
                }
            } else if constexpr (std::is_same_v<Annotation, kw_only>) {
                keywordOnly = true;
            } else if constexpr (ku_is_parameter_annotation_v<Annotation>) {
                plan.m_annotation_indices[parameterIndex] = I;
                plan.m_has_defaults[parameterIndex] = ku_is_default_annotation_v<Annotation>;
                if constexpr (std::is_same_v<Annotation, varargs>) {
                    plan.m_parameter_kinds[parameterIndex] = KuParameterKind::var_positional;
                    keywordOnly = true;
                } else if constexpr (std::is_same_v<Annotation, kwargs>) {
                    plan.m_parameter_kinds[parameterIndex] = KuParameterKind::var_keyword;
                } else {
                    plan.m_parameter_kinds[parameterIndex] =
                        keywordOnly ? KuParameterKind::keyword_only
                                      : KuParameterKind::positional_or_keyword;
                }
                ++parameterIndex;
            }
        };
        (add.template operator()<Is>(), ...);
    }
    return plan;
}

template <typename Fn, size_t VisibleArity, size_t... Is>
constexpr auto mergeStaticBindingPlan(const KuStaticBindingPlan<VisibleArity> &visible,
                                      std::index_sequence<Is...>) {
    constexpr size_t           arity = ku_function_arity_v<std::remove_cvref_t<Fn>>;
    KuStaticBindingPlan<arity> plan{};
    for (auto &annotationIndex : plan.m_annotation_indices) {
        annotationIndex = KuMissingAnnotation;
    }

    size_t visibleIndex = 0;
    (
        [&] {
            if constexpr (ku_is_injected_parameter_v<ku_function_parameter_type_t<Fn, Is>>) {
                plan.m_parameter_kinds[Is] = KuParameterKind::injected;
            } else {
                plan.m_parameter_kinds[Is] = visible.m_parameter_kinds[visibleIndex];
                plan.m_annotation_indices[Is] = visible.m_annotation_indices[visibleIndex];
                plan.m_has_defaults[Is] = visible.m_has_defaults[visibleIndex];
                ++visibleIndex;
            }
        }(),
        ...);
    return plan;
}

template <typename Fn, typename... Extra>
struct KuBindingPlan {
    using function_type = std::remove_cvref_t<Fn>;
    using annotation_types = std::tuple<std::remove_cvref_t<Extra>...>;

    static constexpr size_t arity = ku_function_arity_v<function_type>;
    static constexpr size_t visible_arity = ku_visible_parameter_count_v<function_type>;
    static constexpr auto   visible_parameters =
        makeVisibleBindingPlan<function_type, annotation_types>(
            std::make_index_sequence<sizeof...(Extra)>());
    static constexpr auto parameters = mergeStaticBindingPlan<function_type>(
        visible_parameters, std::make_index_sequence<arity>());
    static constexpr auto parameter_kinds = parameters.m_parameter_kinds;
    static constexpr auto annotation_indices = parameters.m_annotation_indices;
    static constexpr auto has_defaults = parameters.m_has_defaults;
    template <size_t I>
    static constexpr bool parameter_type_matches = [] {
        using Parameter = ku_function_parameter_type_t<function_type, I>;
        if constexpr (parameter_kinds[I] == KuParameterKind::var_positional) {
            return ku_is_slice_parameter_v<Parameter>;
        } else if constexpr (parameter_kinds[I] == KuParameterKind::var_keyword) {
            return ku_is_kws_parameter_v<Parameter>;
        } else {
            return !ku_is_variadic_parameter_v<Parameter>;
        }
    }();
    static constexpr bool parameter_types_match = []<size_t... Is>(std::index_sequence<Is...>) {
        return (parameter_type_matches<Is> && ...);
    }(std::make_index_sequence<arity>());
    static_assert(
        parameter_types_match,
        "varargs and kwargs descriptors must match bind::slice and const bind::kws & parameters");
    static constexpr bool every_parameter_accepts_position = [] {
        for (const auto kind : parameter_kinds) {
            if (kind != KuParameterKind::injected && kind != KuParameterKind::positional_only
                && kind != KuParameterKind::positional_or_keyword) {
                return false;
            }
        }
        return true;
    }();
};

template <typename Fn, size_t... Is>
std::vector<KuParameterRecord> mergeInjectedParameterRecords(std::vector<KuParameterRecord> visible,
                                                             std::index_sequence<Is...>) {
    std::vector<KuParameterRecord> records;
    records.reserve(sizeof...(Is));
    size_t visibleIndex = 0;
    (
        [&] {
            if constexpr (ku_is_injected_parameter_v<ku_function_parameter_type_t<Fn, Is>>) {
                records.push_back(KuParameterRecord{"", KuParameterKind::injected, false});
            } else {
                records.push_back(std::move(visible[visibleIndex++]));
            }
        }(),
        ...);
    return records;
}

template <typename Fn, typename... Extra>
std::vector<KuParameterRecord> makeKuParameterRecords(Extra &&...extra) {
    using FnType = std::remove_cvref_t<Fn>;
    static_assert((ku_is_function_annotation_v<Extra> && ...),
                  "only kuai argument annotations may follow the handler");
    constexpr size_t descriptorCount =
        (size_t{0} + ... + (ku_is_parameter_annotation_v<Extra> ? size_t{1} : size_t{0}));
    constexpr size_t visibleCount = ku_visible_parameter_count_v<FnType>;
    static_assert(sizeof...(Extra) == 0 || descriptorCount == visibleCount,
                  "argument annotation count must match the non-injected handler parameter count");

    std::vector<KuParameterRecord> visible;
    if constexpr (sizeof...(Extra) == 0) {
        visible.assign(visibleCount,
                       KuParameterRecord{"", KuParameterKind::positional_only, false});
    } else {
        KuParameterRecordBuilder builder;
        (builder.add(std::forward<Extra>(extra)), ...);
        visible = std::move(builder).finish();
    }

    return mergeInjectedParameterRecords<FnType>(
        std::move(visible), std::make_index_sequence<ku_function_arity_v<FnType>>());
}
} // namespace detail

} // namespace kuai::bind
