#pragma once
#include "kuai/algorithm/sequence.h"

#include <cub/device/device_for.cuh>
namespace kuai {
namespace vendor::cuda {

template <typename OutputIterator, typename T>
struct KuCubSequenceAt {
    KU_DEVICE void operator()(ku_size_t index) {
        m_output[index] = m_valueAt(index);
    }

    OutputIterator                   m_output;
    algo::detail::SequenceValueAt<T> m_valueAt;
};

template <typename Vendor, typename OutputIterator, typename T>
void sequence(
    const KuVendorContext<Vendor> &dc, OutputIterator begin, OutputIterator end, T init, T step) {
    const auto length = static_cast<ku_size_t>(end - begin);
    Vendor::check(cub::DeviceFor::Bulk(
        length,
        KuCubSequenceAt<OutputIterator, T>{begin, algo::detail::SequenceValueAt<T>{init, step}},
        Vendor::nativeStream(dc.m_stream)));
}
} // namespace vendor::cuda
} // namespace kuai

KU_DEFINE_VENDOR(sequence, cuda)
