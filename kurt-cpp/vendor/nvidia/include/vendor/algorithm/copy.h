#include <kuai/vendor/KuVendorContext.h>

#include "vendor/algorithm/cub.h"

#include <cub/device/device_for.cuh>
#include <cub/device/device_select.cuh>
namespace kuai {

namespace vendor::cuda {

template <typename InputIterator, typename OutputIterator>
struct KuCubCopyAt {
    KU_DEVICE void operator()(ku_size_t index) {
        m_output[index] = m_input[index];
    }

    InputIterator  m_input;
    OutputIterator m_output;
};

template <typename Vendor, typename InputIterator, typename OutputIterator>
OutputIterator copy(const KuVendorContext<Vendor> &dc,
                    OutputIterator                 dstFirst,
                    InputIterator                  srcFirst,
                    InputIterator                  srcLast) {
    const auto length = srcLast - srcFirst;
    Vendor::check(
        cub::DeviceFor::Bulk(length, KuCubCopyAt<InputIterator, OutputIterator>{srcFirst, dstFirst},
                             Vendor::nativeStream(dc.m_stream)));
    return dstFirst + length;
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename CountOutputIterator,
          typename PredictFn>
ku_status_t copy_if(const KuVendorContext<Vendor> &dc,
                    OutputIterator                 dstFirst,
                    InputIterator                  srcFirst,
                    InputIterator                  srcLast,
                    CountOutputIterator            selectedCountOutput,
                    PredictFn                    &&fn) {
    auto       predicate = std::forward<PredictFn>(fn);
    const auto length = srcLast - srcFirst;
    const auto stream = Vendor::nativeStream(dc.m_stream);
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceSelect::If(temporary, temporaryBytes, srcFirst, dstFirst,
                                     selectedCountOutput, length, predicate, stream);
    });
}

template <typename Vendor,
          typename InputIterator,
          typename OutputIterator,
          typename StencilIterator,
          typename CountOutputIterator,
          typename PredictFn>
ku_status_t copy_if(const KuVendorContext<Vendor> &dc,
                    OutputIterator                 dstFirst,
                    InputIterator                  srcFirst,
                    InputIterator                  srcLast,
                    StencilIterator                stencilIterator,
                    CountOutputIterator            selectedCountOutput,
                    PredictFn                    &&fn) {
    auto       predicate = std::forward<PredictFn>(fn);
    const auto length = srcLast - srcFirst;
    const auto stream = Vendor::nativeStream(dc.m_stream);
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceSelect::FlaggedIf(temporary, temporaryBytes, srcFirst, stencilIterator,
                                            dstFirst, selectedCountOutput, length, predicate,
                                            stream);
    });
}
} // namespace vendor::cuda

} // namespace kuai

KU_DEFINE_VENDOR(copy, cuda)

KU_DEFINE_VENDOR(copy_if, cuda)
