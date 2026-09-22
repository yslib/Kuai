#pragma once

#include <type_traits>
#include <utility>

#include <kuai/core/KuCore.h>

namespace kuai::vendor::detail {

struct ku_vendor_contract {};

template <typename Vendor, typename Context, typename... Args>
KU_ALWAYS_INLINE decltype(auto) ku_call_vendor(Context &&context, Args &&...args) {
    static_assert(std::is_base_of_v<ku_vendor_contract, Vendor>,
                  "vendor specialization must be declared with KU_DEFINE_VENDOR");
    return Vendor::call(std::forward<Context>(context), std::forward<Args>(args)...);
}

} // namespace kuai::vendor::detail

#define KU_DETAIL_VENDOR_TRAIT(name) KU_CONCAT(KU_CONCAT(ku_, name), _vendor)

// Defines the primary vendor trait and one backend specialization. Invoke this
// at global scope after backend::name has been declared.
#define KU_DEFINE_VENDOR(name, backend)                                                   \
    namespace kuai::vendor {                                                              \
    template <typename Vendor>                                                            \
    struct KU_DETAIL_VENDOR_TRAIT(name);                                                  \
    template <>                                                                           \
    struct KU_DETAIL_VENDOR_TRAIT(name)<backend::KuVendor> : detail::ku_vendor_contract { \
        template <typename... Args>                                                       \
        KU_ALWAYS_INLINE static decltype(auto) call(Args &&...args) {                     \
            return backend::name(std::forward<Args>(args)...);                            \
        }                                                                                 \
    };                                                                                    \
    }

// The context expression is evaluated only as the first argument to
// ku_call_vendor. Its type is inspected in an unevaluated decltype expression.
#define KU_CALL_VENDOR(name, context, ...)                                                  \
    ::kuai::vendor::detail::ku_call_vendor < ::kuai::vendor::KU_DETAIL_VENDOR_TRAIT(name) < \
        typename std::remove_cvref_t<decltype((context))>::vendor_type                      \
        >> ((context)__VA_OPT__(, ) __VA_ARGS__)
