
#include <kuai/vendor/KuVendorContext.h>

#include <cub/device/device_for.cuh>
namespace kuai {
namespace vendor::cuda {
template <typename Vendor, typename InputIterator, typename UnaryFunction>
InputIterator for_each(const KuVendorContext<Vendor> &dc,
                       InputIterator                  first,
                       InputIterator                  last,
                       UnaryFunction                  f) {
    Vendor::check(
        cub::DeviceFor::ForEach(first, last, std::move(f), Vendor::nativeStream(dc.m_stream)));
    return last;
}
} // namespace vendor::cuda

} // namespace kuai

KU_DEFINE_VENDOR(for_each, cuda)
