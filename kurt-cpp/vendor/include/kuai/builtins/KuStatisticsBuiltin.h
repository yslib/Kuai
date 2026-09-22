#pragma once

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuCore.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/core/KuScalar.h>
#include <kuai/ktl/KuValueTraits.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/builtins/states/KurtosisStates.h"
#include "kuai/builtins/states/MeanStates.h"
#include "kuai/builtins/states/SkewStates.h"
#include "kuai/builtins/states/StdStates.h"
#include "kuai/builtins/states/VarStates.h"
#include "kuai/fn/KuReduceFn.h"
namespace kuai {

template <typename Vendor>
inline KuBuiltinResult MeanBuiltin(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>{}(context, *arg, MeanMapFn{}, MeanReduceFn{},
                                                      MeanResultCastFn{});
}

template <typename Vendor>
inline KuBuiltinResult std(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>{}(context, *arg, StdMapFn{}, StdReduceFn{},
                                                      StdResultCastFn{});
}

template <typename Vendor>
inline KuBuiltinResult stdp(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>{}(context, *arg, StdMapFn{}, StdReduceFn{},
                                                      PopulationStdResultCastFn{});
}

template <typename Vendor>
inline KuBuiltinResult var(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>{}(context, *arg, VarMapFn{}, VarReduceFn{},
                                                      VarResultCastFn{});
}

template <typename Vendor>
inline KuBuiltinResult varp(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>{}(context, *arg, VarMapFn{}, VarReduceFn{},
                                                      PopulationVarResultCastFn{});
}

template <typename Vendor>
KuBuiltinResult KuSkewBuiltin(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>{}(context, *arg, SkewMapFn{}, SkewReduceFn{},
                                                      SkewResultWithUnbiasCastFn{});
}

template <typename Vendor>
KuBuiltinResult KuKurtosisBuiltin(KuContext &context, KuObject *arg) {
    return KuReduceFn<Vendor, KuNonVoidPrimitiveTs>{}(
        context, *arg, KurtosisMapFn{}, KurtosisReduceFn{}, UnbiasedKurtosisResultFn{});
}
} // namespace kuai

#define KU_GENERATE_BUILTIN_avg                       \
    KU_FUNC(m) {                                      \
        using namespace kuai;                         \
        m.def<&MeanBuiltin<vendor::KuVendor>>("avg"); \
    }

#define KU_GENERATE_BUILTIN_std               \
    KU_FUNC(m) {                              \
        using namespace kuai;                 \
        m.def<&std<vendor::KuVendor>>("std"); \
    }

#define KU_GENERATE_BUILTIN_var               \
    KU_FUNC(m) {                              \
        using namespace kuai;                 \
        m.def<&var<vendor::KuVendor>>("var"); \
    }

#define KU_GENERATE_BUILTIN_skew                         \
    KU_FUNC(m) {                                         \
        using namespace kuai;                            \
        m.def<&KuSkewBuiltin<vendor::KuVendor>>("skew"); \
    }

#define KU_GENERATE_BUILTIN_kurtosis                             \
    KU_FUNC(m) {                                                 \
        using namespace kuai;                                    \
        m.def<&KuKurtosisBuiltin<vendor::KuVendor>>("kurtosis"); \
    }
