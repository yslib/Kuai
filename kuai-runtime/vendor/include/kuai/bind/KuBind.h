#pragma once
#include <array>
#include <cstdio>
#include <memory>
#include <new>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <kuai/bind/KuArgDescriptor.h>
#include <kuai/bind/KuBindError.h>
#include <kuai/bind/KuCallableTraits.h>
#include <kuai/bind/KuHandler.h>
#include <kuai/bind/KuObjectCaster.h>
#include <kuai/bind/KuOverload.h>
#include <kuai/core/KuCore.h>
#include <kuai/core/KuObject.h>
#include <kuai/core/KuSmallBuffer.h>
#include <kuai/kuai_c/ku_builtin.h>

#include "kuai_c/KuCHandle.h"

namespace kuai::bind {

namespace detail {
template <typename Arg, typename Storage>
decltype(auto) castArgument(Storage &value) {
    if constexpr (std::is_lvalue_reference_v<Arg> && std::is_pointer_v<Storage>) {
        return static_cast<Arg>(*value);
    } else if constexpr (std::is_lvalue_reference_v<Arg>) {
        return static_cast<Arg>(value);
    } else if constexpr (std::is_rvalue_reference_v<Arg>) {
        return static_cast<Arg>(std::move(value));
    } else {
        return static_cast<Arg>(value);
    }
}
} // namespace detail

namespace detail {
template <typename T>
struct ku_return_count : std::integral_constant<size_t, 1> {};

template <typename... Ts>
struct ku_return_count<std::tuple<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};

template <typename T>
inline constexpr size_t ku_return_count_v = ku_return_count<std::remove_cvref_t<T>>::value;

struct KuSliceCaster {
    KuSmallBufferN<KuObject *, 8> m_storage;
    slice                         m_value;

    bool load(const ku_object_t *objects, size_t count) noexcept {
        if (count != 0 && objects == nullptr) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            m_storage.push_back(capi::fromHandle(objects[i]));
        }
        m_value = m_storage.view();
        return true;
    }
};

struct KuKwsCaster {
    KuSmallBufferN<kw, 8> m_storage;
    kws                   m_value;

    KuKwsCaster() noexcept : m_value(m_storage.view()) {
    }

    void push_back(std::string_view name, ku_object_t object) noexcept {
        m_storage.push_back(kw{name, capi::fromHandle(object)});
    }

    void finish() noexcept {
        m_value = kws(m_storage.view());
    }
};

template <typename Caster, typename... Inputs>
bool loadCaster(Caster &caster, Inputs &&...inputs) noexcept {
    static_assert(std::is_same_v<decltype(caster.load(std::forward<Inputs>(inputs)...)), bool>,
                  "argument caster load() must return bool");
    static_assert(noexcept(caster.load(std::forward<Inputs>(inputs)...)),
                  "argument caster load() must be noexcept");
    return caster.load(std::forward<Inputs>(inputs)...);
}

template <typename Out>
void ku_store_return(Out &&value, ku_object_t *out) noexcept {
    using Caster = KuArgCaster<ku_object_t, std::remove_cvref_t<Out>>;
    static_assert(noexcept(Caster{.m_value = std::forward<Out>(value)}),
                  "return caster construction must be noexcept");
    static_assert(std::is_nothrow_destructible_v<Caster>,
                  "return caster destruction must be noexcept");

    Caster caster{.m_value = std::forward<Out>(value)};
    static_assert(std::is_same_v<decltype(caster.store(out)), void>,
                  "return caster store() must return void");
    static_assert(noexcept(caster.store(out)), "return caster store() must be noexcept");
    caster.store(out);
}

template <typename Out>
void ku_write_returns(Out &&returned, ku_object_t *outs) noexcept {
    static_assert(std::is_nothrow_destructible_v<std::remove_cvref_t<Out>>,
                  "handler return values must be nothrow destructible");
    ku_store_return(std::forward<Out>(returned), outs);
}

template <typename... Outs, size_t... Is>
void ku_write_return_tuple(std::tuple<Outs...> &&returned,
                           ku_object_t          *outs,
                           std::index_sequence<Is...>) noexcept {
    static_assert((std::is_nothrow_destructible_v<Outs> && ...),
                  "handler return values must be nothrow destructible");
    (ku_store_return(std::get<Is>(std::move(returned)), outs + Is), ...);
}

template <typename... Outs>
void ku_write_returns(std::tuple<Outs...> &&returned, ku_object_t *outs) noexcept {
    ku_write_return_tuple(std::move(returned), outs, std::index_sequence_for<Outs...>());
}

inline void ku_assert_frame_contract(const ku_frame_t *frame) noexcept {
    (void)frame;
    KU_ASSERT(frame != nullptr, "ku_frame_t must not be null");
    KU_ASSERT(frame->argv != nullptr,
              "ku_frame_t::argv must not be null, even for an empty argument list");
    KU_ASSERT(frame->pargn <= SIZE_MAX - frame->kargn, "ku_frame_t argument count overflow");
    KU_ASSERT(frame->kargn == 0 || frame->knames != nullptr,
              "ku_frame_t::knames must not be null when keyword arguments are present");
    KU_ASSERT(frame->results != nullptr, "ku_frame_t::results must not be null");
    KU_ASSERT(frame->result_capacity > 0, "ku_frame_t::result_capacity must be greater than zero");
}

template <typename Return, typename Invoke>
ku_status_t ku_invoke_and_write(ku_frame_t *frame, Invoke &&invoke) noexcept {
    using Value = ku_unwrapped_return_t<Return>;
    static_assert(!std::is_void_v<Value>, "kuai handlers must return a value or a tuple of values");
    constexpr size_t ResultCount = ku_return_count_v<Value>;

    frame->result_count = 0;
    if (frame->result_capacity < ResultCount) {
        frame->result_count = ResultCount;
        return KU_STATUS_BUFFER_TOO_SMALL;
    }

    try {
        auto returned = std::forward<Invoke>(invoke)();
        if constexpr (KuIsResultType_v<std::remove_cvref_t<Return>>) {
            if (!returned.hasValue()) {
                const auto message = ku_result_error_message(returned.error());
                if (!message.empty()) {
                    std::fprintf(stderr, "[kuai] builtin execution error: %s\n", message.c_str());
                    std::fflush(stderr);
                }
                return ku_result_error_status(returned.error());
            }
            ku_write_returns(std::move(returned).value(), frame->results);
        } else {
            ku_write_returns(std::move(returned), frame->results);
        }
        frame->result_count = ResultCount;
        return KU_STATUS_SUCCESS;
    } catch (const ::kuai::detail::KuStatusError &error) {
        frame->result_count = 0;
        return error.status();
    } catch (const ku_argument_error &) {
        frame->result_count = 0;
        return KU_STATUS_INVALID_ARGUMENT;
    } catch (const std::bad_alloc &) {
        frame->result_count = 0;
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        frame->result_count = 0;
        return KU_STATUS_INTERNAL_ERROR;
    }
}

template <typename Arg, bool = ku_is_injected_parameter_v<Arg>>
struct ku_argument_caster;

template <typename Arg>
struct ku_argument_caster<Arg, false> {
    using Bare = std::remove_cvref_t<Arg>;
    using CasterValue = std::conditional_t<std::is_lvalue_reference_v<Arg>, Arg, Bare>;
    using type = std::conditional_t<std::is_same_v<Bare, slice>,
                                    KuSliceCaster,
                                    std::conditional_t<std::is_same_v<Bare, kws>,
                                                       KuKwsCaster,
                                                       KuArgCaster<ku_object_t, CasterValue>>>;
};

template <typename Arg>
struct ku_argument_caster<Arg, true> {
    using type = KuArgCaster<ku_frame_ctx_t, ku_injected_object_type_t<Arg> *>;
};

template <typename Arg>
using ku_argument_caster_t = typename ku_argument_caster<Arg>::type;

template <typename Arg, typename Caster>
decltype(auto) loadedArgument(Caster &caster) {
    if constexpr (!ku_is_injected_parameter_v<Arg>) {
        return castArgument<Arg>(caster.m_value);
    } else if constexpr (std::is_pointer_v<std::remove_reference_t<Arg>>) {
        return static_cast<Arg>(caster.m_value);
    } else {
        return static_cast<Arg>(*caster.m_value);
    }
}

template <typename... Args>
struct ku_args_loader {
    using tuple_type = std::tuple<ku_argument_caster_t<Args>...>;

    bool load(ku_frame_t *frame) noexcept {
        size_t visibleIndex = 0;
        return loadImpl(frame, visibleIndex, std::make_index_sequence<sizeof...(Args)>());
    }

    template <size_t I>
    auto &caster() {
        return std::get<I>(m_casters);
    }

    template <typename F>
    decltype(auto) invoke(F &&fn) {
        return invokeImpl(std::forward<F>(fn), std::make_index_sequence<sizeof...(Args)>());
    }

private:
    template <typename Arg, typename Caster>
    static bool loadOne(Caster &caster, ku_frame_t *frame, size_t &visibleIndex) noexcept {
        if constexpr (ku_is_injected_parameter_v<Arg>) {
            KU_ASSERT(frame->ctx != nullptr,
                      "an injected context parameter requires ku_frame_t::ctx");
            return loadCaster(caster, frame->ctx);
        } else {
            return loadCaster(caster, frame->argv[visibleIndex++]);
        }
    }

    template <size_t... Is>
    bool loadImpl(ku_frame_t *frame, size_t &visibleIndex, std::index_sequence<Is...>) noexcept {
        return (... && loadOne<Args>(std::get<Is>(m_casters), frame, visibleIndex));
    }

    template <typename F, size_t... Is>
    decltype(auto) invokeImpl(F &&fn, std::index_sequence<Is...>) {
        return std::forward<F>(fn)(loadedArgument<Args>(std::get<Is>(m_casters))...);
    }

    tuple_type m_casters;
};

template <typename F, size_t... Is>
decltype(auto) ku_try_invoke(F &&fn, ku_frame_t *frame, std::index_sequence<Is...>) {
    ku_args_loader<ku_function_arg_t<Is, std::remove_cvref_t<F>>...> loader;
    if (!loader.load(frame)) {
        throw ku_argument_error();
    }
    return loader.invoke(std::forward<F>(fn));
}

template <typename F>
ku_status_t ku_invoke(F &&fn, ku_frame_t *frame) noexcept {
    using Fn = std::remove_cvref_t<F>;
    KU_DEBUG_EXPR(ku_assert_frame_contract(frame));
    KU_ASSERT(frame->pargn == ku_visible_parameter_count_v<F>,
              "direct kuai FFI requires exactly the registered positional argument count");
    KU_ASSERT(frame->kargn == 0, "direct kuai FFI does not accept keyword arguments");
    return ku_invoke_and_write<ku_function_return_t<Fn>>(frame, [&]() -> decltype(auto) {
        return ku_try_invoke(fn, frame, std::make_index_sequence<ku_function_arity_v<Fn>>());
    });
}

template <typename F>
static ku_status_t erased_ffi_entry(ku_frame_t *frame) noexcept {
    // Unchecked hot entry: the resolver must supply a frame matching the registered signature.
    return ku_invoke(F{}, frame);
}

enum class KuArgumentSourceKind {
    missing,
    object,
    object_range,
    keyword_collection,
    default_value,
    injected
};

struct KuArgumentSource {
    KuArgumentSourceKind m_kind{KuArgumentSourceKind::missing};
    size_t               m_index{0};
    size_t               m_count{0};
};

template <typename Plan>
void mapBoundArguments(const std::vector<KuParameterRecord>      &parameters,
                       ku_frame_t                                *frame,
                       std::array<KuArgumentSource, Plan::arity> &sources) {
    constexpr size_t Arity = Plan::arity;
    KU_ASSERT(parameters.size() == Arity,
              "parameter records must match the registered handler arity");

    size_t positionalIndex = 0;
    for (size_t i = 0; i < Arity; ++i) {
        const auto kind = Plan::parameter_kinds[i];
        if (kind == KuParameterKind::injected) {
            sources[i].m_kind = KuArgumentSourceKind::injected;
        } else if ((kind == KuParameterKind::positional_only
                    || kind == KuParameterKind::positional_or_keyword)
                   && positionalIndex < frame->pargn) {
            sources[i] = {KuArgumentSourceKind::object, positionalIndex++};
        } else if (kind == KuParameterKind::var_positional) {
            sources[i] = {KuArgumentSourceKind::object_range, positionalIndex,
                          frame->pargn - positionalIndex};
            positionalIndex = frame->pargn;
        } else if (kind == KuParameterKind::var_keyword) {
            sources[i].m_kind = KuArgumentSourceKind::keyword_collection;
        }
    }
    KU_ASSERT(positionalIndex == frame->pargn, "frame contains too many positional arguments");

    for (size_t keywordIndex = 0; keywordIndex < frame->kargn; ++keywordIndex) {
        const auto &name = frame->knames[keywordIndex];
        KU_ASSERT(name.data != nullptr, "keyword name data must not be null");
        KU_ASSERT(name.size != 0, "keyword names must not be empty");
        const std::string_view keyword{name.data, name.size};
        KU_DEBUG(for (size_t previousIndex = 0; previousIndex < keywordIndex; ++previousIndex) {
            const auto &previousName = frame->knames[previousIndex];
            KU_ASSERT(std::string_view(previousName.data, previousName.size) != keyword,
                      "frame contains a duplicate keyword name");
        });

        size_t parameterIndex = Arity;
        for (size_t i = 0; i < Arity; ++i) {
            const auto kind = Plan::parameter_kinds[i];
            if ((kind == KuParameterKind::positional_or_keyword
                 || kind == KuParameterKind::keyword_only)
                && parameters[i].m_name == keyword) {
                parameterIndex = i;
                break;
            }
        }
        if (parameterIndex == Arity) {
            KU_ASSERT(
                [] {
                    for (const auto kind : Plan::parameter_kinds) {
                        if (kind == KuParameterKind::var_keyword) {
                            return true;
                        }
                    }
                    return false;
                }(),
                "frame contains an unknown keyword argument");
            continue;
        }
        KU_ASSERT(sources[parameterIndex].m_kind == KuArgumentSourceKind::missing,
                  "frame binds a parameter more than once");
        sources[parameterIndex] = {KuArgumentSourceKind::object, frame->pargn + keywordIndex};
    }

    for (size_t i = 0; i < Arity; ++i) {
        if (sources[i].m_kind == KuArgumentSourceKind::missing) {
            KU_ASSERT(Plan::has_defaults[i], "frame omits a required argument");
            sources[i].m_kind = KuArgumentSourceKind::default_value;
        }
    }
}

template <typename Plan, typename Fn, size_t I, typename Caster, typename Bound>
bool loadBoundArgument(Caster                                          &caster,
                       const KuArgumentSource                          &source,
                       const std::array<KuArgumentSource, Plan::arity> &sources,
                       Bound                                           &bound,
                       ku_frame_t                                      *frame) {
    using Arg = ku_function_parameter_type_t<Fn, I>;
    (void)source;
    if constexpr (ku_is_injected_parameter_v<Arg>) {
        KU_ASSERT(source.m_kind == KuArgumentSourceKind::injected,
                  "invalid injected argument source");
        KU_ASSERT(frame->ctx != nullptr, "an injected context parameter requires ku_frame_t::ctx");
        return loadCaster(caster, frame->ctx);
    } else if constexpr (ku_is_slice_parameter_v<Arg>) {
        KU_ASSERT(source.m_kind == KuArgumentSourceKind::object_range,
                  "invalid varargs argument source");
        const ku_object_t *objects = frame->argv + source.m_index;
        return loadCaster(caster, objects, source.m_count);
    } else if constexpr (ku_is_kws_parameter_v<Arg>) {
        KU_ASSERT(source.m_kind == KuArgumentSourceKind::keyword_collection,
                  "invalid kwargs argument source");
        for (size_t keywordIndex = 0; keywordIndex < frame->kargn; ++keywordIndex) {
            const size_t objectIndex = frame->pargn + keywordIndex;
            bool         consumed = false;
            for (const auto &argumentSource : sources) {
                if (argumentSource.m_kind == KuArgumentSourceKind::object
                    && argumentSource.m_index == objectIndex) {
                    consumed = true;
                    break;
                }
            }
            if (!consumed) {
                const auto &name = frame->knames[keywordIndex];
                caster.push_back(std::string_view(name.data, name.size), frame->argv[objectIndex]);
            }
        }
        caster.finish();
        return true;
    } else {
        if (source.m_kind == KuArgumentSourceKind::object) {
            return loadCaster(caster, frame->argv[source.m_index]);
        }
        KU_ASSERT(source.m_kind == KuArgumentSourceKind::default_value,
                  "invalid ordinary argument source");

        using Annotations = typename Bound::annotation_types;
        constexpr size_t annotationIndex = Plan::annotation_indices[I];
        if constexpr (annotationIndex == KuMissingAnnotation) {
            KU_ASSERT(false, "default argument source has no registered default value");
            return false;
        } else {
            using Annotation = std::tuple_element_t<annotationIndex, Annotations>;
            if constexpr (!ku_is_default_annotation_v<Annotation>) {
                KU_ASSERT(false, "default argument source does not reference a default annotation");
                return false;
            } else {
                caster.m_value = std::get<annotationIndex>(bound.m_annotations).m_value;
                return true;
            }
        }
    }
}

template <typename Bound, typename Loader>
decltype(auto) invokeBoundHandler(Bound &bound, Loader &loader) {
    return loader.invoke(bound.m_fn);
}

template <typename Bound, size_t... Is>
decltype(auto) ku_try_invoke_bound(Bound                                &bound,
                                   const std::vector<KuParameterRecord> &parameters,
                                   ku_frame_t                           *frame,
                                   std::index_sequence<Is...>) {
    using Fn = typename Bound::function_type;
    using Plan = typename Bound::binding_plan;
    std::array<KuArgumentSource, Plan::arity> sources{};
    mapBoundArguments<Plan>(parameters, frame, sources);

    ku_args_loader<ku_function_parameter_type_t<Fn, Is>...> loader;
    if (!(...
          && loadBoundArgument<Plan, Fn, Is>(loader.template caster<Is>(), sources[Is], sources,
                                             bound, frame))) {
        throw ku_argument_error();
    }
    return invokeBoundHandler(bound, loader);
}

template <typename Bound>
ku_status_t ku_invoke_bound(Bound                                &bound,
                            const std::vector<KuParameterRecord> &parameters,
                            ku_frame_t                           *frame) noexcept {
    using Fn = typename Bound::function_type;
    using Plan = typename Bound::binding_plan;

    KU_DEBUG_EXPR(ku_assert_frame_contract(frame));

    if constexpr (Plan::every_parameter_accepts_position) {
        if (frame->kargn == 0 && frame->pargn == Plan::visible_arity) {
            return ku_invoke(bound.m_fn, frame);
        }
    }

    return ku_invoke_and_write<ku_function_return_t<Fn>>(frame, [&]() -> decltype(auto) {
        return ku_try_invoke_bound(bound, parameters, frame,
                                   std::make_index_sequence<ku_function_arity_v<Fn>>());
    });
}

template <typename CapturedState>
static ku_status_t captured_erased_ffi_entry(void *capture, ku_frame_t *frame) noexcept {
    auto &state = *static_cast<CapturedState *>(capture);
    return ku_invoke_bound(state.m_bound, state.m_parameters, frame);
}

template <typename Bound>
struct KuCapturedFunctionState {
    KuCapturedFunctionState(Bound value, std::vector<KuParameterRecord> parameterRecords)
        : m_bound(std::move(value)), m_parameters(std::move(parameterRecords)) {
    }

    [[no_unique_address]] Bound    m_bound;
    std::vector<KuParameterRecord> m_parameters;
};

template <typename State>
void destroyCapturedFunctionState(void *state) noexcept {
    delete static_cast<State *>(state);
}

class KuFunctionRegistration final {
public:
    explicit KuFunctionRegistration(ku_builtin_registration_t registration) noexcept
        : m_registration(registration) {
    }

    KuFunctionRegistration(KuFunctionRegistration &&other) noexcept
        : m_registration(other.m_registration),
          m_ownsState(std::exchange(other.m_ownsState, false)) {
    }

    KuFunctionRegistration(const KuFunctionRegistration &) = delete;
    KuFunctionRegistration &operator=(const KuFunctionRegistration &) = delete;

    ~KuFunctionRegistration() {
        if (m_ownsState) {
            m_registration.destroy(m_registration.state);
        }
    }

    [[nodiscard]] const ku_builtin_registration_t &get() const noexcept {
        return m_registration;
    }

    void releaseState() noexcept {
        m_ownsState = false;
    }

private:
    ku_builtin_registration_t m_registration{};
    bool                      m_ownsState{m_registration.state != nullptr};
};

template <typename Fn, typename... Extra>
KuFunctionRegistration makeKuFunctionRegistration(const char *name, Fn &&fn, Extra &&...extra) {
    using FnType = std::decay_t<Fn>;
    using BoundType = KuBoundFunction<FnType, std::decay_t<Extra>...>;
    const std::string_view nameView(name);

    ku_builtin_registration_t registration{};
    registration.name = ku_string_view_t{.data = nameView.data(), .size = nameView.size()};
    registration.overload_key = ku_handler_overload_id<FnType>();
    registration.match = &ku_match_handler<FnType, std::decay_t<Extra>...>;

    if constexpr (ku_uses_direct_ffi_v<FnType, Extra...>) {
        registration.target.kind = KU_CALL_TARGET_FFI;
        registration.target.value.ffi = &erased_ffi_entry<FnType>;
    } else {
        using State = KuCapturedFunctionState<BoundType>;
        auto parameters = makeKuParameterRecords<FnType>(extra...);
        auto bound = makeKuBoundFunction(std::forward<Fn>(fn), std::forward<Extra>(extra)...);
        auto state = std::make_unique<State>(std::move(bound), std::move(parameters));
        registration.target.kind = KU_CALL_TARGET_CALLABLE;
        registration.target.value.closure =
            ku_closure_t{.capture = state.get(), .ffi = &captured_erased_ffi_entry<State>};
        registration.state = state.get();
        registration.destroy = &destroyCapturedFunctionState<State>;
        KuFunctionRegistration result(registration);
        state.release();
        return result;
    }

    return KuFunctionRegistration(registration);
}

} // namespace detail

} // namespace kuai::bind
