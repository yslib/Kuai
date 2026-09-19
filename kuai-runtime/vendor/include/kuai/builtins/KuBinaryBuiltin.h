#pragma once
#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuTypes.h>
#include <kuai/ktl/functional/KuAdd.h>
#include <kuai/ktl/functional/KuAnd.h>
#include <kuai/ktl/functional/KuBitAnd.h>
#include <kuai/ktl/functional/KuBitOr.h>
#include <kuai/ktl/functional/KuBitXor.h>
#include <kuai/ktl/functional/KuDivide.h>
#include <kuai/ktl/functional/KuEq.h>
#include <kuai/ktl/functional/KuGe.h>
#include <kuai/ktl/functional/KuGt.h>
#include <kuai/ktl/functional/KuLShift.h>
#include <kuai/ktl/functional/KuLe.h>
#include <kuai/ktl/functional/KuLt.h>
#include <kuai/ktl/functional/KuMod.h>
#include <kuai/ktl/functional/KuMul.h>
#include <kuai/ktl/functional/KuNe.h>
#include <kuai/ktl/functional/KuOr.h>
#include <kuai/ktl/functional/KuPow.h>
#include <kuai/ktl/functional/KuRShift.h>
#include <kuai/ktl/functional/KuRatio.h>
#include <kuai/ktl/functional/KuSub.h>
#include <kuai/ktl/functional/KuXor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/fn/KuBinaryFn.h"
#include "kuai/fn/KuBinarySpanFn.h"

namespace kuai {
template <typename Vendor, typename BinaryOp, typename TypeSeq = KuNonVoidPrimitiveTs>
KuBuiltinResult BinaryBuiltin(KuContext &context, KuObject *lh, KuObject *rh) {
    return KuBinaryFn<Vendor, TypeSeq>(context).template operator()(
        *lh, *rh, KuBinarySpanFn(context, BinaryOp()));
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_add                                 \
    KU_FUNC(m) {                                                \
        using namespace kuai;                                   \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_add>>("add"); \
    }

#define KU_GENERATE_BUILTIN_sub                                 \
    KU_FUNC(m) {                                                \
        using namespace kuai;                                   \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_sub>>("sub"); \
    }

#define KU_GENERATE_BUILTIN_mul                                 \
    KU_FUNC(m) {                                                \
        using namespace kuai;                                   \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_mul>>("mul"); \
    }

#define KU_GENERATE_BUILTIN_div                                    \
    KU_FUNC(m) {                                                   \
        using namespace kuai;                                      \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_divide>>("div"); \
    }

#define KU_GENERATE_BUILTIN_ratio                                   \
    KU_FUNC(m) {                                                    \
        using namespace kuai;                                       \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_ratio>>("ratio"); \
    }

#define KU_GENERATE_BUILTIN_mod                                 \
    KU_FUNC(m) {                                                \
        using namespace kuai;                                   \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_mod>>("mod"); \
    }

#define KU_GENERATE_BUILTIN_and                                 \
    KU_FUNC(m) {                                                \
        using namespace kuai;                                   \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_and>>("and"); \
    }

#define KU_GENERATE_BUILTIN_or                                \
    KU_FUNC(m) {                                              \
        using namespace kuai;                                 \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_or>>("or"); \
    }

#define KU_GENERATE_BUILTIN_xor                                 \
    KU_FUNC(m) {                                                \
        using namespace kuai;                                   \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_xor>>("xor"); \
    }

#define KU_GENERATE_BUILTIN_bitAnd                                     \
    KU_FUNC(m) {                                                       \
        using namespace kuai;                                          \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_bit_and>>("bitAnd"); \
    }

#define KU_GENERATE_BUILTIN_bitOr                                    \
    KU_FUNC(m) {                                                     \
        using namespace kuai;                                        \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_bit_or>>("bitOr"); \
    }

#define KU_GENERATE_BUILTIN_bitXor                                     \
    KU_FUNC(m) {                                                       \
        using namespace kuai;                                          \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_bit_xor>>("bitXor"); \
    }

#define KU_GENERATE_BUILTIN_eq                                \
    KU_FUNC(m) {                                              \
        using namespace kuai;                                 \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_eq>>("eq"); \
    }

#define KU_GENERATE_BUILTIN_ne                                \
    KU_FUNC(m) {                                              \
        using namespace kuai;                                 \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_ne>>("ne"); \
    }

#define KU_GENERATE_BUILTIN_ge                                \
    KU_FUNC(m) {                                              \
        using namespace kuai;                                 \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_ge>>("ge"); \
    }

#define KU_GENERATE_BUILTIN_gt                                \
    KU_FUNC(m) {                                              \
        using namespace kuai;                                 \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_gt>>("gt"); \
    }

#define KU_GENERATE_BUILTIN_le                                \
    KU_FUNC(m) {                                              \
        using namespace kuai;                                 \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_le>>("le"); \
    }

#define KU_GENERATE_BUILTIN_lt                                \
    KU_FUNC(m) {                                              \
        using namespace kuai;                                 \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_lt>>("lt"); \
    }

#define KU_GENERATE_BUILTIN_pow                                 \
    KU_FUNC(m) {                                                \
        using namespace kuai;                                   \
        m.def<&BinaryBuiltin<vendor::KuVendor, ku_pow>>("pow"); \
    }
