#pragma once

#include <cstddef>
#include <utility>

#include <kuai/core/KuCore.h>

#define KU_STATIC_SWITCH_CASES_4(CASE, Base) \
    CASE((Base) + 0)                         \
    CASE((Base) + 1)                         \
    CASE((Base) + 2)                         \
    CASE((Base) + 3)

#define KU_STATIC_SWITCH_CASES_16(CASE, Base)  \
    KU_STATIC_SWITCH_CASES_4(CASE, (Base) + 0) \
    KU_STATIC_SWITCH_CASES_4(CASE, (Base) + 4) \
    KU_STATIC_SWITCH_CASES_4(CASE, (Base) + 8) \
    KU_STATIC_SWITCH_CASES_4(CASE, (Base) + 12)

#define KU_STATIC_SWITCH_CASES_64(CASE, Base)    \
    KU_STATIC_SWITCH_CASES_16(CASE, (Base) + 0)  \
    KU_STATIC_SWITCH_CASES_16(CASE, (Base) + 16) \
    KU_STATIC_SWITCH_CASES_16(CASE, (Base) + 32) \
    KU_STATIC_SWITCH_CASES_16(CASE, (Base) + 48)

#define KU_STATIC_SWITCH_CASES_256(CASE, Base)    \
    KU_STATIC_SWITCH_CASES_64(CASE, (Base) + 0)   \
    KU_STATIC_SWITCH_CASES_64(CASE, (Base) + 64)  \
    KU_STATIC_SWITCH_CASES_64(CASE, (Base) + 128) \
    KU_STATIC_SWITCH_CASES_64(CASE, (Base) + 192)

#define KU_STATIC_SWITCH_CASES_512(CASE) \
    KU_STATIC_SWITCH_CASES_256(CASE, 0)  \
    KU_STATIC_SWITCH_CASES_256(CASE, 256)

namespace kuai {

template <std::size_t Size, typename Fn>
decltype(auto) KuStaticSwitch(std::size_t index, Fn &&fn) {
    static_assert(Size > 0, "KuStaticSwitch requires at least one case");
    static_assert(Size <= 512, "KuStaticSwitch supports at most 512 cases");

    KU_ASSERT(index < Size, "KuStaticSwitch index is out of range");
    switch (index) {
#define KU_STATIC_SWITCH_CASE(Index)                                  \
    case Index:                                                       \
        if constexpr (Index < Size) {                                 \
            return std::forward<Fn>(fn).template operator()<Index>(); \
        }                                                             \
        break;
        KU_STATIC_SWITCH_CASES_512(KU_STATIC_SWITCH_CASE)
#undef KU_STATIC_SWITCH_CASE
        default:
            break;
    }
    KU_UNREACHABLE();
}

} // namespace kuai

#undef KU_STATIC_SWITCH_CASES_512
#undef KU_STATIC_SWITCH_CASES_256
#undef KU_STATIC_SWITCH_CASES_64
#undef KU_STATIC_SWITCH_CASES_16
#undef KU_STATIC_SWITCH_CASES_4
