#pragma once

#include <cmath>

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuString.h>
#include <kuai/core/KuTensor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/algorithm/normal.h"

namespace kuai {

template <typename Vendor, typename T>
KuBuiltinResult generateNormal(KuVendorContext<Vendor> &device,
                               KuF64                    mean,
                               KuF64                    standardDeviation,
                               ku_size_t                count) {
    const auto generationCount = count + (count & ku_size_t{1});
    auto       result = device.template createTensor<T>(
        KuTensorDesc(KuDynamicExtents<1>(count), KuCapacity(generationCount)));
    if (!result.hasValue()) {
        return KuBuiltinError(result.error());
    }
    auto output = std::move(result).value();
    if (count != 0) {
        const auto status =
            algo::normal(device, output->template begin<T>(),
                         output->template begin<T>() + generationCount, mean, standardDeviation);
        if (status != KU_STATUS_SUCCESS) {
            return KuBuiltinError(status);
        }
    }
    return KuBuiltinResult(std::move(output));
}

template <typename Vendor>
KuBuiltinResult normalBuiltin(
    KuContext &context, KuF64 mean, KuF64 standardDeviation, KuI64 count, const KuString &dtype) {
    if (count < 0) {
        return KuBuiltinResult("normal count must be non-negative");
    }
    if (!std::isfinite(mean) || !std::isfinite(standardDeviation) || standardDeviation < 0.0) {
        return KuBuiltinResult(
            "normal mean must be finite and standard deviation must be finite and non-negative");
    }

    KuVendorContext<Vendor> device(context);
    const auto              outputCount = static_cast<ku_size_t>(count);
    if (dtype.value() == "f32") {
        return generateNormal<Vendor, KuF32>(device, mean, standardDeviation, outputCount);
    }
    if (dtype.value() == "f64") {
        return generateNormal<Vendor, KuF64>(device, mean, standardDeviation, outputCount);
    }
    return KuBuiltinResult("normal dtype must be \"f32\" or \"f64\"");
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_normal                         \
    KU_FUNC(m) {                                           \
        using namespace kuai;                              \
        m.def<&normalBuiltin<vendor::KuVendor>>("normal"); \
    }
