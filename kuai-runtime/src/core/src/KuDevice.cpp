#include <span>
#include <utility>

#include <kuai/core/KuDevice.h>
#include <kuai/core/KuHostTransfer.h>
#include <kuai/core/KuMemoryResource.h>

#include "core/KuDeviceAccess.h"
#include "core/KuDeviceGuard.h"
#include "core/KuMemoryResourceAccess.h"
#include "core/KuNativeMemoryResource.h"
#include "core/KuTensorBuilder.h"

namespace kuai {

class KuDevice::Impl {
    KU_DECL_API(KuDevice)

public:
    Impl(KuDevice                *api,
         ku_vendor_api_t          vendor,
         ku_device_info_t         info,
         ku_device_capabilities_t deviceCapabilities)
        : q_ptr(api), m_vendorApi(vendor), m_deviceInfo(info), m_capabilities(deviceCapabilities) {
    }

    ku_status_t initialize() noexcept {
        KuDeviceGuard guard(*q_ptr);
        if (guard.status() != KU_STATUS_SUCCESS) {
            return guard.status();
        }
        m_defaultStream = m_vendorApi.get_per_thread_stream(m_vendorApi.ctx);
        if (m_defaultStream == nullptr) {
            return KU_STATUS_INVALID_STATE;
        }
        auto resource = KuNativeMemoryResource::create(*q_ptr);
        if (!resource) {
            return resource.error();
        }
        m_resource = std::move(*resource);
        return KU_STATUS_SUCCESS;
    }

    ku_status_t shutdown() noexcept {
        m_hostTransfer.reset();
        const auto status = m_resource != nullptr
                                ? detail::KuMemoryResourceAccess::shutdown(*m_resource)
                                : KU_STATUS_SUCCESS;
        m_resource.reset();
        return status;
    }

    ku_vendor_api_t                   m_vendorApi;
    ku_device_info_t                  m_deviceInfo;
    ku_stream_t                       m_defaultStream = nullptr;
    std::unique_ptr<KuMemoryResource> m_resource;
    ku_device_capabilities_t          m_capabilities;
    std::unique_ptr<KuHostTransfer>   m_hostTransfer;
};

namespace detail {

std::expected<std::unique_ptr<KuDevice>, ku_status_t>
KuDeviceAccess::create(const ku_vendor_api_t          &vendorApi,
                       ku_device_id_t                  deviceId,
                       ku_device_type_t                deviceType,
                       const ku_device_capabilities_t &capabilities) noexcept {
    int  deviceCount = 0;
    auto status = vendorApi.get_device_count(vendorApi.ctx, &deviceCount);
    if (status != KU_STATUS_SUCCESS) {
        return std::unexpected(status);
    }
    if (deviceId < 0 || deviceId >= deviceCount) {
        return std::unexpected(KU_STATUS_OUT_OF_RANGE);
    }

    try {
        auto device = std::unique_ptr<KuDevice>(
            new KuDevice(vendorApi, {deviceType, deviceId}, capabilities));
        const auto initializeStatus = device->initialize();
        if (initializeStatus != KU_STATUS_SUCCESS) {
            return std::unexpected(initializeStatus);
        }
        return device;
    } catch (const std::bad_alloc &) {
        return std::unexpected(KU_STATUS_OUT_OF_HOST_MEMORY);
    } catch (...) {
        return std::unexpected(KU_STATUS_INTERNAL_ERROR);
    }
}

void KuDeviceAccess::installHostTransfer(KuDevice                       &device,
                                         std::unique_ptr<KuHostTransfer> transfer) noexcept {
    device.installHostTransfer(std::move(transfer));
}

ku_status_t KuDeviceAccess::shutdown(KuDevice &device) noexcept {
    return device.shutdown();
}

} // namespace detail

KuDevice::KuDevice(ku_vendor_api_t          vendorApi,
                   ku_device_info_t         deviceInfo,
                   ku_device_capabilities_t capabilities)
    : d_ptr(std::make_unique<Impl>(this, vendorApi, deviceInfo, capabilities)) {
}

KuDevice::~KuDevice() {
    (void)d_ptr->shutdown();
}

ku_status_t KuDevice::initialize() noexcept {
    return d_ptr->initialize();
}

void KuDevice::installHostTransfer(std::unique_ptr<KuHostTransfer> transfer) noexcept {
    KU_ASSERT(transfer != nullptr, "a device requires a non-null default host transfer");
    KU_ASSERT(&transfer->getDevice() == this, "a host transfer must identify its receiving device");
    KU_ASSERT(d_ptr->m_hostTransfer == nullptr,
              "a device host transfer may only be installed once");
    d_ptr->m_hostTransfer = std::move(transfer);
}

ku_status_t KuDevice::shutdown() noexcept {
    return d_ptr->shutdown();
}

const ku_vendor_api_t &KuDevice::getVendorApi() const noexcept {
    return d_ptr->m_vendorApi;
}

ku_device_info_t KuDevice::getDeviceInfo() const noexcept {
    return d_ptr->m_deviceInfo;
}

ku_device_capabilities_t KuDevice::getCapabilities() const noexcept {
    return d_ptr->m_capabilities;
}

KuMemoryResource &KuDevice::getDefaultMemoryResource() const noexcept {
    KU_ASSERT(d_ptr->m_resource != nullptr,
              "a published device must have a default memory resource");
    return *d_ptr->m_resource;
}

KuHostTransfer &KuDevice::getHostTransfer() const noexcept {
    KU_ASSERT(d_ptr->m_hostTransfer != nullptr,
              "a published device must have a default host transfer");
    return *d_ptr->m_hostTransfer;
}

ku_stream_t KuDevice::getDefaultStream() noexcept {
    KU_ASSERT(d_ptr->m_defaultStream != nullptr, "a published device must have a default stream");
    return d_ptr->m_defaultStream;
}

ku_status_t KuDevice::synchronize(ku_stream_t stream) noexcept {
    KU_ASSERT(stream != nullptr, "synchronize requires a non-null stream");
    KuDeviceGuard guard(*this);
    if (guard.status() != KU_STATUS_SUCCESS) {
        return guard.status();
    }
    return d_ptr->m_vendorApi.stream_synchronize(d_ptr->m_vendorApi.ctx, stream);
}

ku_status_t KuDevice::flush() noexcept {
    getHostTransfer().waitIdle();
    return synchronize(getDefaultStream());
}

ku_status_t KuDevice::copyAsync(void                *dst,
                                const void          *src,
                                ku_size_t            bytes,
                                ku_memcpy_kind_t     kind,
                                ku_stream_t          dependencyStream,
                                ku_sp<KuCompletion> &out) noexcept {
    KU_ASSERT(dependencyStream != nullptr, "copyAsync requires a non-null dependency stream");
    return getHostTransfer().copyAsync(dst, src, bytes, kind, dependencyStream, out);
}

ku_status_t KuDevice::createTensor(ku_primitive_type_t dt,
                                   const ku_size_t    *sz,
                                   ku_size_t           dim,
                                   ku_size_t           capacity,
                                   ku_sp<KuTensor>    &out) noexcept {
    KU_ASSERT(dim <= KuTensor::MaxRank);
    KU_ASSERT(dim == 0 || sz != nullptr);
    out.reset();

    KuDeviceGuard guard(*this);
    if (guard.status() != KU_STATUS_SUCCESS) {
        return guard.status();
    }
    const KuTensorBuilder tensorBuilder(*this);
    return tensorBuilder.create(dt, std::span<const ku_size_t>(sz, static_cast<std::size_t>(dim)),
                                capacity, out);
}

} // namespace kuai
