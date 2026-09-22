#include <numeric>

#include <kuai/vendor/KuVendorContext.h>

namespace kuai {
namespace vendor::cpu {

template <typename Vendor, typename InputIterator, typename OutputIterator, typename BinaryOp>
ku_status_t adjacent_difference(const KuVendorContext<Vendor> &dc,
                                InputIterator                  first,
                                InputIterator                  last,
                                OutputIterator                 output,
                                BinaryOp                       op) {
    (void)dc;
    std::adjacent_difference(first, last, output, op);
    return KU_STATUS_SUCCESS;
}
} // namespace vendor::cpu
} // namespace kuai

KU_DEFINE_VENDOR(adjacent_difference, cpu)
