#pragma once

#include <kuai/core/KuDevice.h>
#include <kuai/core/KuTensor.h>

namespace kuai {

struct KuContext {
    explicit KuContext(KuDevice &device) noexcept : m_device(device) {
    }

    [[nodiscard]] KuDevice &getDevice() const noexcept {
        return m_device;
    }

    [[nodiscard]] ku_device_id_t getDeviceId() const noexcept {
        return m_device.getDeviceInfo().device_id;
    }

    [[nodiscard]] ku_stream_t getStream() const noexcept {
        const auto stream = m_device.getDefaultStream();
        KU_ASSERT(stream != nullptr, "a kuai device must publish a non-null default stream");
        return stream;
    }

    [[nodiscard]] KuMemoryResource &getMemoryResource() const noexcept {
        return m_device.getDefaultMemoryResource();
    }

    ku_status_t createTensor(ku_primitive_type_t dataType,
                             const ku_size_t    *shape,
                             ku_size_t           rank,
                             ku_size_t           capacity,
                             ku_sp<KuTensor>    &out) const noexcept {
        return m_device.createTensor(dataType, shape, rank, capacity, out);
    }

private:
    // The borrowed device is the single source of device identity, the default
    // memory resource, and the default execution stream.
    KuDevice &m_device;
};

} // namespace kuai
