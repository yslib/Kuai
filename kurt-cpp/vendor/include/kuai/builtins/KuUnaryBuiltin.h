#pragma once

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/ktl/functional/KuACos.h>
#include <kuai/ktl/functional/KuACosh.h>
#include <kuai/ktl/functional/KuASin.h>
#include <kuai/ktl/functional/KuASinh.h>
#include <kuai/ktl/functional/KuATan.h>
#include <kuai/ktl/functional/KuATanh.h>
#include <kuai/ktl/functional/KuAbs.h>
#include <kuai/ktl/functional/KuCbrt.h>
#include <kuai/ktl/functional/KuCos.h>
#include <kuai/ktl/functional/KuCosh.h>
#include <kuai/ktl/functional/KuExp.h>
#include <kuai/ktl/functional/KuLog.h>
#include <kuai/ktl/functional/KuNeg.h>
#include <kuai/ktl/functional/KuNot.h>
#include <kuai/ktl/functional/KuReciprocal.h>
#include <kuai/ktl/functional/KuSin.h>
#include <kuai/ktl/functional/KuSinh.h>
#include <kuai/ktl/functional/KuSqrt.h>
#include <kuai/ktl/functional/KuTan.h>
#include <kuai/ktl/functional/KuTanh.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/core/KuTypes.h"
#include "kuai/fn/KuUnaryFn.h"
#include "kuai/fn/KuUnarySpanFn.h"

namespace kuai {

template <typename Vendor, typename UnaryOp>
KuBuiltinResult UnaryBuiltin(KuContext &context, KuObject *in) {
    return KuUnaryFn<Vendor, KuNonVoidPrimitiveTs>()(context, *in,
                                                     KuUnarySpanFn(context, UnaryOp()));
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_sqrt                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_sqrt>>("sqrt"); \
    }

#define KU_GENERATE_BUILTIN_abs                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_abs>>("abs"); \
    }

#define KU_GENERATE_BUILTIN_cbrt                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_cbrt>>("cbrt"); \
    }

#define KU_GENERATE_BUILTIN_exp                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_exp>>("exp"); \
    }

#define KU_GENERATE_BUILTIN_log                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_log>>("log"); \
    }

#define KU_GENERATE_BUILTIN_neg                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_neg>>("neg"); \
    }

#define KU_GENERATE_BUILTIN_not                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_not>>("not"); \
    }

#define KU_GENERATE_BUILTIN_reciprocal                                       \
    KU_FUNC(m) {                                                             \
        using namespace kuai;                                                \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_reciprocal>>("reciprocal"); \
    }

#define KU_GENERATE_BUILTIN_sin                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_sin>>("sin"); \
    }

#define KU_GENERATE_BUILTIN_cos                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_cos>>("cos"); \
    }

#define KU_GENERATE_BUILTIN_tan                                \
    KU_FUNC(m) {                                               \
        using namespace kuai;                                  \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_tan>>("tan"); \
    }

#define KU_GENERATE_BUILTIN_asin                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_asin>>("asin"); \
    }

#define KU_GENERATE_BUILTIN_acos                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_acos>>("acos"); \
    }

#define KU_GENERATE_BUILTIN_atan                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_atan>>("atan"); \
    }

#define KU_GENERATE_BUILTIN_sinh                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_sinh>>("sinh"); \
    }

#define KU_GENERATE_BUILTIN_cosh                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_cosh>>("cosh"); \
    }

#define KU_GENERATE_BUILTIN_tanh                                 \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_tanh>>("tanh"); \
    }

#define KU_GENERATE_BUILTIN_asinh                                  \
    KU_FUNC(m) {                                                   \
        using namespace kuai;                                      \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_asinh>>("asinh"); \
    }

#define KU_GENERATE_BUILTIN_acosh                                  \
    KU_FUNC(m) {                                                   \
        using namespace kuai;                                      \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_acosh>>("acosh"); \
    }

#define KU_GENERATE_BUILTIN_atanh                                  \
    KU_FUNC(m) {                                                   \
        using namespace kuai;                                      \
        m.def<&UnaryBuiltin<vendor::KuVendor, ku_atanh>>("atanh"); \
    }
