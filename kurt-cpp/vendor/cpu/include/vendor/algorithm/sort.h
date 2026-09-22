#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include <kuai/vendor/KuVendorContext.h>
namespace kuai {
namespace vendor::cpu {

template <typename Vendor, typename Iterator>
ku_status_t
stable_sort(const KuVendorContext<Vendor> &dc, Iterator first, Iterator last, Iterator output) {
    (void)dc;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    std::vector<value_type> values(first, last);
    std::stable_sort(values.begin(), values.end());
    std::copy(values.begin(), values.end(), output);
    return KU_STATUS_SUCCESS;
}

template <typename Vendor, typename KeyIterator, typename ValueIterator>
ku_status_t stable_sort_by_key(const KuVendorContext<Vendor> &dc,
                               KeyIterator                    first,
                               KeyIterator                    last,
                               ValueIterator                  valBegin,
                               KeyIterator                    keyOut,
                               ValueIterator                  valueOut) {
    (void)dc;
    using key_type = typename std::iterator_traits<KeyIterator>::value_type;
    using value_type = typename std::iterator_traits<ValueIterator>::value_type;
    using pair_type = std::pair<key_type, value_type>;

    std::vector<pair_type> pairs;
    pairs.reserve(static_cast<std::size_t>(std::distance(first, last)));
    std::transform(first, last, valBegin, std::back_inserter(pairs),
                   [](const auto &key, const auto &value) { return pair_type(key, value); });
    std::stable_sort(pairs.begin(), pairs.end(), [](const pair_type &lhs, const pair_type &rhs) {
        return lhs.first < rhs.first;
    });
    std::transform(pairs.begin(), pairs.end(), keyOut,
                   [](const pair_type &pair) { return pair.first; });
    std::transform(pairs.begin(), pairs.end(), valueOut,
                   [](const pair_type &pair) { return pair.second; });
    return KU_STATUS_SUCCESS;
}

} // namespace vendor::cpu

} // namespace kuai

KU_DEFINE_VENDOR(stable_sort, cpu)

KU_DEFINE_VENDOR(stable_sort_by_key, cpu)
