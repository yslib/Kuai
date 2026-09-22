#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuPointer.h>
#include <kuai/core/KuRefCounted.h>

#include "kuai_c/KuCHandle.h"

namespace kuai::capi {

template <typename Fn>
ku_status_t invokeBoundary(Fn &&fn) noexcept {
    static_assert(std::same_as<std::invoke_result_t<Fn>, ku_status_t>);
    try {
        return std::forward<Fn>(fn)();
    } catch (const ::kuai::detail::KuStatusError &error) {
        return error.status();
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
}

template <typename Handle>
using method_receiver_t = std::remove_const_t<ku_c_handle_cpp_t<std::remove_cvref_t<Handle>>>;

template <typename ReceiverHandle, typename Invoke>
ku_status_t
invokeStatusMethod(const char *functionName, ReceiverHandle receiver, Invoke &&invoke) noexcept {
    using Receiver = method_receiver_t<ReceiverHandle>;
    static_assert(std::same_as<std::invoke_result_t<Invoke, Receiver &>, ku_status_t>);
    static_assert(std::is_nothrow_invocable_v<Invoke, Receiver &>);

    static_cast<void>(functionName);
    KU_ASSERT(receiver != nullptr, functionName);
    return invokeBoundary([&]() -> ku_status_t {
        return std::invoke(std::forward<Invoke>(invoke), *fromHandle(receiver));
    });
}

template <typename ReceiverHandle, typename Output, typename Invoke>
ku_status_t invokeValueMethod(const char    *functionName,
                              ReceiverHandle receiver,
                              Output        *out,
                              Invoke       &&invoke) noexcept {
    using Receiver = method_receiver_t<ReceiverHandle>;
    using Result = std::invoke_result_t<Invoke, Receiver &>;
    static_assert(std::same_as<std::remove_cvref_t<Result>, std::remove_cv_t<Output>>);
    static_assert(std::is_nothrow_invocable_v<Invoke, Receiver &>);

    static_cast<void>(functionName);
    KU_ASSERT(receiver != nullptr, functionName);
    KU_ASSERT(out != nullptr, functionName);
    return invokeBoundary([&]() -> ku_status_t {
        auto value = std::invoke(std::forward<Invoke>(invoke), *fromHandle(receiver));
        *out = std::move(value);
        return KU_STATUS_SUCCESS;
    });
}

template <typename ReceiverHandle, typename OutputHandle, typename Invoke>
ku_status_t invokeBorrowedMethod(const char    *functionName,
                                 ReceiverHandle receiver,
                                 OutputHandle  *out,
                                 Invoke       &&invoke) noexcept {
    using Receiver = method_receiver_t<ReceiverHandle>;
    using Output = std::remove_const_t<ku_c_handle_cpp_t<OutputHandle>>;
    using Result = std::invoke_result_t<Invoke, Receiver &>;
    static_assert(std::is_pointer_v<Result>);
    static_assert(std::is_convertible_v<Result, Output *>);
    static_assert(std::is_nothrow_invocable_v<Invoke, Receiver &>);

    static_cast<void>(functionName);
    KU_ASSERT(receiver != nullptr, functionName);
    KU_ASSERT(out != nullptr, functionName);
    *out = nullptr;
    return invokeBoundary([&]() -> ku_status_t {
        auto *borrowed = std::invoke(std::forward<Invoke>(invoke), *fromHandle(receiver));
        KU_ASSERT(borrowed != nullptr, "a successful borrowed getter must return an object");
        *out = toHandle<OutputHandle>(borrowed);
        return KU_STATUS_SUCCESS;
    });
}

template <typename ReceiverHandle, typename OutputHandle, typename Invoke>
ku_status_t invokeBorrowedStatusMethod(const char    *functionName,
                                       ReceiverHandle receiver,
                                       OutputHandle  *out,
                                       Invoke       &&invoke) noexcept {
    using Receiver = method_receiver_t<ReceiverHandle>;
    using Output = std::remove_const_t<ku_c_handle_cpp_t<OutputHandle>>;
    static_assert(std::same_as<std::invoke_result_t<Invoke, Receiver &, Output **>, ku_status_t>);
    static_assert(std::is_nothrow_invocable_v<Invoke, Receiver &, Output **>);

    static_cast<void>(functionName);
    KU_ASSERT(receiver != nullptr, functionName);
    KU_ASSERT(out != nullptr, functionName);
    *out = nullptr;
    return invokeBoundary([&]() -> ku_status_t {
        Output    *borrowed = nullptr;
        const auto status =
            std::invoke(std::forward<Invoke>(invoke), *fromHandle(receiver), &borrowed);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        KU_ASSERT(borrowed != nullptr, "a successful borrowed method must return an object");
        *out = toHandle<OutputHandle>(borrowed);
        return KU_STATUS_SUCCESS;
    });
}

template <typename ReceiverHandle, typename OutputHandle, typename Invoke>
ku_status_t invokeOwnedMethod(const char    *functionName,
                              ReceiverHandle receiver,
                              OutputHandle  *out,
                              Invoke       &&invoke) noexcept {
    using Receiver = method_receiver_t<ReceiverHandle>;
    using Output = std::remove_const_t<ku_c_handle_cpp_t<OutputHandle>>;
    static_assert(std::derived_from<Output, KuRefCounted>);
    static_assert(
        std::same_as<std::invoke_result_t<Invoke, Receiver &, ku_sp<Output> &>, ku_status_t>);
    static_assert(std::is_nothrow_invocable_v<Invoke, Receiver &, ku_sp<Output> &>);

    static_cast<void>(functionName);
    KU_ASSERT(receiver != nullptr, functionName);
    KU_ASSERT(out != nullptr, functionName);
    *out = nullptr;
    return invokeBoundary([&]() -> ku_status_t {
        ku_sp<Output> owned;
        const auto status = std::invoke(std::forward<Invoke>(invoke), *fromHandle(receiver), owned);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        KU_ASSERT(owned != nullptr, "a successful owned method must return an object");
        *out = toHandle<OutputHandle>(owned.detach());
        return KU_STATUS_SUCCESS;
    });
}

template <typename ReceiverHandle, typename Output, typename Invoke>
ku_status_t invokeBorrowedPointerMethod(const char    *functionName,
                                        ReceiverHandle receiver,
                                        Output        *out,
                                        Invoke       &&invoke) noexcept {
    using Receiver = method_receiver_t<ReceiverHandle>;
    using Result = std::invoke_result_t<Invoke, Receiver &>;
    static_assert(std::is_pointer_v<Output>);
    static_assert(std::is_nothrow_invocable_v<Invoke, Receiver &>);

    static_cast<void>(functionName);
    KU_ASSERT(receiver != nullptr, functionName);
    KU_ASSERT(out != nullptr, functionName);
    *out = nullptr;
    return invokeBoundary([&]() -> ku_status_t {
        if constexpr (std::is_pointer_v<std::remove_reference_t<Result>>) {
            static_assert(std::is_convertible_v<Result, Output>);
            *out = std::invoke(std::forward<Invoke>(invoke), *fromHandle(receiver));
        } else {
            static_assert(std::is_lvalue_reference_v<Result>);
            static_assert(
                std::is_convertible_v<std::add_pointer_t<std::remove_reference_t<Result>>, Output>);
            auto &&borrowed = std::invoke(std::forward<Invoke>(invoke), *fromHandle(receiver));
            *out = std::addressof(borrowed);
        }
        KU_ASSERT(*out != nullptr, "a successful borrowed getter must return an object");
        return KU_STATUS_SUCCESS;
    });
}

} // namespace kuai::capi

#define KU_DEFINE_C_API_STATUS_METHOD(C_FUNCTION, PARAMETERS, RECEIVER, MEMBER, ...)               \
    extern "C" ku_status_t C_FUNCTION PARAMETERS {                                                 \
        return ::kuai::capi::invokeStatusMethod(                                                   \
            #C_FUNCTION, RECEIVER,                                                                 \
            [&](auto &ku_c_api_receiver) noexcept(noexcept(ku_c_api_receiver.MEMBER(__VA_ARGS__))) \
                -> decltype(auto) { return ku_c_api_receiver.MEMBER(__VA_ARGS__); });              \
    }

#define KU_DEFINE_C_API_VALUE_METHOD(C_FUNCTION, PARAMETERS, RECEIVER, OUT, MEMBER, ...)           \
    extern "C" ku_status_t C_FUNCTION PARAMETERS {                                                 \
        return ::kuai::capi::invokeValueMethod(                                                    \
            #C_FUNCTION, RECEIVER, OUT,                                                            \
            [&](auto &ku_c_api_receiver) noexcept(noexcept(ku_c_api_receiver.MEMBER(__VA_ARGS__))) \
                -> decltype(auto) { return ku_c_api_receiver.MEMBER(__VA_ARGS__); });              \
    }

#define KU_DEFINE_C_API_BORROWED_METHOD(C_FUNCTION, PARAMETERS, RECEIVER, OUT, MEMBER, ...)        \
    extern "C" ku_status_t C_FUNCTION PARAMETERS {                                                 \
        return ::kuai::capi::invokeBorrowedMethod(                                                 \
            #C_FUNCTION, RECEIVER, OUT,                                                            \
            [&](auto &ku_c_api_receiver) noexcept(noexcept(ku_c_api_receiver.MEMBER(__VA_ARGS__))) \
                -> decltype(auto) { return ku_c_api_receiver.MEMBER(__VA_ARGS__); });              \
    }

#define KU_DEFINE_C_API_BORROWED_STATUS_METHOD(C_FUNCTION, PARAMETERS, RECEIVER, OUT, MEMBER, ...) \
    extern "C" ku_status_t C_FUNCTION PARAMETERS {                                                 \
        return ::kuai::capi::invokeBorrowedStatusMethod(                                           \
            #C_FUNCTION, RECEIVER, OUT,                                                            \
            [&](auto &ku_c_api_receiver, auto **ku_c_api_out) noexcept(                            \
                noexcept(ku_c_api_receiver.MEMBER(__VA_ARGS__ __VA_OPT__(, )                       \
                                                      ku_c_api_out))) -> decltype(auto) {          \
                return ku_c_api_receiver.MEMBER(__VA_ARGS__ __VA_OPT__(, ) ku_c_api_out);          \
            });                                                                                    \
    }

#define KU_DEFINE_C_API_OWNED_METHOD(C_FUNCTION, PARAMETERS, RECEIVER, OUT, MEMBER, ...)  \
    extern "C" ku_status_t C_FUNCTION PARAMETERS {                                        \
        return ::kuai::capi::invokeOwnedMethod(                                           \
            #C_FUNCTION, RECEIVER, OUT,                                                   \
            [&](auto &ku_c_api_receiver, auto &ku_c_api_out) noexcept(                    \
                noexcept(ku_c_api_receiver.MEMBER(__VA_ARGS__ __VA_OPT__(, )              \
                                                      ku_c_api_out))) -> decltype(auto) { \
                return ku_c_api_receiver.MEMBER(__VA_ARGS__ __VA_OPT__(, ) ku_c_api_out); \
            });                                                                           \
    }

#define KU_DEFINE_C_API_BORROWED_POINTER_METHOD(C_FUNCTION, PARAMETERS, RECEIVER, OUT, MEMBER,     \
                                                ...)                                               \
    extern "C" ku_status_t C_FUNCTION PARAMETERS {                                                 \
        return ::kuai::capi::invokeBorrowedPointerMethod(                                          \
            #C_FUNCTION, RECEIVER, OUT,                                                            \
            [&](auto &ku_c_api_receiver) noexcept(noexcept(ku_c_api_receiver.MEMBER(__VA_ARGS__))) \
                -> decltype(auto) { return ku_c_api_receiver.MEMBER(__VA_ARGS__); });              \
    }
