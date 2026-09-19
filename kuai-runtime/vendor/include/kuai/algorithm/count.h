#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/profiler/KuTracy.h"

#include <vendor/algorithm/count.h>

namespace kuai {
namespace algo {
template <typename Vendor, typename InputIterator, typename OutputIterator, typename T>
ku_status_t count(const KuVendorContext<Vendor> &dc,
                  InputIterator                  first,
                  InputIterator                  last,
                  OutputIterator                 output,
                  const T                       &target) {
    KU_KERNEL_CALL_ZONE_SCOPED("count");
    return KU_CALL_VENDOR(count, dc, first, last, output, target);
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename PredictFn>
ku_status_t count_if(const KuVendorContext<Vendor> &dc,
                     InputIterator                  first,
                     InputIterator                  last,
                     OutputIterator                 output,
                     PredictFn                    &&fn) {
    KU_KERNEL_CALL_ZONE_SCOPED("count_if");
    return KU_CALL_VENDOR(count_if, dc, first, last, output, std::forward<PredictFn>(fn));
}

} // namespace algo
} // namespace kuai
