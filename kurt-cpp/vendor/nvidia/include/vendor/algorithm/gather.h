#include <kuai/vendor/KuVendorContext.h>

#include <cub/device/device_for.cuh>

namespace kuai {
namespace vendor::cuda {

template <typename MapIterator, typename InputIterator, typename OutputIterator>
struct KuCubGatherAt {
    KU_DEVICE void operator()(ku_size_t index) {
        m_output[index] = m_input[m_map[index]];
    }

    MapIterator    m_map;
    InputIterator  m_input;
    OutputIterator m_output;
};

template <typename Vendor,
          typename InputIterator1,
          typename RandomAccessIterator,
          typename OutputIterator>
void gather(const KuVendorContext<Vendor> &dc,
            InputIterator1                 mapFirst,
            InputIterator1                 mapLast,
            RandomAccessIterator           input,
            OutputIterator                 output) {
    const auto length = static_cast<ku_size_t>(mapLast - mapFirst);
    Vendor::check(
        cub::DeviceFor::Bulk(length,
                             KuCubGatherAt<InputIterator1, RandomAccessIterator, OutputIterator>{
                                 mapFirst, input, output},
                             Vendor::nativeStream(dc.m_stream)));
}
} // namespace vendor::cuda
} // namespace kuai

KU_DEFINE_VENDOR(gather, cuda)
