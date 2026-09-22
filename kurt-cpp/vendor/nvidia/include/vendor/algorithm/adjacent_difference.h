#include <kuai/vendor/KuVendorContext.h>

#include "vendor/algorithm/cub.h"

#include <cub/device/device_adjacent_difference.cuh>
namespace kuai {
namespace vendor::cuda {

template <typename Vendor, typename InputIterator, typename OutputIterator, typename BinaryOp>
ku_status_t adjacent_difference(const KuVendorContext<Vendor> &dc,
                                InputIterator                  first,
                                InputIterator                  last,
                                OutputIterator                 output,
                                BinaryOp                       op) {
    const auto length = last - first;
    const auto stream = Vendor::nativeStream(dc.m_stream);
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceAdjacentDifference::SubtractLeftCopy(temporary, temporaryBytes, first,
                                                               output, length, op, stream);
    });
}
} // namespace vendor::cuda
} // namespace kuai

KU_DEFINE_VENDOR(adjacent_difference, cuda)
