#pragma once

#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

#include <kuai/core/KuContext.h>
#include <kuai/core/KuDeviceBuffer.h>
#include <kuai/core/KuDeviceGuard.h>
#include <kuai/core/KuTensor.h>

namespace kuai {

// Constructs backend-neutral storage and publishes it as a kuai tensor only
// after shape and capacity are established.
class KuTensorBuilder {
public:
    explicit KuTensorBuilder(KuDevice &device) noexcept
        : m_deviceObject(&device), m_stream(device.getDefaultStream()),
          m_resource(&device.getDefaultMemoryResource()) {
        assertIdentity();
    }

    explicit KuTensorBuilder(const KuContext &context) noexcept
        : m_deviceObject(&context.getDevice()), m_stream(context.getStream()),
          m_resource(&context.getMemoryResource()) {
        assertIdentity();
    }

    template <typename Context>
        requires requires(const Context &context) {
            context.m_stream;
            context.m_resource;
            context.m_deviceObject;
        }
    explicit KuTensorBuilder(const Context &context) noexcept
        : m_deviceObject(context.m_deviceObject), m_stream(context.m_stream),
          m_resource(context.m_resource) {
        assertIdentity();
    }

    template <typename T>
    ku_status_t storage(ku_size_t size, KuDeviceBuffer<T> &out) const noexcept {
        return storage(size, size, out);
    }

    template <typename T>
    ku_status_t storage(ku_size_t size, ku_size_t capacity, KuDeviceBuffer<T> &out) const noexcept {
        out = KuDeviceBuffer<T>();
        const auto validationStatus = validateStorageSize<T>(size, capacity);
        if (validationStatus != KU_STATUS_SUCCESS) {
            return validationStatus;
        }
        return KuDeviceBuffer<T>::allocate(*m_resource, m_stream, capacity, out);
    }

    template <typename Storage>
    ku_status_t
    wrap(Storage &&storage, std::span<const ku_size_t> shape, ku_sp<KuTensor> &out) const noexcept {
        using StorageType = std::decay_t<Storage>;
        using T = typename StorageType::value_type;
        out.reset();
        if (shape.size() > KuTensor::MaxRank) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
        ku_size_t  elementCountValue = 0;
        const auto countStatus = elementCount(shape, elementCountValue);
        if (countStatus != KU_STATUS_SUCCESS) {
            return countStatus;
        }
        KU_ASSERT(storage.size() >= elementCountValue,
                  "Storage allocation must cover the tensor shape");
        try {
            auto storageOwner = std::make_shared<StorageType>(std::forward<Storage>(storage));
            KuTensorOwner owner = storageOwner;
            out = ku_make_sp<KuTensor>(
                *m_deviceObject, KuTypeTraits<T>::primitiveType, storageOwner->data(), shape,
                static_cast<ku_size_t>(storageOwner->size()), std::move(owner));
            return KU_STATUS_SUCCESS;
        } catch (const std::bad_alloc &) {
            return KU_STATUS_OUT_OF_HOST_MEMORY;
        } catch (...) {
            return KU_STATUS_INTERNAL_ERROR;
        }
    }

    template <typename T>
    ku_status_t create(std::span<const ku_size_t> shape,
                       ku_size_t                  capacity,
                       ku_sp<KuTensor>           &out) const noexcept {
        out.reset();
        if (shape.size() > KuTensor::MaxRank) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
        ku_size_t size = 0;
        auto      status = elementCount(shape, size);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        KuDeviceBuffer<T> storageValue;
        status = storage<T>(size, capacity, storageValue);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        return wrap(std::move(storageValue), shape, out);
    }

    ku_status_t create(ku_primitive_type_t        dataType,
                       std::span<const ku_size_t> shape,
                       ku_size_t                  capacity,
                       ku_sp<KuTensor>           &out) const noexcept {
        out.reset();
        switch (dataType) {
            case KU_PRIMITIVE_BOOLEAN:
                return create<KuBool8>(shape, capacity, out);
            case KU_PRIMITIVE_BYTE:
                return create<KuChar8>(shape, capacity, out);
            case KU_PRIMITIVE_I16:
                return create<KuI16>(shape, capacity, out);
            case KU_PRIMITIVE_I32:
                return create<KuI32>(shape, capacity, out);
            case KU_PRIMITIVE_I64:
                return create<KuI64>(shape, capacity, out);
            case KU_PRIMITIVE_F32:
                return create<KuF32>(shape, capacity, out);
            case KU_PRIMITIVE_F64:
                return create<KuF64>(shape, capacity, out);
            default:
                return KU_STATUS_NOT_SUPPORTED;
        }
    }

    ku_status_t clone(const KuTensor &source, ku_sp<KuTensor> &out) const noexcept {
        auto status = create(source.getType(), source.shape(), source.capacity(), out);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        status = copyData(*out, source);
        if (status != KU_STATUS_SUCCESS) {
            out.reset();
        }
        return status;
    }

private:
    template <typename T>
    static ku_status_t validateStorageSize(ku_size_t size, ku_size_t capacity) noexcept {
        if (capacity < size) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
        constexpr auto alignmentSlack = KuMemoryResource::DefaultAlignment - 1;
        constexpr auto maxAlignedBytes = std::numeric_limits<std::size_t>::max() - alignmentSlack;
        if (capacity > maxAlignedBytes / sizeof(T)) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
        return KU_STATUS_SUCCESS;
    }

    static ku_status_t elementCount(std::span<const ku_size_t> shape, ku_size_t &out) noexcept {
        out = 1;
        for (const auto extent : shape) {
            if (extent != 0 && out > std::numeric_limits<ku_size_t>::max() / extent) {
                out = 0;
                return KU_STATUS_INVALID_ARGUMENT;
            }
            out *= extent;
        }
        return KU_STATUS_SUCCESS;
    }

    ku_status_t copyData(KuTensor &destination, const KuTensor &source) const noexcept {
        KU_ASSERT(destination.getType() == source.getType(), "tensor clone dtype mismatch");
        KU_ASSERT(destination.bytes() == source.bytes(), "tensor clone size mismatch");
        if (source.bytes() == 0) {
            return KU_STATUS_SUCCESS;
        }
        KU_ASSERT(&source.getDevice() == m_deviceObject,
                  "cross-device tensor clone must use the transfer scheduler");
        const auto   &api = m_deviceObject->getVendorApi();
        KuDeviceGuard guard(*m_deviceObject);
        if (guard.status() != KU_STATUS_SUCCESS) {
            return guard.status();
        }
        return api.memcpy_async(api.ctx, destination.data(), source.data(), source.bytes(),
                                KU_MEMCPY_DEVICE_TO_DEVICE, m_stream);
    }

    void assertIdentity() const noexcept {
        KU_ASSERT(m_stream != nullptr, "tensor construction requires a non-null default stream");
        KU_ASSERT(m_deviceObject->getDefaultStream() == m_stream,
                  "tensor construction must use the device default stream");
        KU_ASSERT(&m_deviceObject->getDefaultMemoryResource() == m_resource,
                  "tensor construction must use the device default memory resource");
        KU_ASSERT(&m_resource->getDevice() == m_deviceObject,
                  "tensor memory resource must belong to its device");
    }

    KuDevice         *m_deviceObject;
    ku_stream_t       m_stream;
    KuMemoryResource *m_resource;
};

} // namespace kuai
