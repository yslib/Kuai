#include <algorithm>
#include <utility>

#include <kuai/vendor/KuVendorContext.h>

namespace kuai {
namespace vendor::cpu {
template <typename Vendor, typename InputIterator, typename UnaryFunction>
InputIterator for_each(const KuVendorContext<Vendor> &dc,
                       InputIterator                  first,
                       InputIterator                  last,
                       UnaryFunction                  f) {
    (void)dc;
    std::for_each(first, last, std::move(f));
    return last;
}
} // namespace vendor::cpu

} // namespace kuai

KU_DEFINE_VENDOR(for_each, cpu)
