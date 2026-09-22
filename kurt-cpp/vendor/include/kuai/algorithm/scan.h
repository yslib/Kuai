#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/scan.h>
namespace kuai {

namespace algo {
template <typename Vendor,
          typename InputIterator1,
          typename OutputIterator,
          typename MapFn,
          typename BinaryFn>
ku_status_t inclusive_scan(const KuVendorContext<Vendor> &dc,
                           InputIterator1                 first,
                           InputIterator1                 last,
                           OutputIterator                 output,
                           MapFn                          mapFn,
                           BinaryFn                       binaryOp) {
    KU_KERNEL_CALL_ZONE_SCOPED("inclusive_scan_1");
    return KU_CALL_VENDOR(inclusive_scan, dc, first, last, output, std::move(mapFn),
                          std::move(binaryOp));
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename MapFn,
          typename BinaryFn>
ku_status_t segmented_inclusive_scan(const KuVendorContext<Vendor> &dc,
                                     InputIterator                  first,
                                     InputIterator                  last,
                                     ku_size_t                      size,
                                     OutputIterator                 output,
                                     MapFn                          fn,
                                     BinaryFn                       binaryOp) {
    KU_KERNEL_CALL_ZONE_SCOPED("segmented_inclusive_scan_1");
    return KU_CALL_VENDOR(segmented_inclusive_scan, dc, first, last, size, output, std::move(fn),
                          std::move(binaryOp));
}

// above two functions are deprecated, use the following two functions instead
template <typename Vendor, typename InputIterator, typename OutputIterator, typename HomoBinaryFn>
ku_status_t inclusive_scan(const KuVendorContext<Vendor> &dc,
                           InputIterator                  first,
                           InputIterator                  last,
                           OutputIterator                 output,
                           HomoBinaryFn                   fn) {
    KU_KERNEL_CALL_ZONE_SCOPED("inclusive_scan_2");
    return KU_CALL_VENDOR(inclusive_scan, dc, first, last, output, std::move(fn));
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename HomoBinaryFn>
ku_status_t segmented_inclusive_scan(const KuVendorContext<Vendor> &dc,
                                     InputIterator                  first,
                                     InputIterator                  last,
                                     ku_size_t                      segmentedLength,
                                     OutputIterator                 output,
                                     HomoBinaryFn                   fn) {
    KU_KERNEL_CALL_ZONE_SCOPED("segmented_inclusive_scan_2");
    return KU_CALL_VENDOR(segmented_inclusive_scan, dc, first, last, segmentedLength, output,
                          std::move(fn));
}
} // namespace algo
} // namespace kuai
