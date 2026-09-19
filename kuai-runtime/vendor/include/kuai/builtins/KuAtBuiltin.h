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
#include "kuai/fn/KuViewFn.h"
#include "kuai/ktl/KuView.h"

namespace kuai {

template <typename Vendor, typename View>
KuBuiltinResult materializeAtView(const KuVendorContext<Vendor> &device, View sourceView) {
    using T = std::remove_cv_t<typename View::value_type>;

    const auto           extents = sourceView.extents();
    KuAllocFn<Vendor, T> alloc(device);
    auto                 storageResult = alloc(extents);
    if (!storageResult.hasValue()) {
        return KuBuiltinError(storageResult.error());
    }
    auto storage = std::move(storageResult).value();
    if (extents.prod() != 0) {
        KuCopyFn<Vendor> copy(device);
        copy(storage.data(), sourceView);
    }
    auto tensorResult = device.wrapTensor(std::move(storage), extents);
    if (!tensorResult.hasValue()) {
        return KuBuiltinError(tensorResult.error());
    }
    return KuBuiltinResult(ku_cast_sp<KuObject>(std::move(tensorResult).value()));
}

template <typename Vendor, typename SourceSpan>
    requires(SourceSpan::rank() >= 1 && SourceSpan::rank() <= 2)
KuBuiltinResult materializeAt(const KuVendorContext<Vendor> &device,
                              KuTensor                      &source,
                              bind::slice                    axes,
                              SourceSpan                     sourceSpan) {
    if constexpr (SourceSpan::rank() == 1) {
        return visitStaticIndexAxis<Vendor>(
            device, source.extent(0), axes[0], [&](auto axis) -> KuBuiltinResult {
                auto transform = AxisTransform(std::move(axis));
                return materializeAtView<Vendor>(device, KuView(sourceSpan, std::move(transform)));
            });
    } else {
        return visitStaticIndexAxis<Vendor>(
            device, source.extent(0), axes[0], [&](auto firstAxis) -> KuBuiltinResult {
                return visitStaticIndexAxis<Vendor>(
                    device, source.extent(1), axes[1], [&](auto secondAxis) -> KuBuiltinResult {
                        auto transform = AxisTransform(std::move(firstAxis), std::move(secondAxis));
                        return materializeAtView<Vendor>(device,
                                                         KuView(sourceSpan, std::move(transform)));
                    });
            });
    }
}

template <typename Vendor, typename SourceSpan>
    requires(SourceSpan::rank() >= 3 && SourceSpan::rank() <= KuTensor::MaxRank)
KuBuiltinResult materializeAt(const KuVendorContext<Vendor> &device,
                              KuTensor                      &source,
                              bind::slice                    axes,
                              SourceSpan                     sourceSpan) {
    constexpr std::size_t Rank = SourceSpan::rank();
    auto                  transform = makeIndexTransform<Rank>(device, source, axes);

    return materializeAtView<Vendor>(device, KuView(sourceSpan, std::move(transform)));
}

template <typename Vendor>
KuBuiltinResult atBuiltin(KuContext &context, KuTensor &source, bind::slice axes) {
    KuVendorContext<Vendor> device(context);

    if (source.rank() == 0 || source.rank() > KuTensor::MaxRank) {
        return KuBuiltinResult("at requires a tensor with rank from one through eight");
    }
    if (axes.size() != source.rank()) {
        return KuBuiltinResult("at requires exactly one index operand per source axis");
    }
    if (&source.getDevice() != device.m_deviceObject) {
        return KuBuiltinResult("at source must belong to the invocation device");
    }

    for (std::size_t axis = 0; axis < source.rank(); ++axis) {
        KuObject *const operand = axes[axis];
        if (operand == nullptr) {
            return KuBuiltinResult("at received a null axis object");
        }

        if (const auto *scalar = operand->as<KuScalar>(); scalar != nullptr) {
            if (scalar->getType() != KU_PRIMITIVE_NONE) {
                return KuBuiltinResult("at scalar axes must have NONE type");
            }
            continue;
        }

        if (const auto *slice = operand->as<KuSlice>(); slice != nullptr) {
            auto normalized = normalizeKuSlice(*slice, source.extent(axis));
            if (!normalized) {
                return KuBuiltinResult(std::move(normalized).error());
            }
            continue;
        }

        if (const auto *indices = operand->as<KuTensor>(); indices != nullptr) {
            if (indices->rank() != 1) {
                return KuBuiltinResult("at index tensors must have rank one");
            }
            if (indices->getType() != KU_PRIMITIVE_I32 && indices->getType() != KU_PRIMITIVE_I64) {
                return KuBuiltinResult("at index tensors must have I32 or I64 dtype");
            }
            if (&indices->getDevice() != device.m_deviceObject) {
                return KuBuiltinResult("at index tensors must belong to the invocation device");
            }
            continue;
        }

        return KuBuiltinResult("at axes must be NONE, slice, or an I32/I64 tensor");
    }

    return visitKuSpan<Vendor, Rs<1, 2, 3, 4, 5, 6, 7, 8>, KuNonVoidPrimitiveTs>(
        source, [&](auto sourceSpan) -> KuBuiltinResult {
            return materializeAt<Vendor>(device, source, axes, std::move(sourceSpan));
        });
}

} // namespace kuai

#define KU_GENERATE_BUILTIN_at                                                                 \
    KU_FUNC(m) {                                                                               \
        using namespace kuai;                                                                  \
        m.def<&atBuiltin<vendor::KuVendor>>("at", bind::arg("source"), bind::varargs("axes")); \
    }
