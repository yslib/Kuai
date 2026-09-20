#pragma once

#include <expected>
#include <memory>

#include <kuai/core/KuDevice.h>

namespace kuai {

class KuHostTransfer;

namespace detail {

class KuDeviceAccess final {
public:
    // Host-private factory; callers preserve the C API's ku_status_t contract.
    static std::expected<std::unique_ptr<KuDevice>, ku_status_t>
    create(const ku_vendor_api_t          &vendorApi,
           ku_device_id_t                  deviceId,
           ku_device_type_t                deviceType,
           const ku_device_capabilities_t &capabilities) noexcept;

    static void installHostTransfer(KuDevice                       &device,
                                    std::unique_ptr<KuHostTransfer> transfer) noexcept;

    static ku_status_t shutdown(KuDevice &device) noexcept;
};

} // namespace detail
} // namespace kuai
