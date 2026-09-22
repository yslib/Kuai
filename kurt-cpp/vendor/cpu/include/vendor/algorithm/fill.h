#include <algorithm>

#include <kuai/vendor/KuVendorContext.h>

namespace kuai {
namespace vendor::cpu {
template <typename Vendor, typename InputIterator, typename T>
void fill(const KuVendorContext<Vendor> &dc,
          InputIterator                  srcFirst,
          InputIterator                  srcLast,
          const T                       &value) {
    (void)dc;
    std::fill(srcFirst, srcLast, value);
}

template <typename Vendor, typename OutputIterator, typename T>
void fill_n(const KuVendorContext<Vendor> &dc, OutputIterator srcFirst, size_t n, const T &value) {
    (void)dc;
    std::fill_n(srcFirst, n, value);
}

} // namespace vendor::cpu

} // namespace kuai

KU_DEFINE_VENDOR(fill, cpu)

KU_DEFINE_VENDOR(fill_n, cpu)
