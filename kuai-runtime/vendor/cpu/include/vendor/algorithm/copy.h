#include <algorithm>
#include <functional>
#include <utility>

#include <kuai/vendor/KuVendorContext.h>
namespace kuai {

namespace vendor::cpu {
template <typename Vendor, typename InputIterator, typename OutputIterator>
OutputIterator copy(const KuVendorContext<Vendor> &dc,
                    OutputIterator                 dstFirst,
                    InputIterator                  srcFirst,
                    InputIterator                  srcLast) {
    (void)dc;
    return std::copy(srcFirst, srcLast, dstFirst);
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
    (void)dc;
    ku_size_t selectedCount = 0;
    for (; srcFirst != srcLast; ++srcFirst) {
        if (std::invoke(fn, *srcFirst)) {
            *dstFirst = *srcFirst;
            ++dstFirst;
            ++selectedCount;
        }
    }
    *selectedCountOutput = selectedCount;
    return KU_STATUS_SUCCESS;
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
    (void)dc;
    ku_size_t selectedCount = 0;
    for (; srcFirst != srcLast; ++srcFirst, ++stencilIterator) {
        if (std::invoke(fn, *stencilIterator)) {
            *dstFirst = *srcFirst;
            ++dstFirst;
            ++selectedCount;
        }
    }
    *selectedCountOutput = selectedCount;
    return KU_STATUS_SUCCESS;
}

// clang-format off
} // namespace vendor::cpu

// clang-format on
} // namespace kuai

KU_DEFINE_VENDOR(copy, cpu)

KU_DEFINE_VENDOR(copy_if, cpu)
