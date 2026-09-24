#pragma once

#include <algorithm>
#include <iterator>
#include <random>
#include <type_traits>

#include <kuai/vendor/KuVendorContext.h>

#include <vendor/KuVendor.h>

namespace kuai {
namespace vendor::cpu {

template <typename Vendor, typename Iterator>
ku_status_t normal(
    const KuVendorContext<Vendor> &dc, Iterator begin, Iterator end, double mean, double stddev) {
    (void)dc;
    using T = typename std::iterator_traits<Iterator>::value_type;
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>,
                  "normal supports float or double output");
    if (begin == end) {
        return KU_STATUS_SUCCESS;
    }
    // The builtin permits zero deviation; std::normal_distribution requires > 0.
    if (stddev == 0.0) {
        std::fill(begin, end, static_cast<T>(mean));
        return KU_STATUS_SUCCESS;
    }

    thread_local std::mt19937_64 generator = [] {
        std::random_device entropy;
        std::seed_seq      seed{entropy(), entropy(), entropy(), entropy()};
        return std::mt19937_64(seed);
    }();
    std::normal_distribution<double> distribution(mean, stddev);
    std::generate(begin, end, [&] { return static_cast<T>(distribution(generator)); });
    return KU_STATUS_SUCCESS;
}

} // namespace vendor::cpu

} // namespace kuai

KU_DEFINE_VENDOR(normal, cpu)
