#pragma once

#include <kuai/vendor/KuVendorContext.h>

#include <vendor/KuVendor.h>

namespace kuai {
namespace vendor::cpu {

template <typename Vendor, typename Iterator>
ku_status_t
normal(const KuVendorContext<Vendor> &dc, Iterator begin, Iterator end, double mean, double std) {
    (void)dc;
    (void)begin;
    (void)end;
    (void)mean;
    (void)std;
    return KU_STATUS_NOT_SUPPORTED;
}

} // namespace vendor::cpu

} // namespace kuai

KU_DEFINE_VENDOR(normal, cpu)
