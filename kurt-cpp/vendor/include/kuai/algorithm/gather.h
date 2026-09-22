#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/gather.h>

namespace kuai {
namespace algo {
template <typename Vendor,
          typename InputIterator1,
          typename RandomAccessIterator,
          typename OutputIterator>
void gather(const KuVendorContext<Vendor> &dc,
            InputIterator1                 mapFirst,
            InputIterator1                 mapLast,
            RandomAccessIterator           input,
            OutputIterator                 output) {
    KU_KERNEL_CALL_ZONE_SCOPED("gather");
    return KU_CALL_VENDOR(gather, dc, mapFirst, mapLast, input, output);
}
} // namespace algo

} // namespace kuai
