#pragma once

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuCore.h>
#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuLimits.h>
#include <kuai/ktl/functional/KuCast.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/fn/KuUnaryFn.h"
#include "kuai/fn/KuUnarySpanFn.h"

namespace kuai {
template <typename Vendor, typename T>
KuBuiltinResult ConversionBuiltin(KuContext &context, KuObject *in) {
    return KuUnaryFn<Vendor, KuNonVoidPrimitiveTs>()(context, *in,
                                                     KuUnarySpanFn(context, ku_cast<T>()));
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_bool                                     \
    KU_FUNC(m) {                                                     \
        using namespace kuai;                                        \
        m.def<&ConversionBuiltin<vendor::KuVendor, KuBool>>("bool"); \
    }

#define KU_GENERATE_BUILTIN_char                                      \
    KU_FUNC(m) {                                                      \
        using namespace kuai;                                         \
        m.def<&ConversionBuiltin<vendor::KuVendor, KuChar8>>("char"); \
    }

#define KU_GENERATE_BUILTIN_short                                    \
    KU_FUNC(m) {                                                     \
        using namespace kuai;                                        \
        m.def<&ConversionBuiltin<vendor::KuVendor, KuI16>>("short"); \
    }

#define KU_GENERATE_BUILTIN_int                                    \
    KU_FUNC(m) {                                                   \
        using namespace kuai;                                      \
        m.def<&ConversionBuiltin<vendor::KuVendor, KuI32>>("int"); \
    }

#define KU_GENERATE_BUILTIN_long                                    \
    KU_FUNC(m) {                                                    \
        using namespace kuai;                                       \
        m.def<&ConversionBuiltin<vendor::KuVendor, KuI64>>("long"); \
    }

#define KU_GENERATE_BUILTIN_float                                    \
    KU_FUNC(m) {                                                     \
        using namespace kuai;                                        \
        m.def<&ConversionBuiltin<vendor::KuVendor, KuF32>>("float"); \
    }

#define KU_GENERATE_BUILTIN_double                                    \
    KU_FUNC(m) {                                                      \
        using namespace kuai;                                         \
        m.def<&ConversionBuiltin<vendor::KuVendor, KuF64>>("double"); \
    }
