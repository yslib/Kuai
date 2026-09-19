#pragma once

#include <limits>
#include <optional>
#include <type_traits>

#include <kuai/core/KuTypes.h>
#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai {

namespace detail {

template <typename T>
struct ku_value_traits_impl {
    static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>,
                  "ku_value_traits requires a non-bool arithmetic representation");

    KU_DEVICE_HOST static constexpr T dflt() noexcept {
        return T{};
    }

    KU_DEVICE_HOST static constexpr T null() noexcept {
        return lowest();
    }

    KU_DEVICE_HOST static constexpr T lowest() noexcept {
        return std::numeric_limits<T>::lowest();
    }

    KU_DEVICE_HOST static constexpr T max() noexcept {
        return std::numeric_limits<T>::max();
    }

    KU_DEVICE_HOST static constexpr bool isNull(const T &value) noexcept {
        return value == null();
    }

    KU_DEVICE_HOST static constexpr T repr(const T &value) noexcept {
        return value;
    }
};

template <>
struct ku_value_traits_impl<KuBool8> {
    KU_DEVICE_HOST static constexpr KuBool8 fromRepr(ku_char8_t value) noexcept {
        return KuBool8{value};
    }

    KU_DEVICE_HOST static constexpr KuBool8 fromBool(bool value) noexcept {
        return fromRepr(value ? KU_BOOL_TRUE : KU_BOOL_FALSE);
    }

    KU_DEVICE_HOST static constexpr std::optional<bool> boolean(const KuBool8 &value) noexcept {
        if (isNull(value)) {
            return std::nullopt;
        }
        return value.value != KU_BOOL_FALSE;
    }

    KU_DEVICE_HOST static constexpr KuBool8 dflt() noexcept {
        return fromRepr(KU_BOOL_FALSE);
    }

    KU_DEVICE_HOST static constexpr KuBool8 null() noexcept {
        return fromRepr(KU_BOOL_NULL);
    }

    KU_DEVICE_HOST static constexpr KuBool8 lowest() noexcept {
        return null();
    }

    KU_DEVICE_HOST static constexpr KuBool8 max() noexcept {
        return fromRepr(std::numeric_limits<ku_char8_t>::max());
    }

    KU_DEVICE_HOST static constexpr bool isNull(const KuBool8 &value) noexcept {
        return value.value == KU_BOOL_NULL;
    }

    KU_DEVICE_HOST static constexpr ku_char8_t repr(const KuBool8 &value) noexcept {
        return value.value;
    }
};

template <>
struct ku_value_traits_impl<KuVoid8> {
    KU_DEVICE_HOST static constexpr KuVoid8 dflt() noexcept {
        return KuVoid8{};
    }

    KU_DEVICE_HOST static constexpr KuVoid8 null() noexcept {
        return KuVoid8{};
    }

    KU_DEVICE_HOST static constexpr KuVoid8 lowest() noexcept {
        return KuVoid8{};
    }

    KU_DEVICE_HOST static constexpr KuVoid8 max() noexcept {
        return KuVoid8{};
    }

    KU_DEVICE_HOST static constexpr bool isNull(const KuVoid8 &) noexcept {
        return true;
    }
};

} // namespace detail

template <typename T>
struct ku_value_traits : detail::ku_value_traits_impl<std::remove_cvref_t<T>> {};

} // namespace kuai
