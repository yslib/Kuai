#pragma once

#include <kuai/core/KuDevice.h>

namespace kuai {

// Restores the process/thread-local native device selected before construction.
// The device owns the vendor API and ctx; this guard only borrows the device.
class KuDeviceGuard {
public:
    explicit KuDeviceGuard(KuDevice &device) noexcept : m_device(device) {
        const auto &api = m_device.getVendorApi();
        const auto  target = m_device.getDeviceInfo().device_id;
        m_status = api.get_device(api.ctx, &m_original);
        if (m_status != KU_STATUS_SUCCESS || m_original == target) {
            return;
        }
        m_status = api.set_device(api.ctx, target);
        m_changed = m_status == KU_STATUS_SUCCESS;
    }

    KuDeviceGuard(const KuDeviceGuard &) = delete;
    KuDeviceGuard &operator=(const KuDeviceGuard &) = delete;
    KuDeviceGuard(KuDeviceGuard &&) = delete;
    KuDeviceGuard &operator=(KuDeviceGuard &&) = delete;

    ~KuDeviceGuard() {
        if (m_changed) {
            const auto &api = m_device.getVendorApi();
            KU_VERIFY(api.set_device(api.ctx, m_original) == KU_STATUS_SUCCESS,
                      "failed to restore the native device");
        }
    }

    [[nodiscard]] ku_status_t status() const noexcept {
        return m_status;
    }

private:
    KuDevice      &m_device;
    ku_device_id_t m_original{};
    ku_status_t    m_status = KU_STATUS_SUCCESS;
    bool           m_changed = false;
};

} // namespace kuai
