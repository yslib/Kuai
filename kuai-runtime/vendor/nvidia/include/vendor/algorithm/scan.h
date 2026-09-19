#pragma once

#include <cstddef>
#include <utility>

#include <kuai/ktl/iterator/KuCountingIterator.h>
#include <kuai/ktl/iterator/KuTransformIterator.h>

#include "vendor/algorithm/cub.h"

#include <cub/device/device_scan.cuh>

namespace kuai::vendor::cuda {

struct KuCubSegmentKey {
    KU_DEVICE_HOST ku_size_t operator()(ku_size_t index) const noexcept {
        return index / m_segmentLength;
    }

    ku_size_t m_segmentLength;
};

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename ValueIterator,
          typename BinaryFn>
ku_status_t scan(const KuVendorContext<Vendor> &dc,
                 InputIterator                  first,
                 InputIterator                  last,
                 OutputIterator                 output,
                 ValueIterator                  values,
                 BinaryFn                       binaryFn) {
    const auto length = last - first;
    const auto stream = Vendor::nativeStream(dc.m_stream);
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceScan::InclusiveScan(temporary, temporaryBytes, values, output, binaryFn,
                                              length, stream);
    });
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename ValueIterator,
          typename BinaryFn>
ku_status_t segmentedScan(const KuVendorContext<Vendor> &dc,
                          InputIterator                  first,
                          InputIterator                  last,
                          ku_size_t                      segmentedLength,
                          OutputIterator                 output,
                          ValueIterator                  values,
                          BinaryFn                       binaryFn) {
    KU_ASSERT(segmentedLength > 0, "segmented scan requires a nonzero segment length");
    const auto length = last - first;
    const auto keys =
        makeTransformIterator(makeCountingIterator(ku_size_t{0}), KuCubSegmentKey{segmentedLength});
    const auto stream = Vendor::nativeStream(dc.m_stream);
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceScan::InclusiveScanByKey(temporary, temporaryBytes, keys, values, output,
                                                   binaryFn, length, ::cuda::std::equal_to<>{},
                                                   stream);
    });
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename MapFn,
          typename BinaryFn>
ku_status_t inclusive_scan(const KuVendorContext<Vendor> &dc,
                           InputIterator                  first,
                           InputIterator                  last,
                           OutputIterator                 output,
                           MapFn                          mapFn,
                           BinaryFn                       binaryFn) {
    const auto values = makeTransformIterator(first, std::move(mapFn));
    return scan(dc, first, last, output, values, std::move(binaryFn));
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename MapFn,
          typename BinaryFn>
ku_status_t segmented_inclusive_scan(const KuVendorContext<Vendor> &dc,
                                     InputIterator                  first,
                                     InputIterator                  last,
                                     ku_size_t                      segmentedLength,
                                     OutputIterator                 output,
                                     MapFn                          mapFn,
                                     BinaryFn                       binaryFn) {
    const auto values = makeTransformIterator(first, std::move(mapFn));
    return segmentedScan(dc, first, last, segmentedLength, output, values, std::move(binaryFn));
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename BinaryFn>
ku_status_t inclusive_scan(const KuVendorContext<Vendor> &dc,
                           InputIterator                  first,
                           InputIterator                  last,
                           OutputIterator                 output,
                           BinaryFn                       binaryFn) {
    return scan(dc, first, last, output, first, std::move(binaryFn));
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename BinaryFn>
ku_status_t segmented_inclusive_scan(const KuVendorContext<Vendor> &dc,
                                     InputIterator                  first,
                                     InputIterator                  last,
                                     ku_size_t                      segmentedLength,
                                     OutputIterator                 output,
                                     BinaryFn                       binaryFn) {
    return segmentedScan(dc, first, last, segmentedLength, output, first, std::move(binaryFn));
}

} // namespace kuai::vendor::cuda

KU_DEFINE_VENDOR(inclusive_scan, cuda)

KU_DEFINE_VENDOR(segmented_inclusive_scan, cuda)
