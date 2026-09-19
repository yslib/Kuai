#include <algorithm>

#include <kuai/vendor/KuVendorContext.h>

namespace kuai {
namespace vendor::cpu {

template <typename Vendor,
          typename InputIterator1,
          typename InputIterator2,
          typename RandomOutputIterator>
void scatter(const KuVendorContext<Vendor> &dc,
             InputIterator1                 first,
             InputIterator1                 last,
             InputIterator2                 map,
             RandomOutputIterator           output) {
    (void)dc;
    std::for_each(first, last, [&](const auto &value) {
        output[*map] = value;
        ++map;
    });
}

} // namespace vendor::cpu
} // namespace kuai

KU_DEFINE_VENDOR(scatter, cpu)
