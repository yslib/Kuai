#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/profiler/KuTracy.h"

#include <vendor/algorithm/normal.h>

namespace kuai {
namespace algo {

template <typename Vendor, typename Iterator>
ku_status_t
normal(const KuVendorContext<Vendor> &dc, Iterator begin, Iterator end, double mean, double std) {
    KU_KERNEL_CALL_ZONE_SCOPED("normal");
    return KU_CALL_VENDOR(normal, dc, begin, end, mean, std);
}
} // namespace algo
} // namespace kuai
