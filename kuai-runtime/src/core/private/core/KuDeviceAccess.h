#pragma once

#include <memory>

#include <kuai/core/KuDevice.h>

namespace kuai {

class KuHostTransfer;

namespace detail {

class KuDeviceAccess final {
public:
    static ku_status_t create(const ku_vendor_api_t          &vendorApi,
                              ku_device_id_t                  deviceId,
                              ku_device_type_t                deviceType,
                              const ku_device_capabilities_t &capabilities,
                              std::unique_ptr<KuDevice>      &out) noexcept;

    static void installHostTransfer(KuDevice                       &device,
                                    std::unique_ptr<KuHostTransfer> transfer) noexcept;

    static ku_status_t shutdown(KuDevice &device) noexcept;
};

} // namespace detail
} // namespace kuai
