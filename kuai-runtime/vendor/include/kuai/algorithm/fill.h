#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/fill.h>
namespace kuai {

namespace algo {
template <typename Vendor, typename InputIterator, typename T>
void fill(const KuVendorContext<Vendor> &exec,
          InputIterator                  srcFirst,
          InputIterator                  srcLast,
          const T                       &value) {
    KU_KERNEL_CALL_ZONE_SCOPED("fill");
    return KU_CALL_VENDOR(fill, exec, srcFirst, srcLast, value);
}

template <typename Vendor, typename OutputIterator, typename T>
void fill_n(const KuVendorContext<Vendor> &exec,
            OutputIterator                 srcFirst,
            size_t                         n,
            const T                       &value) {
    KU_KERNEL_CALL_ZONE_SCOPED("fill_n");
    return KU_CALL_VENDOR(fill_n, exec, srcFirst, n, value);
}
} // namespace algo
} // namespace kuai
