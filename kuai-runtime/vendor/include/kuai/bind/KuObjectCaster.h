#pragma once

#include <type_traits>

#include <kuai/bind/KuArgCaster.h>
#include <kuai/core/KuCallable.h>
#include <kuai/core/KuContext.h>
#include <kuai/core/KuDeviceData.h>
#include <kuai/core/KuFrameContext.h>
#include <kuai/core/KuObject.h>
#include <kuai/core/KuPointer.h>
#include <kuai/core/KuScalar.h>

#include "kuai_c/KuCHandle.h"

namespace kuai::bind {

template <typename T>
struct KuArgCaster<ku_object_t, ::ku_sp<T>> {
    static_assert(std::is_base_of_v<KuObject, T>, "ku_sp return values must contain an KuObject");

    using storage_type = ku_object_t;
    using value_type = ::ku_sp<T>;
    value_type m_value;

    void store(storage_type *object) noexcept {
        *object = capi::toHandle<storage_type>(m_value.detach());
    }
};

template <>
struct KuArgCaster<ku_object_t, KuObject *> {
    using storage_type = ku_object_t;
    using value_type = KuObject *;
    value_type m_value;

    bool load(const storage_type object) noexcept {
        m_value = capi::fromHandle(object);
        return true;
    }

    void store(storage_type *object) noexcept {
        *object = capi::toHandle<storage_type>(m_value);
    }
};

template <typename T>
    requires std::is_base_of_v<KuObject, std::remove_cv_t<T>>
struct KuArgCaster<ku_object_t, T &> {
    using storage_type = ku_object_t;
    using object_type = std::remove_cv_t<T>;
    using value_type = T *;
    value_type m_value{nullptr};

    bool load(const storage_type objectHandle) noexcept {
        auto *object = capi::fromHandle(objectHandle);
        if (object == nullptr) {
            m_value = nullptr;
            return false;
        }
        if constexpr (std::is_same_v<object_type, KuObject>) {
            m_value = object;
        } else {
            m_value = object->template as<object_type>();
        }
        return m_value != nullptr;
    }
};

namespace detail {

template <typename T>
inline constexpr bool ku_is_scalar_value_v =
    std::is_same_v<T, KuVoid8> || std::is_same_v<T, KuBool> || std::is_same_v<T, KuChar8>
    || std::is_same_v<T, KuI16> || std::is_same_v<T, KuI32> || std::is_same_v<T, KuI64>
    || std::is_same_v<T, KuF32> || std::is_same_v<T, KuF64>;

} // namespace detail

template <typename T>
    requires detail::ku_is_scalar_value_v<std::remove_cvref_t<T>>
struct KuArgCaster<ku_object_t, T> {
    using storage_type = ku_object_t;
    using value_type = std::remove_cvref_t<T>;
    value_type m_value{};

    bool load(const storage_type objectHandle) noexcept {
        auto *object = capi::fromHandle(objectHandle);
        auto *scalar = object == nullptr ? nullptr : object->as<KuScalar>();
        if (scalar == nullptr) {
            return false;
        }
        auto scalarValue = scalar->getIf<value_type>();
        if (!scalarValue) {
            return false;
        }
        m_value = *scalarValue;
        return true;
    }
};

template <>
struct KuArgCaster<ku_object_t, KuDeviceData *> {
    using storage_type = ku_object_t;
    using value_type = KuDeviceData *;
    value_type m_value{nullptr};

    bool load(const storage_type objectHandle) noexcept {
        auto *object = capi::fromHandle(objectHandle);
        m_value = object == nullptr ? nullptr : object->as<KuDeviceData>();
        return m_value != nullptr;
    }
};

template <>
struct KuArgCaster<ku_object_t, KuCallable *> {
    using storage_type = ku_object_t;
    using value_type = KuCallable *;
    value_type m_value{nullptr};

    bool load(const storage_type objectHandle) noexcept {
        auto *object = capi::fromHandle(objectHandle);
        m_value = object == nullptr ? nullptr : object->as<KuCallable>();
        return m_value != nullptr;
    }
};

template <>
struct KuArgCaster<ku_frame_ctx_t, KuFrameContext *> {
    using storage_type = ku_frame_ctx_t;
    using value_type = KuFrameContext *;
    value_type m_value{nullptr};

    bool load(storage_type context) noexcept {
        m_value = capi::fromHandle(context);
        return m_value != nullptr;
    }

    void store(storage_type *context) noexcept {
        *context = capi::toHandle<storage_type>(m_value);
    }
};

template <>
struct KuArgCaster<ku_frame_ctx_t, KuContext *> {
    using storage_type = ku_frame_ctx_t;
    using value_type = KuContext *;
    value_type m_value{nullptr};

    bool load(storage_type context) noexcept {
        auto *frameContext = capi::fromHandle(context);
        m_value = frameContext == nullptr ? nullptr : frameContext->context();
        return m_value != nullptr;
    }
};

} // namespace kuai::bind
