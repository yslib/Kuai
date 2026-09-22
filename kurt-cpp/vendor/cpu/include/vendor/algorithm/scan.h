#include <numeric>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/ktl/functional/KuCast.h>

#include "kuai/algorithm/scan.h"

namespace kuai {
namespace vendor::cpu {

namespace detail {

// Bridge the no-init transform_inclusive_scan difference between libstdc++ 14
// (auto init = map(*first)) and libc++ 21 (input value_type init = map(*first)).
// These are different library declarations, not different C++ deduction rules.
//
// [transform.inclusive.scan] constrains convertibility, rather than promising
// that applying map determines the accumulator type: without init, the result
// of op(map(*first), map(*first)) must convert to the input iterator's value_type.
// Kuai's ku_bool_t -> i64 widening does not meet that contract, even though
// libstdc++ accepts it. With an explicit init of type T, op(init, init),
// op(init, map(*first)), and op(map(*first), map(*first)) must instead convert to T.
// See https://eel.is/c++draft/transform.inclusive.scan#2.
//
// Seed the explicit-init overload with the first mapped value to select Kuai's
// mapped accumulator type. Emit that value and scan the remaining range to
// preserve inclusive semantics without requiring an identity element for op.
template <typename InputIterator, typename OutputIterator, typename MapFn, typename BinaryFn>
OutputIterator transformInclusiveScanWithMappedAccumulator(
    InputIterator first, InputIterator last, OutputIterator output, MapFn map, BinaryFn op) {
    if (first == last) {
        return output;
    }
    auto initial = map(*first);
    *output = initial;
    ++first;
    ++output;
    return std::transform_inclusive_scan(first, last, output, op, map, std::move(initial));
}

} // namespace detail

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
    detail::transformInclusiveScanWithMappedAccumulator(first, last, output, map, op);
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
        output = detail::transformInclusiveScanWithMappedAccumulator(first, segmentLast, output,
                                                                     mapFn, opFn);
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
