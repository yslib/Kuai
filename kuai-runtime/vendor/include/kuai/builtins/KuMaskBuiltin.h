#pragma once

#include <type_traits>
#include <utility>

#include <kuai/bind/KuRegistration.h>
#include <kuai/builtins/KuBuiltin.h>
#include <kuai/core/KuTensor.h>
#include <kuai/vendor/KuObjectView.h>
#include <kuai/vendor/KuTypeVisit.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/fn/KuAllocFn.h"
#include "kuai/fn/KuCopyFn.h"
#include "kuai/ktl/KuView.h"

namespace kuai {

template <typename Vendor>
KuBuiltinResult maskBuiltin(KuContext &context, KuTensor &source, KuTensor &stencil) {
    if (source.rank() == 0 || source.rank() > KuTensor::MaxRank) {
        return KuBuiltinResult("mask requires tensors with rank from one through eight");
    }
    if (stencil.getType() != KU_PRIMITIVE_BOOLEAN) {
        return KuBuiltinResult("mask stencil must have BOOLEAN dtype");
    }
    if (source.rank() != stencil.rank()) {
        return KuBuiltinResult("mask source and stencil must have identical shapes");
    }
    for (std::size_t axis = 0; axis < source.rank(); ++axis) {
        if (source.extent(axis) != stencil.extent(axis)) {
            return KuBuiltinResult("mask source and stencil must have identical shapes");
        }
    }
    if (&source.getDevice() != &context.getDevice()
        || &stencil.getDevice() != &context.getDevice()) {
        return KuBuiltinResult("mask tensors must belong to the invocation device");
    }

    KuVendorContext<Vendor> device(context);
    return visitKuSpan<Vendor, Rs<1, 2, 3, 4, 5, 6, 7, 8>, KuNonVoidPrimitiveTs>(
        source, [&](auto sourceSpan) -> KuBuiltinResult {
            using SourceSpan = decltype(sourceSpan);
            using T = std::remove_cv_t<typename SourceSpan::value_type>;
            constexpr std::size_t Rank = SourceSpan::rank();

            auto                 stencilSpan = makeKuSpan<Vendor, KuBool, Rank>(stencil);
            auto                 sourceView = KuView(sourceSpan, MaskTransform(stencilSpan));
            const auto           extents = sourceView.extents();
            KuAllocFn<Vendor, T> alloc(context);
            auto                 storageResult = alloc(extents);
            if (!storageResult.hasValue()) {
                return KuBuiltinError(storageResult.error());
            }
            auto storage = std::move(storageResult).value();
            if (extents.prod() != 0) {
                KuCopyFn<Vendor> copy(context);
                copy(storage.data(), sourceView);
            }
            auto tensorResult = device.wrapTensor(std::move(storage), extents);
            if (!tensorResult.hasValue()) {
                return KuBuiltinError(tensorResult.error());
            }
            return KuBuiltinResult(ku_cast_sp<KuObject>(std::move(tensorResult).value()));
        });
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_mask                       \
    KU_FUNC(m) {                                       \
        using namespace kuai;                          \
        m.def<&maskBuiltin<vendor::KuVendor>>("mask"); \
    }
