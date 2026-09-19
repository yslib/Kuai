#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/sort.h>
namespace kuai {

namespace algo {

template <typename Vendor, typename Iterator>
ku_status_t
stable_sort(const KuVendorContext<Vendor> &dc, Iterator first, Iterator last, Iterator output) {
    KU_KERNEL_CALL_ZONE_SCOPED("stable_sort");
    return KU_CALL_VENDOR(stable_sort, dc, first, last, output);
}

template <typename Vendor, typename KeyIterator, typename ValueIterator>
ku_status_t stable_sort_by_key(const KuVendorContext<Vendor> &dc,
                               KeyIterator                    first,
                               KeyIterator                    last,
                               ValueIterator                  valBegin,
                               KeyIterator                    keyOut,
                               ValueIterator                  valueOut) {
    KU_KERNEL_CALL_ZONE_SCOPED("stable_sort_by_key");
    return KU_CALL_VENDOR(stable_sort_by_key, dc, first, last, valBegin, keyOut, valueOut);
}

} // namespace algo
} // namespace kuai
