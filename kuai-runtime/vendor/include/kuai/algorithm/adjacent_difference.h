#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/adjacent_difference.h>

namespace kuai {
namespace algo {
template <typename Vendor, typename InputIterator, typename OutputIterator, typename BinaryOp>
ku_status_t adjacent_difference(const KuVendorContext<Vendor> &dc,
                                InputIterator                  first,
                                InputIterator                  last,
                                OutputIterator                 output,
                                BinaryOp                       op) {
    KU_KERNEL_CALL_ZONE_SCOPED("adjacent_difference");
    return KU_CALL_VENDOR(adjacent_difference, dc, first, last, output, std::move(op));
}
} // namespace algo

} // namespace kuai
