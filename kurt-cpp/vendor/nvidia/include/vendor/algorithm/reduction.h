#pragma once

#include <cstddef>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

#include <kuai/core/KuDeviceData.h>
#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/ktl/iterator/KuTransformIterator.h>

#include "vendor/algorithm/cub.h"

#include <cub/device/device_reduce.cuh>
#include <cub/device/device_segmented_reduce.cuh>
#include <cub/iterator/discard_output_iterator.cuh>

namespace kuai::vendor::cuda {

struct KuCubSegmentBegin {
    KU_DEVICE_HOST ku_size_t operator()(ku_size_t segment) const noexcept {
        return segment * m_segmentLength;
    }

    ku_size_t m_segmentLength;
};

struct KuCubSegmentEnd {
    KU_DEVICE_HOST ku_size_t operator()(ku_size_t segment) const noexcept {
        const auto begin = segment * m_segmentLength;
        const auto remaining = m_totalLength - begin;
        return remaining < m_segmentLength ? m_totalLength : begin + m_segmentLength;
    }

    ku_size_t m_segmentLength;
    ku_size_t m_totalLength;
};

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename InitValueType,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce(const KuVendorContext<Vendor> &dc,
                       InitValueType                &&initValue,
                       InputIterator                  first,
                       InputIterator                  last,
                       OutputIterator                 output,
                       MapFn                        &&mapFn,
                       ReduceBinaryFn               &&reduceFn) {
    auto       map = std::forward<MapFn>(mapFn);
    auto       reduce = std::forward<ReduceBinaryFn>(reduceFn);
    auto       initial = std::forward<InitValueType>(initValue);
    const auto mappedFirst = makeTransformIterator(first, map);
    const auto length = last - first;
    const auto stream = Vendor::nativeStream(dc.m_stream);

    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceReduce::Reduce(temporary, temporaryBytes, mappedFirst, output, length,
                                         reduce, initial, stream);
    });
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename InitValueType,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce(const KuVendorContext<Vendor> &dc,
                       InitValueType                &&initValue,
                       InputIterator                  first,
                       InputIterator                  last,
                       ku_size_t                      segmentedLength,
                       OutputIterator                 output,
                       MapFn                        &&mapFn,
                       ReduceBinaryFn               &&reduceFn) {
    KU_ASSERT(segmentedLength > 0, "segmented reduction requires a nonzero segment length");
    const auto length = static_cast<ku_size_t>(last - first);
    const auto segmentCount =
        length == 0 ? ku_size_t{0} : ku_size_t{1} + (length - 1) / segmentedLength;
    if (segmentCount > static_cast<ku_size_t>(std::numeric_limits<int>::max())) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    auto       map = std::forward<MapFn>(mapFn);
    auto       reduce = std::forward<ReduceBinaryFn>(reduceFn);
    auto       initial = std::forward<InitValueType>(initValue);
    const auto mappedFirst = makeTransformIterator(first, map);
    const auto segmentIndices = makeCountingIterator(ku_size_t{0});
    const auto beginOffsets =
        makeTransformIterator(segmentIndices, KuCubSegmentBegin{segmentedLength});
    const auto endOffsets =
        makeTransformIterator(segmentIndices, KuCubSegmentEnd{segmentedLength, length});
    const auto stream = Vendor::nativeStream(dc.m_stream);

    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceSegmentedReduce::Reduce(temporary, temporaryBytes, mappedFirst, output,
                                                  static_cast<int>(segmentCount), beginOffsets,
                                                  endOffsets, reduce, initial, stream);
    });
}

template <typename Vendor,
          typename KeyIterator,
          typename InputIterator,
          typename OutputIterator,
          typename RunCountOutputIterator,
          typename MapFn,
          typename ReduceBinaryFn>
ku_status_t map_reduce_by_key(const KuVendorContext<Vendor> &dc,
                              KeyIterator                    keyFirst,
                              KeyIterator                    keyLast,
                              InputIterator                  valueFirst,
                              OutputIterator                 outputFirst,
                              RunCountOutputIterator         runCountOutput,
                              MapFn                        &&mapFn,
                              ReduceBinaryFn               &&reduceFn) {
    auto       map = std::forward<MapFn>(mapFn);
    auto       reduce = std::forward<ReduceBinaryFn>(reduceFn);
    const auto mappedFirst = makeTransformIterator(valueFirst, map);
    const auto length = keyLast - keyFirst;
    const auto stream = Vendor::nativeStream(dc.m_stream);
    const auto discardedKeys = cub::DiscardOutputIterator<>{};

    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceReduce::ReduceByKey(temporary, temporaryBytes, keyFirst, discardedKeys,
                                              mappedFirst, outputFirst, runCountOutput, reduce,
                                              length, stream);
    });
}

template <typename Vendor,
          typename KeyIterator,
          typename InputIterator,
          typename OutputIterator,
          typename RunCountOutputIterator,
          typename BinaryFn>
ku_status_t reduce_by_key(const KuVendorContext<Vendor> &dc,
                          KeyIterator                    keyFirst,
                          KeyIterator                    keyLast,
                          InputIterator                  valueFirst,
                          OutputIterator                 outputFirst,
                          RunCountOutputIterator         runCountOutput,
                          BinaryFn                     &&reduceFn) {
    return map_reduce_by_key(dc, keyFirst, keyLast, valueFirst, outputFirst, runCountOutput,
                             detail::KuCubIdentity{}, std::forward<BinaryFn>(reduceFn));
}

} // namespace kuai::vendor::cuda

KU_DEFINE_VENDOR(map_reduce, cuda)

KU_DEFINE_VENDOR(map_reduce_by_key, cuda)

KU_DEFINE_VENDOR(reduce_by_key, cuda)
