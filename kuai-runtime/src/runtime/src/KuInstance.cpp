#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <kuai/core/KuHostTransfer.h>
#include <kuai/runtime/KuInstance.h>

#include "core/KuDeviceAccess.h"
#include "runtime/KuBuiltinRegistry.h"
#include "runtime/KuHostTransferFactory.h"
#include "runtime/KuScheduler.h"

namespace kuai {

class KuInstance::Impl {
    KU_DECL_API(KuInstance)

public:
    Impl(KuInstance                                *api,
         std::unique_ptr<KuInstanceCapabilityState> capabilities,
         const ku_vendor_module_t                  &module)
        : q_ptr(api), m_capabilityState(std::move(capabilities)), m_vendorModule(module) {
        KU_ASSERT(m_capabilityState);
        const auto &vendor = m_vendorModule.vendor_api;
        KU_ASSERT(vendor.get_device_count);
        KU_ASSERT(vendor.get_device);
        KU_ASSERT(vendor.set_device);
        KU_ASSERT(vendor.get_per_thread_stream);
        KU_ASSERT(vendor.stream_create);
        KU_ASSERT(vendor.stream_destroy);
        KU_ASSERT(vendor.stream_query);
        KU_ASSERT(vendor.stream_synchronize);
        KU_ASSERT(vendor.malloc_device);
        KU_ASSERT(vendor.free_device);
        KU_ASSERT(vendor.memory_pool_create);
        KU_ASSERT(vendor.memory_pool_destroy);
        KU_ASSERT(vendor.malloc_from_pool_async);
        KU_ASSERT(vendor.free_async);
        KU_ASSERT(vendor.malloc_host);
        KU_ASSERT(vendor.free_host);
        KU_ASSERT(vendor.memcpy);
        KU_ASSERT(vendor.memcpy_async);
        KU_ASSERT(m_vendorModule.records_begin != nullptr);
        KU_ASSERT(m_vendorModule.records_end != nullptr);
        KU_ASSERT(m_vendorModule.records_begin <= m_vendorModule.records_end);

        const auto builder = m_builtinRegistry.builder();
        for (const auto *record = m_vendorModule.records_begin;
             record != m_vendorModule.records_end; ++record) {
            KU_ASSERT(record->name != nullptr);
            KU_ASSERT(record->loader != nullptr);
            const auto loadStatus = record->loader(&builder);
            if (loadStatus != KU_STATUS_SUCCESS) {
                throw detail::KuStatusError(loadStatus);
            }
        }
    }

    ku_status_t initializeDevices(ku_device_id_t defaultDeviceId) noexcept {
        const auto &capabilities = m_capabilityState->effective();
        try {
            m_devices.reserve(capabilities.device_count);
        } catch (const std::bad_alloc &) {
            return KU_STATUS_OUT_OF_HOST_MEMORY;
        } catch (...) {
            return KU_STATUS_INTERNAL_ERROR;
        }

        for (std::size_t index = 0; index < capabilities.device_count; ++index) {
            const auto &deviceCapabilities = capabilities.devices[index];
            auto        deviceResult = detail::KuDeviceAccess::create(
                m_vendorModule.vendor_api, deviceCapabilities.device_id, m_vendorModule.device_type,
                deviceCapabilities);
            if (!deviceResult) {
                clearDevices();
                return deviceResult.error();
            }
            auto device = std::move(*deviceResult);
            KU_ASSERT(device != nullptr);

            std::unique_ptr<KuHostTransfer> transfer;
            const auto status = detail::createDefaultHostTransfer(capabilities, *device, transfer);
            if (status != KU_STATUS_SUCCESS) {
                clearDevices();
                return status;
            }
            KU_ASSERT(transfer != nullptr);
            KU_ASSERT(&transfer->getDevice() == device.get());
            detail::KuDeviceAccess::installHostTransfer(*device, std::move(transfer));

            try {
                auto *const rawDevice = device.get();
                const auto [position, inserted] =
                    m_devices.emplace(deviceCapabilities.device_id, std::move(device));
                if (!inserted) {
                    clearDevices();
                    return KU_STATUS_INVALID_ARGUMENT;
                }
                if (deviceCapabilities.device_id == defaultDeviceId) {
                    m_defaultDevice = rawDevice;
                }
                (void)position;
            } catch (const std::bad_alloc &) {
                clearDevices();
                return KU_STATUS_OUT_OF_HOST_MEMORY;
            } catch (...) {
                clearDevices();
                return KU_STATUS_INTERNAL_ERROR;
            }
        }
        if (m_defaultDevice == nullptr) {
            clearDevices();
            return KU_STATUS_INVALID_ARGUMENT;
        }
        return KU_STATUS_SUCCESS;
    }

    ku_status_t getDevice(ku_device_id_t deviceId, KuDevice **out) noexcept {
        KU_ASSERT(out != nullptr, "getDevice requires a non-null output slot");
        *out = nullptr;

        const auto found = m_devices.find(deviceId);
        if (found == m_devices.end()) {
            return KU_STATUS_OUT_OF_RANGE;
        }
        *out = found->second.get();
        return KU_STATUS_SUCCESS;
    }

    ku_status_t shutdown() noexcept {
        m_defaultDevice = nullptr;
        ku_status_t firstFailure = KU_STATUS_SUCCESS;
        for (auto &[deviceId, device] : m_devices) {
            (void)deviceId;
            const auto status = detail::KuDeviceAccess::shutdown(*device);
            if (firstFailure == KU_STATUS_SUCCESS && status != KU_STATUS_SUCCESS) {
                firstFailure = status;
            }
        }
        m_devices.clear();
        return firstFailure;
    }

    void clearDevices() noexcept {
        m_defaultDevice = nullptr;
        m_devices.clear();
    }

    std::unique_ptr<KuInstanceCapabilityState>                    m_capabilityState;
    KuBuiltinRegistry                                             m_builtinRegistry;
    KuDevice                                                     *m_defaultDevice = nullptr;
    ku_vendor_module_t                                            m_vendorModule;
    std::unordered_map<ku_device_id_t, std::unique_ptr<KuDevice>> m_devices;
};

KuInstance::KuInstance(std::unique_ptr<KuInstanceCapabilityState> capabilityState,
                       const ku_vendor_module_t                  &vendorModule)
    : d_ptr(std::make_unique<Impl>(this, std::move(capabilityState), vendorModule)) {
}

ku_status_t KuInstance::create(std::unique_ptr<KuInstanceCapabilityState> capabilityState,
                               ku_device_id_t                             defaultDeviceId,
                               const ku_vendor_module_t                  &vendorModule,
                               std::unique_ptr<KuInstance>               &out) noexcept {
    KU_ASSERT(capabilityState != nullptr);
    out.reset();
    try {
        auto instance =
            std::unique_ptr<KuInstance>(new KuInstance(std::move(capabilityState), vendorModule));
        const auto status = instance->d_ptr->initializeDevices(defaultDeviceId);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        out = std::move(instance);
        return KU_STATUS_SUCCESS;
    } catch (const detail::KuStatusError &error) {
        return error.status();
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
}

KuInstance::~KuInstance() {
    (void)d_ptr->shutdown();
}

ku_status_t KuInstance::shutdown() noexcept {
    return d_ptr->shutdown();
}

KuDevice *KuInstance::getDefaultDevice() noexcept {
    KU_ASSERT(d_ptr->m_defaultDevice != nullptr);
    return d_ptr->m_defaultDevice;
}

const KuDevice *KuInstance::getDefaultDevice() const noexcept {
    KU_ASSERT(d_ptr->m_defaultDevice != nullptr);
    return d_ptr->m_defaultDevice;
}

ku_status_t KuInstance::getDevice(ku_device_id_t deviceId, KuDevice **out) noexcept {
    return d_ptr->getDevice(deviceId, out);
}

ku_status_t KuInstance::getBuiltinInfo(ku_builtin_info_t *out,
                                       ku_size_t          capacity,
                                       ku_size_t         *outCount) noexcept {
    KU_ASSERT(outCount != nullptr, "getBuiltinInfo requires a non-null count output");
    KU_ASSERT(capacity == 0 || out != nullptr,
              "getBuiltinInfo requires storage when capacity is nonzero");

    *outCount = 0;
    try {
        const auto names = d_ptr->m_builtinRegistry.functionNames();
        *outCount = names.size();
        if (capacity == 0) {
            return KU_STATUS_SUCCESS;
        }
        if (capacity < names.size()) {
            return KU_STATUS_BUFFER_TOO_SMALL;
        }
        for (std::size_t i = 0; i < names.size(); ++i) {
            out[i].name.data = names[i].data();
            out[i].name.size = names[i].size();
        }
        return KU_STATUS_SUCCESS;
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
}

ku_status_t KuInstance::getKuProcAddress(ku_string_view_t name, ku_call_target_t *out) noexcept {
    KU_ASSERT(out != nullptr, "getKuProcAddress requires a non-null output slot");
    const auto target =
        d_ptr->m_builtinRegistry.getCallTarget(std::string_view(name.data, name.size));
    if (!target.has_value()) {
        return KU_STATUS_NOT_FOUND;
    }
    *out = *target;
    return KU_STATUS_SUCCESS;
}

ku_status_t KuInstance::getCapabilities(ku_instance_capabilities_t *out) noexcept {
    KU_ASSERT(out != nullptr, "getCapabilities requires a non-null output slot");
    *out = d_ptr->m_capabilityState->effective();
    return KU_STATUS_SUCCESS;
}

ku_status_t KuInstance::flush() noexcept {
    ku_status_t result = KU_STATUS_SUCCESS;
    for (const auto &[deviceId, device] : d_ptr->m_devices) {
        (void)deviceId;
        const auto status = device->flush();
        if (result == KU_STATUS_SUCCESS && status != KU_STATUS_SUCCESS) {
            result = status;
        }
    }
    return result;
}

} // namespace kuai
