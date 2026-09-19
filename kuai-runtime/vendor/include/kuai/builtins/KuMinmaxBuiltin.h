#pragma once

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/ktl/KuLimits.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/ktl/functional/KuMax.h>
#include <kuai/ktl/functional/KuMin.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/fn/KuBinaryFn.h"
#include "kuai/fn/KuBinarySpanFn.h"
#include "kuai/fn/KuReduceFn.h"

namespace kuai {

namespace {

template <typename Vendor, typename Fn>
class KuMinmaxBuiltin {
public:
    struct MapFn {
        template <typename T>
        KU_DEVICE_HOST auto operator()(const T &a) const {
            return a;
        }

        template <typename T>
        KU_DEVICE_HOST static auto initValue() {
            return kuai::ku_value_traits<T>::null();
        }
    };

    struct ReduceFn {
        template <typename T1, typename T2>
        KU_DEVICE_HOST auto operator()(const T1 &a, const T2 &b) const {
            static_assert(std::is_same_v<T1, T2>, "T1 and T2 must be the same type");
            if (ku_value_traits<decltype(b)>::isNull(b)
                || ku_value_traits<decltype(a)>::isNull(a)) {
                return ku_value_traits<decltype(b)>::isNull(b) ? a : b;
            } else {
                return Fn()(a, b);
            }
        }
    };

    KuBuiltinResult operator()(KuContext &context, KuObject &arg) {
        return KuReduceFn<Vendor, KuAllPrimitiveTs>()(context, arg, KuMinmaxBuiltin::MapFn(),
                                                      KuMinmaxBuiltin::ReduceFn());
    }

    KuBuiltinResult operator()(KuContext &context, KuObject &arg1, KuObject &arg2) {
        return KuBinaryFn<Vendor, KuNonVoidPrimitiveTs>(context)(arg1, arg2,
                                                                 KuBinarySpanFn(context, Fn()));
    }
};
} // namespace

template <typename Vendor>
inline KuBuiltinResult min(KuContext &context, KuObject &arg) {
    return KuMinmaxBuiltin<Vendor, ku_min>()(context, arg);
}

template <typename Vendor>
inline KuBuiltinResult min(KuContext &context, KuObject &arg1, KuObject &arg2) {
    return KuMinmaxBuiltin<Vendor, ku_min>()(context, arg1, arg2);
}

template <typename Vendor>
inline KuBuiltinResult max(KuContext &context, KuObject &arg) {
    return KuMinmaxBuiltin<Vendor, ku_max>()(context, arg);
}

template <typename Vendor>
inline KuBuiltinResult max(KuContext &context, KuObject &arg1, KuObject &arg2) {
    return KuMinmaxBuiltin<Vendor, ku_max>()(context, arg1, arg2);
}

namespace detail {

template <typename Vendor>
using ku_unary_min_handler_t = KuBuiltinResult (*)(KuContext &, KuObject &);

template <typename Vendor>
using ku_binary_min_handler_t = KuBuiltinResult (*)(KuContext &, KuObject &, KuObject &);

template <typename Vendor>
using ku_unary_max_handler_t = KuBuiltinResult (*)(KuContext &, KuObject &);

template <typename Vendor>
using ku_binary_max_handler_t = KuBuiltinResult (*)(KuContext &, KuObject &, KuObject &);

// NVCC 12.9 can lose the type selected by overload_cast when its call expression is used directly
// as an auto NTTP. Naming the explicitly typed pointer preserves overload selection for def<Fn>.
template <typename Vendor>
inline constexpr ku_unary_min_handler_t<Vendor> ku_unary_min_handler =
    bind::overload_cast<KuContext &, KuObject &>(&min<Vendor>);

template <typename Vendor>
inline constexpr ku_binary_min_handler_t<Vendor> ku_binary_min_handler =
    bind::overload_cast<KuContext &, KuObject &, KuObject &>(&min<Vendor>);

template <typename Vendor>
inline constexpr ku_unary_max_handler_t<Vendor> ku_unary_max_handler =
    bind::overload_cast<KuContext &, KuObject &>(&max<Vendor>);

template <typename Vendor>
inline constexpr ku_binary_max_handler_t<Vendor> ku_binary_max_handler =
    bind::overload_cast<KuContext &, KuObject &, KuObject &>(&max<Vendor>);

} // namespace detail

} // namespace kuai

#define KU_GENERATE_BUILTIN_min                                        \
    KU_FUNC(m) {                                                       \
        using namespace kuai;                                          \
        m.def<detail::ku_unary_min_handler<vendor::KuVendor>>("min");  \
        m.def<detail::ku_binary_min_handler<vendor::KuVendor>>("min"); \
    }

#define KU_GENERATE_BUILTIN_max                                        \
    KU_FUNC(m) {                                                       \
        using namespace kuai;                                          \
        m.def<detail::ku_unary_max_handler<vendor::KuVendor>>("max");  \
        m.def<detail::ku_binary_max_handler<vendor::KuVendor>>("max"); \
    }
