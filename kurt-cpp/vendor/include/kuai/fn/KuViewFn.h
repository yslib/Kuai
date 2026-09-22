#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <string>

#include <kuai/core/KuScalar.h>
#include <kuai/core/KuSlice.h>
#include <kuai/core/KuTensor.h>
#include <kuai/ktl/KuResult.h>
#include <kuai/ktl/KuView.h>
#include <kuai/vendor/KuDeviceAccessor.h>
#include <kuai/vendor/KuVendorContext.h>

namespace kuai {

struct KuNormalizedSlice {
    int64_t   m_offset;
    ku_size_t m_count;
    int64_t   m_stride;
};

namespace detail {

inline int64_t normalizePositiveBound(int64_t value, int64_t length) noexcept {
    if (value < 0) {
        return value < -length ? 0 : value + length;
    }
    return value > length ? length : value;
}

inline int64_t normalizeNegativeBound(int64_t value, int64_t length) noexcept {
    if (value < 0) {
        return value < -length ? -1 : value + length;
    }
    return value >= length ? length - 1 : value;
}

inline KuResultInfo<ku_size_t> checkedSliceCount(std::uint64_t count) {
    if (count > std::numeric_limits<ku_size_t>::max()) {
        return std::string("slice result extent exceeds ku_size_t");
    }
    return static_cast<ku_size_t>(count);
}

} // namespace detail

inline KuResultInfo<KuNormalizedSlice> normalizeKuSlice(const KuSlice &slice,
                                                        ku_size_t      axisExtent) {
    if (axisExtent > static_cast<ku_size_t>(std::numeric_limits<int64_t>::max())) {
        return std::string("slice source extent exceeds the int64_t range");
    }

    const auto             length = static_cast<int64_t>(axisExtent);
    const auto            &value = slice.value();
    const ku_slice_flags_t flags = value.flags;
    const int64_t          stride = (flags & KU_SLICE_HAS_STEP) != 0 ? value.step : 1;
    if (stride == 0) {
        return std::string("slice step must not be zero");
    }

    if (stride > 0) {
        const int64_t start = (flags & KU_SLICE_HAS_START) != 0
                                  ? detail::normalizePositiveBound(value.start, length)
                                  : 0;
        const int64_t stop = (flags & KU_SLICE_HAS_STOP) != 0
                                 ? detail::normalizePositiveBound(value.stop, length)
                                 : length;

        std::uint64_t count = 0;
        if (start < stop) {
            const auto distance = static_cast<std::uint64_t>(stop - start - 1);
            count = 1 + distance / static_cast<std::uint64_t>(stride);
        }
        auto checkedCount = detail::checkedSliceCount(count);
        if (!checkedCount) {
            return checkedCount.error();
        }
        return KuNormalizedSlice{start, checkedCount.value(), stride};
    }

    const int64_t start = (flags & KU_SLICE_HAS_START) != 0
                              ? detail::normalizeNegativeBound(value.start, length)
                              : length - 1;
    const int64_t stop =
        (flags & KU_SLICE_HAS_STOP) != 0 ? detail::normalizeNegativeBound(value.stop, length) : -1;
    const auto strideMagnitude = stride == std::numeric_limits<int64_t>::min()
                                     ? std::uint64_t{1} << 63
                                     : static_cast<std::uint64_t>(-stride);

    std::uint64_t count = 0;
    if (stop < start) {
        const auto distance = static_cast<std::uint64_t>(start - stop - 1);
        count = 1 + distance / strideMagnitude;
    }
    auto checkedCount = detail::checkedSliceCount(count);
    if (!checkedCount) {
        return checkedCount.error();
    }
    return KuNormalizedSlice{start, checkedCount.value(), stride};
}

template <typename Vendor, typename T>
auto makeStaticIndexSpan(KuTensor &indices) {
    using Extents = KuDynamicExtents<1>;
    using Accessor = KuDeviceAccessor<T, Vendor>;
    using Span = KuMdSpan<T, Extents, KuLayoutStride, Accessor>;

    const Extents                                     extents(indices.extent(0));
    const std::array<typename Extents::index_type, 1> strides{indices.stride(0)};
    const typename Span::mapping_type                 mapping(extents, strides);
    return Span(indices.template begin<T>(), mapping, Accessor{});
}

template <typename Vendor, typename Fn>
auto visitStaticIndexAxis(const KuVendorContext<Vendor> &device,
                          ku_size_t                      axisExtent,
                          KuObject                      *operand,
                          Fn &&fn) -> std::invoke_result_t<Fn &, AxisFullTransformFn> {
    KU_ASSERT(operand != nullptr, "a validated at axis must not be null");

    if (const auto *scalar = operand->as<KuScalar>(); scalar != nullptr) {
        KU_ASSERT(scalar->getType() == KU_PRIMITIVE_NONE,
                  "a validated at scalar axis must have NONE type");
        return std::invoke(fn, AxisFullTransformFn{});
    }

    if (const auto *slice = operand->as<KuSlice>(); slice != nullptr) {
        auto normalized = normalizeKuSlice(*slice, axisExtent);
        KU_ASSERT(normalized, "a validated at slice must normalize successfully");
        const auto &value = normalized.value();
        return std::invoke(fn,
                           KuAffineIndexTransform(value.m_offset, value.m_count, value.m_stride));
    }

    auto *indices = operand->as<KuTensor>();
    KU_ASSERT(indices != nullptr, "a validated at axis must be NONE, slice, or tensor");
    KU_ASSERT(indices->rank() == 1, "a validated at index tensor must have rank one");
    KU_ASSERT(indices->getType() == KU_PRIMITIVE_I32 || indices->getType() == KU_PRIMITIVE_I64,
              "a validated at index tensor must have I32 or I64 dtype");
    KU_ASSERT(&indices->getDevice() == device.m_deviceObject,
              "a validated at index tensor must belong to the invocation device");

    if (indices->getType() == KU_PRIMITIVE_I32) {
        return std::invoke(fn,
                           KuIndirectIndexTransform(makeStaticIndexSpan<Vendor, KuI32>(*indices)));
    }
    return std::invoke(fn, KuIndirectIndexTransform(makeStaticIndexSpan<Vendor, KuI64>(*indices)));
}

template <std::size_t Rank, typename Vendor>
KuIndexTransform<Rank> makeIndexTransform(const KuVendorContext<Vendor> &device,
                                          KuTensor                      &source,
                                          std::span<KuObject *const>     axes) {
    KU_ASSERT(axes.size() == Rank, "a validated at call must have one axis per source dimension");

    std::array<KuAxisIndex, Rank> descriptors{};
    for (std::size_t axis = 0; axis < Rank; ++axis) {
        KuObject *const operand = axes[axis];
        KU_ASSERT(operand != nullptr, "a validated at axis must not be null");

        if (const auto *scalar = operand->as<KuScalar>(); scalar != nullptr) {
            KU_ASSERT(scalar->getType() == KU_PRIMITIVE_NONE,
                      "a validated at scalar axis must have NONE type");
            descriptors[axis] = KuAxisIndex::affine(0, source.extent(axis), 1);
            continue;
        }

        if (const auto *slice = operand->as<KuSlice>(); slice != nullptr) {
            auto normalized = normalizeKuSlice(*slice, source.extent(axis));
            KU_ASSERT(normalized, "a validated at slice must normalize successfully");
            const auto &value = normalized.value();
            descriptors[axis] = KuAxisIndex::affine(value.m_offset, value.m_count, value.m_stride);
            continue;
        }

        const auto *indices = operand->as<KuTensor>();
        KU_ASSERT(indices != nullptr, "a validated at axis must be NONE, slice, or tensor");
        KU_ASSERT(indices->rank() == 1, "a validated at index tensor must have rank one");
        KU_ASSERT(indices->getType() == KU_PRIMITIVE_I32 || indices->getType() == KU_PRIMITIVE_I64,
                  "a validated at index tensor must have I32 or I64 dtype");
        KU_ASSERT(&indices->getDevice() == device.m_deviceObject,
                  "a validated at index tensor must belong to the invocation device");
        descriptors[axis] = KuAxisIndex::indirect(indices->data(), indices->extent(0),
                                                  indices->stride(0), indices->getType());
    }

    return KuIndexTransform<Rank>(std::move(descriptors));
}

} // namespace kuai
