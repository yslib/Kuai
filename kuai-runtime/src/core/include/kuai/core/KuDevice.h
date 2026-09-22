#pragma once

#include <memory>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuTensor.h>
#include <kuai/core/KuTypes.h>
#include <kuai/kuai_c/ku_runtime.h>

namespace kuai {

class KuInstance;
class KuMemoryResource;
class KuHostTransfer;
class KuCompletion;
namespace detail {
class KuDeviceAccess;
}

// A persistent backend-neutral device owned by KuInstance. Platform operations
// are dispatched through the copied C vendor API rather than C++ inheritance.
// Returned references and native handles are borrowed and remain stable for
// the owning instance's lifetime.
class KuDevice final {
    KU_DECL_IMPL()

public:
    KuDevice(const KuDevice &) = delete;
    KuDevice &operator=(const KuDevice &) = delete;
    KuDevice(KuDevice &&) = delete;
    KuDevice &operator=(KuDevice &&) = delete;

    ~KuDevice();

    [[nodiscard]] KuInstance              *getInstance() const noexcept;
    [[nodiscard]] const ku_vendor_api_t   &getVendorApi() const noexcept;
    [[nodiscard]] ku_device_info_t         getDeviceInfo() const noexcept;
    [[nodiscard]] ku_device_capabilities_t getCapabilities() const noexcept;
    /* Borrowed and stable for this device's lifetime. */
    [[nodiscard]] KuMemoryResource &getDefaultMemoryResource() const noexcept;
    [[nodiscard]] KuHostTransfer   &getHostTransfer() const noexcept;
    [[nodiscard]] ku_stream_t       getDefaultStream() noexcept;

    ku_status_t synchronize(ku_stream_t stream) noexcept;
    // Device-wide fallback barrier. It waits for transfer work and the default
    // execution stream owned by this device.
    ku_status_t flush() noexcept;

    ku_status_t copyAsync(void                *dst,
                          const void          *src,
                          ku_size_t            bytes,
                          ku_memcpy_kind_t     kind,
                          ku_stream_t          dependencyStream,
                          ku_sp<KuCompletion> &out) noexcept;

    ku_status_t createTensor(ku_primitive_type_t dt,
                             const ku_size_t    *sz,
                             ku_size_t           dim,
                             ku_size_t           capacity,
                             ku_sp<KuTensor>    &out) noexcept;

private:
    friend class detail::KuDeviceAccess;

    KuDevice(KuInstance              &instance,
             ku_vendor_api_t          vendorApi,
             ku_device_info_t         deviceInfo,
             ku_device_capabilities_t capabilities);

    ku_status_t initialize() noexcept;
    void        installHostTransfer(std::unique_ptr<KuHostTransfer> transfer) noexcept;
    ku_status_t shutdown() noexcept;
};

} // namespace kuai
