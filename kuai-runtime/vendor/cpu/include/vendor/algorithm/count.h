#include <algorithm>
#include <utility>

#include <kuai/vendor/KuVendorContext.h>

namespace kuai {

namespace vendor::cpu {

template <typename Vendor, typename InputIterator, typename OutputIterator, typename T>
ku_status_t count(const KuVendorContext<Vendor> &dc,
                  InputIterator                  first,
                  InputIterator                  last,
                  OutputIterator                 output,
                  const T                       &target) {
    (void)dc;
    *output = static_cast<ku_size_t>(std::count(first, last, target));
    return KU_STATUS_SUCCESS;
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename PredictFn>
ku_status_t count_if(const KuVendorContext<Vendor> &dc,
                     InputIterator                  first,
                     InputIterator                  last,
                     OutputIterator                 output,
                     PredictFn                    &&fn) {
    (void)dc;
    *output = static_cast<ku_size_t>(std::count_if(first, last, std::forward<PredictFn>(fn)));
    return KU_STATUS_SUCCESS;
}

} // namespace vendor::cpu

} // namespace kuai

KU_DEFINE_VENDOR(count, cpu)

KU_DEFINE_VENDOR(count_if, cpu)
