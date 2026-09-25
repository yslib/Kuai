#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <utility>

#include <kuai/core/KuContext.h>
#include <kuai/core/KuCore.h>
#include <kuai/core/KuDeviceBuffer.h>
#include <kuai/core/KuMemoryResource.h>
#include <kuai/core/KuTensorBuilder.h>
#include <kuai/ktl/KuDeviceMarker.h>
#include <kuai/ktl/KuResult.h>
#include <kuai/vendor/KuTensorDesc.h>

namespace kuai {

// Short-lived typed snapshot used inside one backend implementation. All state
// is derived from one KuDevice so Vendor remains available for algorithm
// deduction without duplicating mutable device state in KuContext.
template <typename Vendor>
struct KuVendorContext {
    using vendor_type = Vendor;

    explicit KuVendorContext(const KuContext &context) noexcept
        : m_ctx(vendorContext(context)), m_dev(context.getDeviceId()),
          m_stream(context.getStream()), m_resource(&context.getMemoryResource()),
          m_deviceObject(&context.getDevice()) {
        KU_ASSERT(&m_resource->getDevice() == m_deviceObject,
                  "KuContext memory resource must belong to its bound kuai device");
    }

    void             *m_ctx;
    ku_device_id_t    m_dev;
    ku_stream_t       m_stream;
    KuMemoryResource *m_resource;
    KuDevice         *m_deviceObject;

    void memcpyAsync(void *dst, const void *src, size_t count, ku_memcpy_kind_t kind) const {
        Vendor::kuMemcpyAsync(m_ctx, dst, src, count, kind, m_stream);
    }

    void memcpy(void *dst, const void *src, size_t count, ku_memcpy_kind_t kind) const {
        Vendor::kuMemcpy(m_ctx, dst, src, count, kind);
    }

    template <typename T, typename Extents>
    KuResult<ku_sp<KuTensor>, ku_status_t> createTensor(const KuTensorDesc<Extents> &desc) const {
        auto            shape = runtimeShape(desc.extents());
        ku_sp<KuTensor> out;
        const auto      status = KuTensorBuilder(*this).template create<T>(
            std::span<const ku_size_t>(shape.data(), shape.size()), desc.capacity(), out);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        return out;
    }

    template <typename Extents>
    KuResult<ku_sp<KuTensor>, ku_status_t> createTensor(ku_primitive_type_t          dataType,
                                                        const KuTensorDesc<Extents> &desc) const {
        auto            shape = runtimeShape(desc.extents());
        ku_sp<KuTensor> out;
        const auto      status = KuTensorBuilder(*this).create(
            dataType, std::span<const ku_size_t>(shape.data(), shape.size()), desc.capacity(), out);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        return out;
    }

    template <typename T>
    KuResult<KuDeviceBuffer<T>, ku_status_t> createStorage(ku_size_t size) const {
        KuDeviceBuffer<T> out;
        const auto        status = KuTensorBuilder(*this).template storage<T>(size, out);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        return std::move(out);
    }

    template <typename T>
    KuResult<KuDeviceBuffer<T>, ku_status_t> createStorage(ku_size_t size,
                                                           ku_size_t capacity) const {
        KuDeviceBuffer<T> out;
        const auto        status = KuTensorBuilder(*this).template storage<T>(size, capacity, out);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        return std::move(out);
    }

    template <typename Storage, typename Extents>
    KuResult<ku_sp<KuTensor>, ku_status_t> wrapTensor(Storage      &&storage,
                                                      const Extents &extents) const {
        auto            shape = runtimeShape(extents);
        ku_sp<KuTensor> out;
        const auto      status = KuTensorBuilder(*this).wrap(
            std::forward<Storage>(storage), std::span<const ku_size_t>(shape.data(), shape.size()),
            out);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        return out;
    }

private:
    static void *vendorContext(const KuContext &context) noexcept {
        return context.getDevice().getVendorApi().ctx;
    }

    template <typename Extents>
    static auto runtimeShape(const Extents &extents) {
        std::array<ku_size_t, Extents::rank()> shape{};
        for (std::size_t i = 0; i < Extents::rank(); ++i) {
            shape[i] = static_cast<ku_size_t>(extents.extent(i));
        }
        return shape;
    }
};

} // namespace kuai
