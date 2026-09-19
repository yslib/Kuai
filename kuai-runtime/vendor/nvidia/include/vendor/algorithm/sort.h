#include <kuai/vendor/KuVendorContext.h>

#include "vendor/algorithm/cub.h"

#include <cub/device/device_radix_sort.cuh>
namespace kuai {
namespace vendor::cuda {

template <typename Vendor, typename Iterator>
ku_status_t
stable_sort(const KuVendorContext<Vendor> &dc, Iterator first, Iterator last, Iterator output) {
    const auto stream = Vendor::nativeStream(dc.m_stream);
    const auto length = last - first;
    using Key = typename std::iterator_traits<Iterator>::value_type;
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceRadixSort::SortKeys(temporary, temporaryBytes, first, output, length, 0,
                                              sizeof(Key) * 8, stream);
    });
}

template <typename Vendor, typename KeyIterator, typename ValueIterator>
ku_status_t stable_sort_by_key(const KuVendorContext<Vendor> &dc,
                               KeyIterator                    first,
                               KeyIterator                    last,
                               ValueIterator                  valBegin,
                               KeyIterator                    keyOut,
                               ValueIterator                  valueOut) {
    const auto stream = Vendor::nativeStream(dc.m_stream);
    const auto length = last - first;
    using Key = typename std::iterator_traits<KeyIterator>::value_type;
    return detail::invokeCub<Vendor>(dc, [&](void *temporary, std::size_t &temporaryBytes) {
        return cub::DeviceRadixSort::SortPairs(temporary, temporaryBytes, first, keyOut, valBegin,
                                               valueOut, length, 0, sizeof(Key) * 8, stream);
    });
}

} // namespace vendor::cuda
} // namespace kuai

KU_DEFINE_VENDOR(stable_sort, cuda)

KU_DEFINE_VENDOR(stable_sort_by_key, cuda)
