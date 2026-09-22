#pragma once
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

#include "kuai/profiler/KuTracy.h"

#include <vendor/algorithm/copy.h>

namespace kuai {
namespace algo {
template <typename Vendor, typename InputIterator, typename OutputIterator>
OutputIterator copy(const KuVendorContext<Vendor> &dc,
                    OutputIterator                 dstFirst,
                    InputIterator                  srcFirst,
                    InputIterator                  srcLast) {
    KU_KERNEL_CALL_ZONE_SCOPED("copy");
    return KU_CALL_VENDOR(copy, dc, dstFirst, srcFirst, srcLast);
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename CountOutputIterator,
          typename PredictFn>
ku_status_t copy_if(const KuVendorContext<Vendor> &dc,
                    OutputIterator                 dstFirst,
                    InputIterator                  srcFirst,
                    InputIterator                  srcLast,
                    CountOutputIterator            selectedCountOutput,
                    PredictFn                    &&fn) {
    KU_KERNEL_CALL_ZONE_SCOPED("copy_if_1");
    return KU_CALL_VENDOR(copy_if, dc, dstFirst, srcFirst, srcLast, selectedCountOutput,
                          std::forward<PredictFn>(fn));
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename StencilIterator,
          typename CountOutputIterator,
          typename PredictFn>
ku_status_t copy_if(const KuVendorContext<Vendor> &dc,
                    OutputIterator                 dstFirst,
                    InputIterator                  srcFirst,
                    InputIterator                  srcLast,
                    StencilIterator                stencilIterator,
                    CountOutputIterator            selectedCountOutput,
                    PredictFn                    &&fn) {
    KU_KERNEL_CALL_ZONE_SCOPED("copy_if_2");
    return KU_CALL_VENDOR(copy_if, dc, dstFirst, srcFirst, srcLast, stencilIterator,
                          selectedCountOutput, std::forward<PredictFn>(fn));
}
} // namespace algo
} // namespace kuai
