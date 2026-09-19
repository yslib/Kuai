#pragma once

#include <cstddef>
#include <curand.h>
#include <iterator>
#include <type_traits>
#include <utility>

#include <vendor/KuVendorStatus.h>
#include <vendor/KuVendorTraits.h>

namespace kuai::vendor::cuda {

template <typename Vendor, typename Iterator>
ku_status_t normal(
    const KuVendorContext<Vendor> &dc, Iterator begin, Iterator end, double mean, double stddev) {
    using T = typename std::iterator_traits<Iterator>::value_type;
    const auto size = static_cast<std::size_t>(std::distance(begin, end));
    KU_ASSERT(size % 2U == 0U, "cuRAND normal generation requires an even storage extent");

    auto generatorResult =
        ku_vendor_traits<Vendor>::random_generator(dc.m_ctx, dc.m_dev, dc.m_stream);
    if (!generatorResult) {
        return generatorResult.error();
    }
    auto generator = std::move(generatorResult).value();
    if constexpr (std::is_same_v<T, float>) {
        return detail::toStatus(curandGenerateNormal(
            generator.get(), begin, size, static_cast<float>(mean), static_cast<float>(stddev)));
    } else {
        static_assert(std::is_same_v<T, double>, "normal supports float or double output");
        return detail::toStatus(
            curandGenerateNormalDouble(generator.get(), begin, size, mean, stddev));
    }
}

} // namespace kuai::vendor::cuda

KU_DEFINE_VENDOR(normal, cuda)
