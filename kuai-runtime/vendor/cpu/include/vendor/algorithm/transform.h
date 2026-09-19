#pragma once
#include <algorithm>
#include <utility>

#include <kuai/vendor/KuVendorContext.h>

namespace kuai {
namespace vendor::cpu {

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
    (void)dc;
    std::transform(first, last, first2, output, std::move(fn));
}

template <typename Vendor, typename InputIterator1, typename OutputIterator, typename UnaryMap>
void transform(const KuVendorContext<Vendor> &dc,
               InputIterator1                 first,
               InputIterator1                 last,
               OutputIterator                 output,
               UnaryMap                       fn) {
    (void)dc;
    std::transform(first, last, output, std::move(fn));
}

} // namespace vendor::cpu
} // namespace kuai

KU_DEFINE_VENDOR(transform, cpu)
