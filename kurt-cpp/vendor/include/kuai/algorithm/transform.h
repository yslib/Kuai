#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>

#include "kuai/profiler/KuTracy.h"

namespace kuai {
template <typename Vendor>
struct KuVendorContext;
}

#include <vendor/algorithm/transform.h>

namespace kuai {

namespace algo {
template <typename Vendor,
          typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename BinaryFn>
void transform(const KuVendorContext<Vendor> &dc,
               InputIterator1                 first,
               InputIterator1                 last,
               InputIterator2                 first2,
               OutputIterator                 output,
               BinaryFn                       fn) {
    KU_KERNEL_CALL_ZONE_SCOPED("transform_1");
    return KU_CALL_VENDOR(transform, dc, first, last, first2, output, std::move(fn));
}

template <typename Vendor, typename InputIterator1, typename OutputIterator, typename UnaryMap>
void transform(const KuVendorContext<Vendor> &dc,
               InputIterator1                 first,
               InputIterator1                 last,
               OutputIterator                 output,
               UnaryMap                       fn) {
    KU_KERNEL_CALL_ZONE_SCOPED("transform_2");
    return KU_CALL_VENDOR(transform, dc, first, last, output, std::move(fn));
}
} // namespace algo
} // namespace kuai
