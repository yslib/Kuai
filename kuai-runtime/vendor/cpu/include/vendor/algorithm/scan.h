#include <numeric>

#include <kuai/core/KuCore.h>
#include <kuai/ktl/functional/KuCast.h>

#include "kuai/algorithm/scan.h"

namespace kuai {
namespace vendor::cpu {

template <typename Vendor,
          typename InputIterator1,
          typename OutputIterator,
          typename MapFn,
          typename BinaryFn>
ku_status_t inclusive_scan(const KuVendorContext<Vendor> &dc,
                           InputIterator1                 first,
                           InputIterator1                 last,
                           OutputIterator                 output,
                           MapFn                          map,
                           BinaryFn                       op) {
    (void)dc;
    std::transform_inclusive_scan(first, last, output, op, map);
    return KU_STATUS_SUCCESS;
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
                                     BinaryFn                       opFn) {
    (void)dc;
    KU_ASSERT(segmentedLength > 0);
    while (first != last) {
        auto segmentLast = first;
        for (ku_size_t i = 0; i < segmentedLength && segmentLast != last; ++i) {
            ++segmentLast;
        }
        output = std::transform_inclusive_scan(first, segmentLast, output, opFn, mapFn);
        first = segmentLast;
    }
    return KU_STATUS_SUCCESS;
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename HomoBinaryFn>
ku_status_t inclusive_scan(const KuVendorContext<Vendor> &dc,
                           InputIterator                  first,
                           InputIterator                  last,
                           OutputIterator                 output,
                           HomoBinaryFn                   binaryOp) {
    (void)dc;
    std::inclusive_scan(first, last, output, binaryOp);
    return KU_STATUS_SUCCESS;
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename HomoBinaryFn>
ku_status_t segmented_inclusive_scan(const KuVendorContext<Vendor> &dc,
                                     InputIterator                  first,
                                     InputIterator                  last,
                                     ku_size_t                      segmentedLength,
                                     OutputIterator                 output,
                                     HomoBinaryFn                   fn) {
    (void)dc;
    KU_ASSERT(segmentedLength > 0);
    while (first != last) {
        auto segmentLast = first;
        for (ku_size_t i = 0; i < segmentedLength && segmentLast != last; ++i) {
            ++segmentLast;
        }
        output = std::inclusive_scan(first, segmentLast, output, fn);
        first = segmentLast;
    }
    return KU_STATUS_SUCCESS;
}

// clang-format off
} // namespace vendor::cpu
// clang-format on

} // namespace kuai

KU_DEFINE_VENDOR(inclusive_scan, cpu)

KU_DEFINE_VENDOR(segmented_inclusive_scan, cpu)
