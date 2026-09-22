
#include <kuai/vendor/KuVendorContext.h>

#include <cub/device/device_for.cuh>
namespace kuai {
namespace vendor::cuda {

template <typename OutputIterator, typename T>
struct KuCubFillAt {
    KU_DEVICE void operator()(ku_size_t index) {
        m_output[index] = m_value;
    }

    OutputIterator m_output;
    T              m_value;
};

template <typename Vendor, typename InputIterator, typename T>
void fill(const KuVendorContext<Vendor> &dc,
          InputIterator                  srcFirst,
          InputIterator                  srcLast,
          const T                       &value) {
    fill_n(dc, srcFirst, static_cast<ku_size_t>(srcLast - srcFirst), value);
}

template <typename Vendor, typename OutputIterator, typename T>
void fill_n(const KuVendorContext<Vendor> &dc, OutputIterator srcFirst, size_t n, const T &value) {
    Vendor::check(cub::DeviceFor::Bulk(static_cast<ku_size_t>(n),
                                       KuCubFillAt<OutputIterator, T>{srcFirst, value},
                                       Vendor::nativeStream(dc.m_stream)));
}
} // namespace vendor::cuda
} // namespace kuai

KU_DEFINE_VENDOR(fill, cuda)

KU_DEFINE_VENDOR(fill_n, cuda)
