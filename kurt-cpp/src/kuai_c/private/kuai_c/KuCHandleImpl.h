#pragma once

#include <concepts>
#include <type_traits>

#include <kuai/core/KuRefCounted.h>
#include <kuai/kuai_c/KuCHandle.h>

#define KU_C_REF_COUNTED_LIFETIME_IMPL(RETAIN_FUNCTION, RELEASE_FUNCTION, C_HANDLE)              \
    static_assert(std::derived_from<std::remove_cv_t<::kuai::capi::ku_c_handle_cpp_t<C_HANDLE>>, \
                                    ::kuai::KuRefCounted>,                                       \
                  #C_HANDLE " must map to a public KuRefCounted-derived C++ type");              \
    extern "C" ku_status_t RETAIN_FUNCTION(C_HANDLE handle) {                                    \
        KU_ASSERT(handle != nullptr, #RETAIN_FUNCTION " requires a non-null " #C_HANDLE);        \
        ::kuai::capi::fromHandle(handle)->ref();                                                 \
        return KU_STATUS_SUCCESS;                                                                \
    }                                                                                            \
    extern "C" ku_status_t RELEASE_FUNCTION(C_HANDLE handle) {                                   \
        KU_ASSERT(handle != nullptr, #RELEASE_FUNCTION " requires a non-null " #C_HANDLE);       \
        ::kuai::capi::fromHandle(handle)->deref();                                               \
        return KU_STATUS_SUCCESS;                                                                \
    }
