#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include <vendor/algorithm/for_each.h>
namespace kuai {

namespace algo {
template <typename Vendor, typename InputIterator, typename UnaryFunction>
InputIterator for_each(const KuVendorContext<Vendor> &dc,
                       InputIterator                  first,
                       InputIterator                  last,
                       UnaryFunction                  f) {
    KU_KERNEL_CALL_ZONE_SCOPED("for_each");
    return KU_CALL_VENDOR(for_each, dc, first, last, std::move(f));
}
} // namespace algo
} // namespace kuai
