#pragma once
#include <kuai/vendor/KuVendorContext.h>

#include <cub/device/device_for.cuh>
namespace kuai {

namespace vendor::cuda {

template <typename InputIterator, typename OutputIterator, typename UnaryFn>
struct KuCubUnaryTransformAt {
    KU_DEVICE void operator()(ku_size_t index) {
        m_output[index] = m_fn(m_input[index]);
    }

    InputIterator  m_input;
    OutputIterator m_output;
    UnaryFn        m_fn;
};

template <typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename BinaryFn>
struct KuCubBinaryTransformAt {
    KU_DEVICE void operator()(ku_size_t index) {
        m_output[index] = m_fn(m_input1[index], m_input2[index]);
    }

    InputIterator1 m_input1;
    InputIterator2 m_input2;
    OutputIterator m_output;
    BinaryFn       m_fn;
};

template <typename Vendor,
          typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename BinaryFn>
void transform(const KuVendorContext<Vendor> &dc,
               InputIterator1                 first,
               InputIterator1                 last,
               InputIterator2                 first2,
               OutputIterator                 output,
               BinaryFn                       fn) {
    const auto length = last - first;
    Vendor::check(cub::DeviceFor::Bulk(
        length,
        KuCubBinaryTransformAt<InputIterator1, InputIterator2, OutputIterator, BinaryFn>{
            first, first2, output, std::move(fn)},
        Vendor::nativeStream(dc.m_stream)));
}

template <typename Vendor, typename InputIterator1, typename OutputIterator, typename UnaryMap>
void transform(const KuVendorContext<Vendor> &dc,
               InputIterator1                 first,
               InputIterator1                 last,
               OutputIterator                 output,
               UnaryMap                       fn) {
    const auto length = last - first;
    Vendor::check(
        cub::DeviceFor::Bulk(length,
                             KuCubUnaryTransformAt<InputIterator1, OutputIterator, UnaryMap>{
                                 first, output, std::move(fn)},
                             Vendor::nativeStream(dc.m_stream)));
}
} // namespace vendor::cuda

} // namespace kuai

KU_DEFINE_VENDOR(transform, cuda)
