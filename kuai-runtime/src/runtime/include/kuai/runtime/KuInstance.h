#pragma once

#include <memory>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuDevice.h>
#include <kuai/core/KuTypes.h>
#include <kuai/kuai_c/ku_builtin.h>
#include <kuai/kuai_c/ku_runtime.h>

namespace kuai {

class KuInstanceCapabilityState;
class KuInstanceRegistry;

// Backend-neutral runtime instance. Backend variation is supplied by one stable
// C vendor module descriptor rather than C++ inheritance.
class KuInstance final {
    KU_DECL_IMPL()

public:
    KuInstance(const KuInstance &) = delete;
    KuInstance &operator=(const KuInstance &) = delete;
    KuInstance(KuInstance &&) = delete;
    KuInstance &operator=(KuInstance &&) = delete;

    ~KuInstance();

    // Device instances are owned and cached by the instance. Returned pointers
    // are borrowed and stable for the instance lifetime.
    [[nodiscard]] KuDevice       *getDefaultDevice() noexcept;
    [[nodiscard]] const KuDevice *getDefaultDevice() const noexcept;
    ku_status_t                   getDevice(ku_device_id_t deviceId, KuDevice **out) noexcept;

    ku_status_t
    getBuiltinInfo(ku_builtin_info_t *out, ku_size_t capacity, ku_size_t *outCount) noexcept;

    // Writes an invocable target on success. A missing builtin returns
    // KU_STATUS_NOT_FOUND and leaves the output slot unchanged.
    ku_status_t getKuProcAddress(ku_string_view_t name, ku_call_target_t *out) noexcept;

    ku_status_t getCapabilities(ku_instance_capabilities_t *out) noexcept;

    // Instance-wide fallback barrier. It forwards to every cached device and
    // returns the first failure.
    ku_status_t flush() noexcept;

private:
    friend class KuInstanceRegistry;

    static ku_status_t create(std::unique_ptr<KuInstanceCapabilityState> capabilityState,
                              ku_device_id_t                             defaultDeviceId,
                              const ku_vendor_module_t                  &vendorModule,
                              std::unique_ptr<KuInstance>               &out) noexcept;

    KuInstance(std::unique_ptr<KuInstanceCapabilityState> capabilityState,
               const ku_vendor_module_t                  &vendorModule);

    ku_status_t shutdown() noexcept;
};

} // namespace kuai
