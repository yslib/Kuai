#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <kuai/ktl/KuMdSpan.h>
#include <kuai/ktl/KuValueTraits.h>

#include "kuai/core/KuCore.h"
#include "kuai/core/KuTypes.h"
#include "kuai/ktl/functional/KuCast.h"

namespace kuai {

using Dim0 = KuExtents<size_t>;
using Dim1 = KuExtents<size_t, dynamic_extent>;
using Dim2 = KuExtents<size_t, dynamic_extent, dynamic_extent>;

struct CompactionView_t {};
struct TransposeView_t {};

template <std::size_t I, typename U, std::size_t N, std::size_t... Idx>
static constexpr auto
array_replace_impl(const std::array<U, N> &arr, const U &v, std::index_sequence<Idx...>) {
    return std::array<U, N>{((Idx == I) ? v : arr[Idx])...};
}

template <std::size_t I, typename U, std::size_t N>
static constexpr auto array_replace(const std::array<U, N> &arr, const U &v) {
    static_assert(I < N, "Index out of range");
    return array_replace_impl<I>(arr, v, std::make_index_sequence<N>{});
}

template <typename OffsetT, typename ExtentT, typename StrideT>
struct AffineTransformFn {
public:
    using slice_type = StridedSlice<OffsetT, ExtentT, StrideT>;
    using size_type = size_t;
    KU_DEVICE_HOST AffineTransformFn(StridedSlice<OffsetT, ExtentT, StrideT> slice)
        : m_slice(std::move(slice)) {
    }

    template <typename Idx>
    KU_DEVICE_HOST constexpr void operator()(Idx &idx) const noexcept {
        idx = ku_cast<Idx>()(m_slice.m_offset + ku_cast<size_type>()(idx) * m_slice.m_stride);
    }

    template <typename Idx>
    KU_DEVICE_HOST bool at(Idx &idx) const noexcept {
        return idx < m_slice.m_extent ? (this->operator()(idx), true) : false;
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t r) const noexcept {
        (void)r;
        return 1 + (m_slice.m_extent - 1) / m_slice.m_stride;
    }

    KU_DEVICE_HOST constexpr void flat(size_type flat_idx, size_type &multi_idx) const noexcept {
        multi_idx = flat_idx;
    }

private:
    slice_type m_slice;
};

template <typename OffsetT, typename ExtentT, typename StrideT>
AffineTransformFn(StridedSlice<OffsetT, ExtentT, StrideT>)
    -> AffineTransformFn<OffsetT, ExtentT, StrideT>;

struct AxisFullTransformFn {
public:
    using size_type = size_t;
    KU_DEVICE_HOST explicit AxisFullTransformFn() {
    }

    template <typename Idx>
    KU_DEVICE_HOST constexpr void operator()(Idx &idx) const noexcept {
    }

    template <typename Idx>
    KU_DEVICE_HOST constexpr bool at(Idx &idx) const noexcept {
        return true;
    }
    KU_DEVICE_HOST constexpr size_type extent(std::size_t r) const noexcept {
        return ku_npos;
    }

    KU_DEVICE_HOST constexpr void flat(size_type flat_idx, size_type &multi_idx) const noexcept {
        // TODO::
        multi_idx = flat_idx;
    }
};

template <typename T, typename L, typename A>
struct IndirectTransformFn {
public:
    using Span = KuMdSpan<T, Dim1, L, A>;
    using size_type = typename Span::size_type;
    using value_type = T;
    KU_DEVICE_HOST IndirectTransformFn(Span span) : m_span(std::move(span)) {
    }

    template <typename Idx>
    KU_DEVICE_HOST constexpr void operator()(Idx &idx) const noexcept {
        idx = ku_cast<Idx>()(m_span(ku_cast<ku_size_t>()(idx)));
    }

    template <typename Idx>
    KU_DEVICE_HOST bool at(Idx &idx) const noexcept {
        return idx < m_span.extent(0) ? (this->operator()(idx), true) : false;
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t r) const noexcept {
        (void)r;
        return m_span.extent(0);
    }

    KU_DEVICE_HOST constexpr void flat(size_type flat_idx, size_t &multi_idx) const noexcept {
        // optimized
        std::array<size_type, 1> multi_idx_arr{};
        m_span.mapping().flat_to_multi_index(flat_idx, multi_idx_arr);
        multi_idx = multi_idx_arr[0];
    }

private:
    Span m_span;
};

template <typename T, typename L, typename A>
IndirectTransformFn(KuMdSpan<T, Dim1, L, A>) -> IndirectTransformFn<T, L, A>;

template <typename E, typename L, typename A>
struct MaskTransform {
public:
    static constexpr bool hasCompaction = false;
    // static_assert(Rank == E::rank(), "The rank of the mask must match the Rank");
    using MaskSpan = KuMdSpan<KuBool, E, L, A>;
    static constexpr size_t Rank = MaskSpan::rank();
    using size_type = typename MaskSpan::size_type;

    KU_DEVICE_HOST MaskTransform(MaskSpan span) : m_stencil(std::move(span)) {
    }

    template <typename... Idx>
    KU_DEVICE_HOST void operator()(Idx &...idx) const noexcept {
        ku_value_traits<decltype(m_stencil(idx...))>::repr(m_stencil(idx...)) == KU_BOOL_TRUE
            ? void()
            : (..., (idx = ku_npos, void()));
    }

    template <typename... Idx>
    KU_DEVICE_HOST bool at(Idx &...idx) const noexcept {
        return checkBounds(idx..., std::make_index_sequence<Rank>{})
                   ? (this->operator()(idx...), true)
                   : false; // out of bound is considered as false
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t r) const noexcept {
        return m_stencil.extent(r);
    }

    KU_DEVICE_HOST constexpr auto extents() const noexcept {
        return m_stencil.mapping().extents();
    }

    KU_DEVICE_HOST constexpr void flat(size_type                    flat_idx,
                                       std::array<size_type, Rank> &multi_idx) const noexcept {
        m_stencil.mapping().flat_to_multi_index(flat_idx, multi_idx);
    }

private:
    template <typename... Idx, size_t... I>
    bool checkBounds(Idx... idx, std::index_sequence<I...>) const noexcept {
        auto args_tuple = std::forward_as_tuple(idx...);
        return (... && (std::get<I>(args_tuple) < m_stencil.extent(I)));
    }
    MaskSpan m_stencil;
};

namespace detail {

template <typename Idx>
KU_DEVICE_HOST bool
mapAffineIndex(int64_t offsetValue, int64_t strideValue, ku_size_t position, Idx &idx) noexcept {
    static_assert(sizeof(ku_size_t) <= sizeof(std::uint64_t));
    if (offsetValue < 0 || strideValue == 0) {
        idx = static_cast<Idx>(ku_npos);
        return false;
    }

    const auto    offset = static_cast<std::uint64_t>(offsetValue);
    const auto    logicalPosition = static_cast<std::uint64_t>(position);
    std::uint64_t mapped = 0;
    if (strideValue > 0) {
        const auto stride = static_cast<std::uint64_t>(strideValue);
        if (logicalPosition > (std::numeric_limits<std::uint64_t>::max() - offset) / stride) {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }
        mapped = offset + logicalPosition * stride;
    } else {
        const auto stride = strideValue == std::numeric_limits<int64_t>::min()
                                ? std::uint64_t{1} << 63
                                : static_cast<std::uint64_t>(-strideValue);
        if (logicalPosition > std::numeric_limits<std::uint64_t>::max() / stride) {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }
        const auto distance = logicalPosition * stride;
        if (distance > offset) {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }
        mapped = offset - distance;
    }

    if (mapped > std::numeric_limits<ku_size_t>::max()) {
        idx = static_cast<Idx>(ku_npos);
        return false;
    }
    idx = static_cast<Idx>(mapped);
    return true;
}

template <typename Idx>
KU_DEVICE_HOST bool mapIndirectIndex(ku_i64_t value, Idx &idx) noexcept {
    static_assert(sizeof(ku_size_t) <= sizeof(std::uint64_t));
    if (value < 0
        || static_cast<std::uint64_t>(value)
               > static_cast<std::uint64_t>(std::numeric_limits<ku_size_t>::max())) {
        idx = static_cast<Idx>(ku_npos);
        return false;
    }
    idx = static_cast<Idx>(static_cast<ku_size_t>(value));
    return true;
}

} // namespace detail

class KuAffineIndexTransform {
public:
    using size_type = ku_size_t;

    KU_HOST KuAffineIndexTransform(int64_t offset, ku_size_t count, int64_t stride) noexcept
        : m_offset(offset), m_count(count), m_stride(stride) {
    }

    template <typename Idx>
    KU_DEVICE_HOST void operator()(Idx &idx) const noexcept {
        static_cast<void>(map(idx));
    }

    template <typename Idx>
    KU_DEVICE_HOST bool at(Idx &idx) const noexcept {
        return map(idx);
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t axis) const noexcept {
        static_cast<void>(axis);
        return m_count;
    }

    KU_DEVICE_HOST constexpr void flat(size_type flatIndex, size_type &multiIndex) const noexcept {
        multiIndex = flatIndex;
    }

private:
    template <typename Idx>
    KU_DEVICE_HOST bool map(Idx &idx) const noexcept {
        const auto position = static_cast<ku_size_t>(idx);
        if (position >= m_count) {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }
        return detail::mapAffineIndex(m_offset, m_stride, position, idx);
    }

    int64_t   m_offset;
    ku_size_t m_count;
    int64_t   m_stride;
};

static_assert(std::is_trivially_copyable_v<KuAffineIndexTransform>);

template <typename Span>
class KuIndirectIndexTransform {
public:
    using value_type = std::remove_cv_t<typename Span::value_type>;
    using size_type = typename Span::size_type;

    static_assert(std::is_same_v<value_type, KuI32> || std::is_same_v<value_type, KuI64>);

    KU_HOST explicit KuIndirectIndexTransform(Span span) noexcept : m_span(std::move(span)) {
    }

    template <typename Idx>
    KU_DEVICE_HOST void operator()(Idx &idx) const noexcept {
        static_cast<void>(map(idx));
    }

    template <typename Idx>
    KU_DEVICE_HOST bool at(Idx &idx) const noexcept {
        return map(idx);
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t axis) const noexcept {
        static_cast<void>(axis);
        return m_span.extent(0);
    }

    KU_DEVICE_HOST constexpr void flat(size_type flatIndex, size_type &multiIndex) const noexcept {
        std::array<size_type, 1> index{};
        m_span.mapping().flat_to_multi_index(flatIndex, index);
        multiIndex = index[0];
    }

private:
    template <typename Idx>
    KU_DEVICE_HOST bool map(Idx &idx) const noexcept {
        const auto position = static_cast<size_type>(idx);
        if (position >= m_span.extent(0)) {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }
        return detail::mapIndirectIndex(static_cast<ku_i64_t>(m_span(position)), idx);
    }

    Span m_span;
};

struct KuAxisIndex {
public:
    KU_HOST static KuAxisIndex affine(int64_t offset, ku_size_t count, int64_t stride) noexcept {
        KuAxisIndex result;
        result.m_count = count;
        result.m_offset = offset;
        result.m_affineStride = stride;
        return result;
    }

    KU_HOST static KuAxisIndex indirect(const void         *data,
                                        ku_size_t           count,
                                        ku_size_t           elementStride,
                                        ku_primitive_type_t type) noexcept {
        KuAxisIndex result;
        result.m_data = data;
        result.m_count = count;
        result.m_elementStride = elementStride;
        result.m_type = type;
        return result;
    }

    KU_DEVICE_HOST constexpr ku_size_t extent() const noexcept {
        return m_count;
    }

    template <typename Idx>
    KU_DEVICE_HOST bool map(Idx &idx) const noexcept {
        const ku_size_t position = static_cast<ku_size_t>(idx);
        if (position >= m_count) {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }

        if (m_type == KU_PRIMITIVE_NONE) {
            return mapAffine(position, idx);
        }
        return mapIndirect(position, idx);
    }

private:
    template <typename Idx>
    KU_DEVICE_HOST bool mapAffine(ku_size_t position, Idx &idx) const noexcept {
        return detail::mapAffineIndex(m_offset, m_affineStride, position, idx);
    }

    template <typename Idx>
    KU_DEVICE_HOST bool mapIndirect(ku_size_t position, Idx &idx) const noexcept {
        if (m_elementStride != 0
            && position > std::numeric_limits<ku_size_t>::max() / m_elementStride) {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }
        const ku_size_t offset = position * m_elementStride;

        ku_i64_t value = -1;
        if (m_type == KU_PRIMITIVE_I32) {
            value = static_cast<const KuI32 *>(m_data)[offset];
        } else if (m_type == KU_PRIMITIVE_I64) {
            value = static_cast<const KuI64 *>(m_data)[offset];
        } else {
            idx = static_cast<Idx>(ku_npos);
            return false;
        }

        return detail::mapIndirectIndex(value, idx);
    }

    const void         *m_data = nullptr;
    ku_size_t           m_count = 0;
    ku_size_t           m_elementStride = 0;
    int64_t             m_offset = 0;
    int64_t             m_affineStride = 1;
    ku_primitive_type_t m_type = KU_PRIMITIVE_NONE;
};

static_assert(std::is_trivially_copyable_v<KuAxisIndex>);

template <std::size_t Rank>
class KuIndexTransform {
public:
    static constexpr bool hasCompaction = false;
    using size_type = ku_size_t;

    KU_HOST explicit KuIndexTransform(std::array<KuAxisIndex, Rank> axes) noexcept
        : m_axes(std::move(axes)) {
    }

    template <typename... Idx>
    KU_DEVICE_HOST void operator()(Idx &...idx) const noexcept {
        static_assert(sizeof...(Idx) == Rank);
        map(std::index_sequence_for<Idx...>{}, idx...);
    }

    template <typename... Idx>
    KU_DEVICE_HOST bool at(Idx &...idx) const noexcept {
        static_assert(sizeof...(Idx) == Rank);
        return mapChecked(std::index_sequence_for<Idx...>{}, idx...);
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t axis) const noexcept {
        return m_axes[axis].extent();
    }

    KU_DEVICE_HOST constexpr auto extents() const noexcept {
        std::array<size_type, Rank> values{};
        for (std::size_t axis = 0; axis < Rank; ++axis) {
            values[axis] = m_axes[axis].extent();
        }
        return KuDynamicExtents<Rank>(values);
    }

    KU_DEVICE_HOST constexpr void flat(size_type                    flatIndex,
                                       std::array<size_type, Rank> &multiIndex) const noexcept {
        for (std::size_t axis = 0; axis < Rank; ++axis) {
            multiIndex[axis] = flatIndex % m_axes[axis].extent();
            flatIndex /= m_axes[axis].extent();
        }
    }

private:
    template <typename... Idx, std::size_t... Indices>
    KU_DEVICE_HOST void map(std::index_sequence<Indices...>, Idx &...idx) const noexcept {
        auto values = std::forward_as_tuple(idx...);
        (..., static_cast<void>(m_axes[Indices].map(std::get<Indices>(values))));
    }

    template <typename... Idx, std::size_t... Indices>
    KU_DEVICE_HOST bool mapChecked(std::index_sequence<Indices...>, Idx &...idx) const noexcept {
        auto values = std::forward_as_tuple(idx...);
        return (... && m_axes[Indices].map(std::get<Indices>(values)));
    }

    std::array<KuAxisIndex, Rank> m_axes;
};

template <typename... Fn>
struct AxisTransform {
public:
    static_assert(
        ((ku_is_instance_of_v<Fn, IndirectTransformFn> || ku_is_instance_of_v<Fn, AffineTransformFn>
          || std::is_same_v<Fn, AxisFullTransformFn> || std::is_same_v<Fn, KuAffineIndexTransform>
          || ku_is_instance_of_v<Fn, KuIndirectIndexTransform>
          || ku_is_instance_of_v<Fn, MaskTransform>)
         && ...),
        "AxisTransform: each Fn must be one of IndirectTransformFn, AffineTransformFn, "
        "AxisFullTransformFn, KuAffineIndexTransform, KuIndirectIndexTransform, or "
        "MaskTransform");

    static constexpr bool hasCompaction =
        (ku_is_instance_of_v<Fn, MaskTransform> || ...); // whether any of the Fn is Mask

    static constexpr size_t Rank = sizeof...(Fn);
    using size_type = size_t;
    AxisTransform(Fn... fns) : m_fns(std::make_tuple(std::move(fns)...)) {
    }

    template <typename... Idx>
    KU_DEVICE_HOST void operator()(Idx &...idx) const noexcept {
        mapImpl(std::make_index_sequence<Rank>{}, idx...);
    }

    template <typename... Idx>
    KU_DEVICE_HOST bool at(Idx &...idx) const noexcept {
        return atImpl(std::make_index_sequence<Rank>{}, idx...);
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t r) const noexcept {
        return extentImpl(r, std::make_index_sequence<Rank>{});
    }

    KU_DEVICE_HOST constexpr decltype(auto) extents() const noexcept {
        return extentsImpl(std::make_index_sequence<Rank>{});
    }

    KU_DEVICE_HOST constexpr void flat(size_type                    flat_idx,
                                       std::array<size_type, Rank> &multi_idx) const noexcept {
        flatToMultiIndexImpl(flat_idx, multi_idx, std::make_index_sequence<Rank>{});
    }

private:
    template <typename... Idx, size_t... I>
    KU_DEVICE_HOST constexpr void mapImpl(std::index_sequence<I...>, Idx &...idx) const noexcept {
        auto args_tuple = std::forward_as_tuple(idx...);
        (..., std::get<I>(m_fns)(std::get<I>(args_tuple)));
    }

    template <size_t... I>
    KU_DEVICE_HOST constexpr auto extentsImpl(std::index_sequence<I...>) const noexcept {
        return KuExtents(std::array<size_type, Rank>{(std::get<I>(m_fns).extent(0))...});
    }

    template <size_t... I>
    KU_DEVICE_HOST constexpr auto flatToMultiIndexImpl(size_type                    flat_idx,
                                                       std::array<size_type, Rank> &multi_idx,
                                                       std::index_sequence<I...>) const noexcept {
        (..., std::get<I>(m_fns).flat(flat_idx, multi_idx[I]));
    }

    template <size_t... I>
    KU_DEVICE_HOST constexpr size_type
    multiToFlatIndexImpl(const std::array<size_type, Rank> &multi_idx,
                         std::index_sequence<I...>) const noexcept {
        return ku_npos;
    }

    template <typename... Idx, size_t... I>
    KU_DEVICE_HOST bool atImpl(std::index_sequence<I...>, Idx &...idx) const noexcept {
        auto args_tuple = std::forward_as_tuple(idx...);
        return (... && (std::get<I>(m_fns).at(std::get<I>(args_tuple))));
    }

    template <size_t... I>
    KU_DEVICE_HOST constexpr size_type extentImpl(size_t r,
                                                  std::index_sequence<I...>) const noexcept {
        size_type res = ku_npos;
        KU_UNUSED((... || (r == I ? (res = std::get<I>(m_fns).extent(0), true) : false)));
        return res;
    }

    // TODO:: store the transforms in reverse order to utilize EBO of std::tuple
    // For CompactTransform, the previous Rank - 1 transforms are AxisFullTransformFn which are
    // empty class If they are store in tail, the whole tuple can be optimized to the size of
    // CompactTransform.
    std::tuple<Fn...> m_fns; // transform for each dim or axis
};

template <typename T, size_t... I>
auto repeatTs(T &&t, std::index_sequence<I...>) {
    return std::make_tuple(((void)I, std::forward<T>(t))...);
}

template <typename... Trs>
auto makeAxisTransform(Trs &&...trs) {
    auto transform = [](auto &&t) {
        if constexpr (std::is_same_v<std::decay_t<decltype(t)>, KuFullExtent_t>) {
            return AxisFullTransformFn();
        } else if constexpr (ku_is_instance_of_v<std::decay_t<decltype(t)>, StridedSlice>) {
            return AffineTransformFn(t);
        } else if constexpr (ku_is_instance_of_v<std::decay_t<decltype(t)>, KuMdSpan>) {
            using Span = std::decay_t<decltype(t)>;
            using T = typename Span::value_type;
            using L = typename Span::layout_policy;
            using A = typename Span::accessor_type;
            static_assert(Span::rank() == 1, "IndirectTransform only supports rank 1 span");
            return IndirectTransformFn<T, L, A>(t);
        }
    };
    return AxisTransform(transform(std::forward<Trs>(trs))...);
}

template <size_t Rank, typename L, typename A>
auto makeCompactTransform(KuMdSpan<KuBool, Dim1, L, A> compact) {
    static_assert(Rank >= 1, "Rank must be at least 1");
    auto t = MaskTransform<Dim1, L, A>(compact);
    if constexpr (Rank == 1) {
        return AxisTransform(t);
    } else {
        return std::apply(
            [t](auto &&...args) { return AxisTransform(std::forward<decltype(args)>(args)..., t); },
            repeatTs<AxisFullTransformFn>(AxisFullTransformFn(),
                                          std::make_index_sequence<Rank - 1>{}));
    }
}

template <typename Span, typename... Transforms>
class KuView : private std::tuple<Transforms...> {

    static constexpr bool isCompactionView =
        (... || (ku_is_instance_of_v<Transforms, AxisTransform> && Transforms::hasCompaction));
    static constexpr size_t NumTransforms = sizeof...(Transforms);
    // span rank must match each transform's rank
public:
    using view_category =
        std::conditional_t<isCompactionView, CompactionView_t, TransposeView_t>; // tag type
    using extents_type = typename Span::extents_type;
    using size_type = size_t;
    using index_type = size_t;
    using value_type = typename Span::value_type;
    using data_handle_type = value_type *;
    using reference = typename Span::reference;
    // A transformed view can synthesize a null value for an invalid coordinate,
    // so const reads must use value semantics rather than return a dangling
    // reference to that synthesized value.
    using const_reference = value_type;

    KuView(Span span, Transforms &&...transforms)
        : std::tuple<Transforms...>(std::forward<Transforms>(transforms)...), m_span(span) {
    }

    KU_DEVICE_HOST static constexpr size_t transformCount() noexcept {
        return NumTransforms;
    }

    KU_DEVICE_HOST static constexpr size_type rank() noexcept {
        return Span::rank();
    }

    KU_DEVICE_HOST data_handle_type data_handle() const noexcept {
        return m_span.data_handle();
    }

    KU_DEVICE_HOST constexpr size_type extent(std::size_t r) const noexcept {
        if constexpr (NumTransforms == 0) {
            return m_span.extent(r);
        } else {
            return getExtentImpl(r, std::make_index_sequence<NumTransforms>{},
                                 std::make_index_sequence<rank()>{});
        }
    }

    KU_DEVICE_HOST constexpr auto extents() const noexcept {
        std::array<size_type, rank()> exts{};
        getExtentsImpl(exts, std::make_index_sequence<rank()>{});
        return KuDynamicExtents<rank()>(exts);
    }

    template <typename... Idx>
    KU_DEVICE_HOST constexpr reference operator()(Idx... idx) noexcept {
        if constexpr (NumTransforms == 0) {
            return m_span(idx...);
        } else {
            applyTransform(std::make_index_sequence<NumTransforms>{}, idx...);
            return m_span(idx...);
        }
    }

    template <typename... Idx>
    KU_DEVICE_HOST constexpr void set(const value_type &val, Idx... idx) noexcept {
        if constexpr (NumTransforms == 0) {
            m_span(idx...) = val;
        } else {
            applyTransform(std::make_index_sequence<NumTransforms>{}, idx...);
            m_span(idx...) = val;
        }
    }

    template <typename... Idx>
    KU_DEVICE_HOST constexpr const_reference operator()(Idx... idx) const noexcept {
        if constexpr (NumTransforms == 0) {
            return m_span(idx...);
        } else {
            return applyTransformCoordCheck(std::make_index_sequence<NumTransforms>{}, idx...)
                       ? m_span(idx...)
                       : (const_reference)ku_value_traits<value_type>::null();
        }
    }

    template <typename... Idx>
    KU_DEVICE_HOST constexpr const_reference at(Idx... idx) const noexcept {
        if constexpr (NumTransforms == 0) {
            return boundsCheck(std::make_index_sequence<extents_type::rank()>{}, idx...)
                       ? m_span(idx...)
                       : (const_reference)ku_value_traits<value_type>::null();
        } else {
            return applyTransformBoundsCheck(std::make_index_sequence<NumTransforms>{}, idx...)
                       ? (const_reference)m_span(idx...)
                       : (const_reference)ku_value_traits<value_type>::null();
        }
    }

    template <typename... Idx>
    KU_DEVICE_HOST constexpr bool stencilCheck(Idx &...multi_idx) const noexcept {
        return applyTransformCoordCheck(std::make_index_sequence<NumTransforms>{}, multi_idx...);
    }

private:
    template <typename... Idx, size_t... I>
    KU_DEVICE_HOST constexpr void applyTransform(std::index_sequence<I...>,
                                                 Idx &...multi_idx) const noexcept {
        // calculate transformed index in reverse order
        (std::get<NumTransforms - 1 - I>(*this)(multi_idx...), ...);
    }

    template <typename... Idx, size_t... I>
    KU_DEVICE_HOST constexpr bool applyTransformCoordCheck(std::index_sequence<I...>,
                                                           Idx &...multi_idx) const noexcept {
        // calculate transformed index in reverse order
        (std::get<NumTransforms - 1 - I>(*this)(multi_idx...), ...);
        return coordCheck(std::make_index_sequence<extents_type::rank()>{}, multi_idx...);
    }

    template <typename... Idx, size_t... I>
    KU_DEVICE_HOST constexpr bool applyTransformBoundsCheck(std::index_sequence<I...>,
                                                            Idx &...multi_idx) const noexcept {
        // calculate transformed index in reverse order and using short-circuit to return as soon as
        // possible when one axis is out of bound
        return (... && std::get<NumTransforms - 1 - I>(*this).at(multi_idx...))
               && boundsCheck(std::make_index_sequence<extents_type::rank()>{}, multi_idx...);
    }

    template <typename... Idx, size_t... R>
    KU_DEVICE_HOST constexpr bool boundsCheck(std::index_sequence<R...>,
                                              const Idx &...multi_idx) const noexcept {
        auto       args = std::forward_as_tuple(multi_idx...);
        const auto exts = m_span.mapping().extents();
        return (... && (std::get<R>(args) < exts.extent(R)));
    }

    template <typename... Idx, size_t... R>
    KU_DEVICE_HOST constexpr bool coordCheck(std::index_sequence<R...>,
                                             const Idx &...multi_idx) const noexcept {
        auto       args = std::forward_as_tuple(multi_idx...);
        const auto exts = m_span.mapping().extents();
        return (... && (std::get<R>(args) != ku_npos && std::get<R>(args) < exts.extent(R)));
    }

    template <size_t... I, size_t... R>
    KU_DEVICE_HOST constexpr size_type getExtentImpl(size_type rank,
                                                     std::index_sequence<I...>,
                                                     std::index_sequence<R...>) const noexcept {
        // By the definition, a full axis returns ku_npos, so the last transform that is not full
        // will determine the extent. Get the extent recursively in reverse order until a non-full
        // axis is found
        //
        //  the inner fold expression is used to traverse the transforms in reverse order
        //  until it find a non-full axis and early return
        //  the outer fold expression is used to select the 'rank'-th axis
        //
        //

        size_type res = ku_npos;
        KU_UNUSED(
            (...
             || (rank == R
                     ? (KU_UNUSED((...
                                   || (res = std::get<NumTransforms - 1 - I>(*this).extent(rank),
                                       res != ku_npos))),
                        true)
                     : false)));
        if (res == ku_npos)
            res = m_span.extent(rank);
        return res;
    }

    template <size_t... R>
    KU_DEVICE_HOST constexpr void getExtentsImpl(std::array<size_type, rank()> &exts,
                                                 std::index_sequence<R...>) const noexcept {
        if constexpr (NumTransforms == 0) {
            // get span extents directly
            exts = m_span.mapping().extents().as_array();
        } else {
            (..., (exts[R] = getExtentImpl(R, std::make_index_sequence<NumTransforms>{},
                                           std::make_index_sequence<rank()>{})));
        }
    }

    Span m_span;
};

} // namespace kuai
