#pragma once

#include <type_traits>

#include <kuai/kuai_c/ku_types.h>

namespace kuai {

class KuObject;
class KuCompletion;
class KuDevice;
class KuInstance;
class KuFrameContext;

namespace capi {

template <typename Handle>
struct ku_c_handle_traits;

template <typename Handle>
using ku_c_handle_cpp_t = typename ku_c_handle_traits<Handle>::type;

template <typename Handle>
[[nodiscard]] inline std::add_pointer_t<ku_c_handle_cpp_t<Handle>>
fromHandle(Handle handle) noexcept {
    static_assert(std::is_pointer_v<Handle>, "a kuai C handle must be an opaque pointer");
    using traits = ku_c_handle_traits<Handle>;
    if constexpr (traits::is_object) {
        static_assert(std::is_base_of_v<KuObject, std::remove_cv_t<typename traits::type>>,
                      "an object handle must map to an KuObject-derived C++ type");
    }
    return reinterpret_cast<std::add_pointer_t<ku_c_handle_cpp_t<Handle>>>(handle);
}

template <typename Handle, typename T>
    requires std::is_convertible_v<
        T *,
        std::add_pointer_t<std::add_const_t<std::remove_const_t<ku_c_handle_cpp_t<Handle>>>>>
[[nodiscard]] inline Handle toHandle(T *object) noexcept {
    static_assert(std::is_pointer_v<Handle>, "a kuai C handle must be an opaque pointer");
    using traits = ku_c_handle_traits<Handle>;
    using mapped_type = std::remove_const_t<typename traits::type>;
    if constexpr (traits::is_object) {
        static_assert(std::is_base_of_v<KuObject, mapped_type>,
                      "an object handle must map to an KuObject-derived C++ type");
    }
    const auto *mapped = static_cast<const mapped_type *>(object);
    return reinterpret_cast<Handle>(const_cast<mapped_type *>(mapped));
}

} // namespace capi
} // namespace kuai

#define KU_DEFINE_C_API_HANDLE(CPP_TYPE, C_HANDLE)                                             \
    static_assert(std::is_pointer_v<C_HANDLE>, #C_HANDLE " must be an opaque pointer handle"); \
    namespace kuai::capi {                                                                     \
    template <>                                                                                \
    struct ku_c_handle_traits<C_HANDLE> {                                                      \
        using type = CPP_TYPE;                                                                 \
        static constexpr bool is_object = false;                                               \
    };                                                                                         \
    }

#define KU_DEFINE_C_API_OBJECT_HANDLE(CPP_TYPE, C_HANDLE)                                      \
    static_assert(std::is_pointer_v<C_HANDLE>, #C_HANDLE " must be an opaque pointer handle"); \
    namespace kuai::capi {                                                                     \
    template <>                                                                                \
    struct ku_c_handle_traits<C_HANDLE> {                                                      \
        using type = CPP_TYPE;                                                                 \
        static constexpr bool is_object = true;                                                \
    };                                                                                         \
    }

KU_DEFINE_C_API_OBJECT_HANDLE(kuai::KuObject, ku_object_t)
KU_DEFINE_C_API_HANDLE(kuai::KuDevice, ku_device_t)
KU_DEFINE_C_API_HANDLE(kuai::KuFrameContext, ku_frame_ctx_t)
KU_DEFINE_C_API_HANDLE(kuai::KuInstance, ku_instance_t)
KU_DEFINE_C_API_HANDLE(kuai::KuCompletion, ku_completion_t)
