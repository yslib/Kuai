#include <algorithm>

#include <kuai/vendor/KuVendorContext.h>

namespace kuai {
namespace vendor::cpu {

template <typename Vendor,
          typename InputIterator1,
          typename RandomAccessIterator,
          typename OutputIterator>
void gather(const KuVendorContext<Vendor> &dc,
            InputIterator1                 mapFirst,
            InputIterator1                 mapLast,
            RandomAccessIterator           input,
            OutputIterator                 output) {
    (void)dc;
    std::transform(mapFirst, mapLast, output, [input](const auto &index) { return input[index]; });
}
} // namespace vendor::cpu
} // namespace kuai

KU_DEFINE_VENDOR(gather, cpu)
