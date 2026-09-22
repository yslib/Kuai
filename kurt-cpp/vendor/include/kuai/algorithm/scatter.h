#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/scatter.h>

namespace kuai {
namespace algo {
template <typename Vendor,
          typename InputIterator1,
          typename InputIterator2,
          typename RandomOutputIterator>
void scatter(const KuVendorContext<Vendor> &dc,
             InputIterator1                 first,
             InputIterator1                 last,
             InputIterator2                 map,
             RandomOutputIterator           output) {
    KU_KERNEL_CALL_ZONE_SCOPED("scatter");
    return KU_CALL_VENDOR(scatter, dc, first, last, map, output);
}
} // namespace algo

} // namespace kuai
