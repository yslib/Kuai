#pragma once

#include <utility>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuValueTraits.h>

namespace kuai {

template <typename To, typename From>
KU_DEVICE_HOST constexpr bool isConvertibleWithoutOverflow(const From &value) {
    using Wider = ku_promotion_type_t<From, To>;
    using WiderInternal = decltype(ku_value_traits<Wider>::repr(std::declval<Wider>()));
    const auto widened = static_cast<WiderInternal>(ku_value_traits<decltype(value)>::repr(value));
    return widened >= static_cast<WiderInternal>(
               ku_value_traits<To>::repr(ku_value_traits<To>::lowest()))
           && widened <= static_cast<WiderInternal>(
                  ku_value_traits<To>::repr(ku_value_traits<To>::max()));
}

} // namespace kuai
