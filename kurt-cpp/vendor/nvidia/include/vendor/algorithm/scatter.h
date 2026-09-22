#include <kuai/vendor/KuVendorContext.h>

#include <cub/device/device_for.cuh>
namespace kuai {
namespace vendor::cuda {

template <typename InputIterator, typename MapIterator, typename OutputIterator>
struct KuCubScatterAt {
    KU_DEVICE void operator()(ku_size_t index) {
        m_output[m_map[index]] = m_input[index];
    }

    InputIterator  m_input;
    MapIterator    m_map;
    OutputIterator m_output;
};

template <typename Vendor,
          typename InputIterator1,
          typename InputIterator2,
          typename RandomOutputIterator>
void scatter(const KuVendorContext<Vendor> &dc,
             InputIterator1                 first,
             InputIterator1                 last,
             InputIterator2                 map,
             RandomOutputIterator           output) {
    const auto length = static_cast<ku_size_t>(last - first);
    Vendor::check(cub::DeviceFor::Bulk(
        length,
        KuCubScatterAt<InputIterator1, InputIterator2, RandomOutputIterator>{first, map, output},
        Vendor::nativeStream(dc.m_stream)));
}
} // namespace vendor::cuda
} // namespace kuai

KU_DEFINE_VENDOR(scatter, cuda)
