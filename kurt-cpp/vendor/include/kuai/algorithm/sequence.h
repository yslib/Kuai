#pragma once
#include <limits>
#include <type_traits>
#include <utility>

#include <kuai/algorithm/algorithm_vendor.h>
#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/vendor/KuVendor.h>
#include <kuai/vendor/KuVendorContext.h>

namespace kuai {

namespace algo {

namespace detail {

template <typename T>
struct SequenceValueAt {
    static_assert(std::is_integral_v<T> && std::is_signed_v<T>);

    KU_DEVICE_HOST constexpr T operator()(ku_size_t index) const noexcept {
        using Unsigned = std::make_unsigned_t<T>;
        const auto bits = static_cast<Unsigned>(m_init)
                          + static_cast<Unsigned>(m_step) * static_cast<Unsigned>(index);
        constexpr auto signedMax = static_cast<Unsigned>(std::numeric_limits<T>::max());
        if (bits <= signedMax) {
            return static_cast<T>(bits);
        }

        const auto magnitude = Unsigned{0} - bits;
        if (magnitude == signedMax + Unsigned{1}) {
            return std::numeric_limits<T>::min();
        }
        return -static_cast<T>(magnitude);
    }

    T m_init;
    T m_step;
};

} // namespace detail
} // namespace algo
} // namespace kuai

#include <vendor/algorithm/sequence.h>

namespace kuai {
namespace algo {

template <typename Vendor, typename OutputIterator, typename T>
void sequence(
    const KuVendorContext<Vendor> &exec, OutputIterator begin, OutputIterator end, T init, T step) {
    KU_KERNEL_CALL_ZONE_SCOPED("sequence");
    return KU_CALL_VENDOR(sequence, exec, begin, end, init, step);
}
} // namespace algo
} // namespace kuai
