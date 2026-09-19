
#include <kuai/ktl/iterator/KuTransformIterator.h>
#include <kuai/vendor/KuVendorContext.h>

#include "vendor/algorithm/cub.h"

#include <cub/device/device_reduce.cuh>
namespace kuai {

namespace vendor::cuda {

template <typename T>
struct KuCubEqualToCount {
    template <typename U>
    KU_DEVICE_HOST ku_size_t operator()(const U &value) const {
        return value == m_target ? 1 : 0;
    }

    T m_target;
};

template <typename Predicate>
struct KuCubPredicateCount {
    template <typename T>
    KU_DEVICE_HOST ku_size_t operator()(const T &value) const {
        return m_predicate(value) ? 1 : 0;
    }

    Predicate m_predicate;
};

template <typename Vendor, typename InputIterator, typename OutputIterator, typename CountFn>
ku_status_t countMapped(const KuVendorContext<Vendor> &dc,
                        InputIterator                  first,
                        InputIterator                  last,
                        OutputIterator                 output,
                        CountFn                        countFn) {
    const auto mapped = makeTransformIterator(first, std::move(countFn));
    const auto length = last - first;
    const auto stream = Vendor::nativeStream(dc.m_stream);
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceReduce::Sum(temporary, temporaryBytes, mapped, output, length, stream);
    });
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename T>
ku_status_t count(const KuVendorContext<Vendor> &dc,
                  InputIterator                  first,
                  InputIterator                  last,
                  OutputIterator                 output,
                  const T                       &target) {
    return countMapped(dc, first, last, output, KuCubEqualToCount<T>{target});
}

template <typename Vendor, typename InputIterator, typename OutputIterator, typename PredictFn>
ku_status_t count_if(const KuVendorContext<Vendor> &dc,
                     InputIterator                  first,
                     InputIterator                  last,
                     OutputIterator                 output,
                     PredictFn                    &&fn) {
    return countMapped(dc, first, last, output,
                       KuCubPredicateCount<std::decay_t<PredictFn>>{std::forward<PredictFn>(fn)});
}

} // namespace vendor::cuda

} // namespace kuai

KU_DEFINE_VENDOR(count, cuda)

KU_DEFINE_VENDOR(count_if, cuda)
