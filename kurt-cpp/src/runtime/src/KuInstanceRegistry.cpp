#include "runtime/KuInstanceRegistry.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <kuai/kuai_c/ku_runtime.h>
#include <kuai/runtime/KuInstance.h>
#include <kuai/runtime/KuModuleManager.h>

#include "runtime/KuScheduler.h"

namespace kuai {
namespace {

struct KuHostState {};

using KuVendorModuleFn = const ku_vendor_module_t *(*)(ku_host_t);

constexpr std::string_view kVendorModuleEntry = "kuVendorModule";

ku_host_t hostHandle() noexcept {
    static KuHostState state;
    return reinterpret_cast<ku_host_t>(&state);
}

bool isValidVendorTag(std::string_view vendor) noexcept {
    if (vendor.empty()) {
        return false;
    }
    for (const unsigned char ch : vendor) {
        if ((ch < 'a' || ch > 'z') && (ch < '0' || ch > '9') && ch != '_') {
            return false;
        }
    }
    return true;
}

const ku_vendor_module_t *loadVendorModule(std::string_view vendor) {
    try {
#if defined(__APPLE__)
        const auto libraryName = "libkurt_" + std::string(vendor) + ".dylib";
#else
        const auto libraryName = "libkurt_" + std::string(vendor) + ".so";
#endif
        auto      &manager = KuModuleManager::getKuModuleManager();
        const auto library = manager.loadLibrary(libraryName);
        if (!library) {
            return nullptr;
        }
        const auto entry =
            reinterpret_cast<KuVendorModuleFn>(library->symbol(kVendorModuleEntry.data()));
        return entry == nullptr ? nullptr : entry(hostHandle());
    } catch (const std::runtime_error &) {
        return nullptr;
    }
}

} // namespace

struct KuInstanceRegistry::InstanceSlot {
    std::mutex                  m_mutex;
    std::unique_ptr<KuInstance> m_instance;
};

KuInstanceRegistry::~KuInstanceRegistry() = default;

KuInstanceRegistry &KuInstanceRegistry::getInstance() {
    // The registry deliberately has process lifetime. Instance cleanup is an
    // explicit ku_instance_destroy responsibility; static destruction must not
    // enter a backend runtime after its own process-global teardown has begun.
    static auto *const registry = new KuInstanceRegistry;
    return *registry;
}

ku_status_t KuInstanceRegistry::init(const ku_instance_init_info_t &info, KuInstance **out) {
    KU_ASSERT(out != nullptr);
    *out = nullptr;
    if (info.vendor.data == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    std::string vendor;
    try {
        vendor.assign(info.vendor.data, info.vendor.size);
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
    if (!isValidVendorTag(vendor)) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    auto status = KuInstanceCapabilityState::validate(info.capabilities);
    if (status != KU_STATUS_SUCCESS) {
        return status;
    }
    if (!std::ranges::contains(std::span(info.capabilities.devices, info.capabilities.device_count),
                               info.default_device_id, &ku_device_capabilities_t::device_id)) {
        return KU_STATUS_OUT_OF_RANGE;
    }

    auto           &slot = getOrCreateSlot(vendor);
    std::lock_guard slotLock(slot.m_mutex);
    if (slot.m_instance != nullptr) {
        return KU_STATUS_ALREADY_INITIALIZED;
    }

    const ku_vendor_module_t *module = nullptr;
    {
        std::lock_guard moduleLock(m_moduleMutex);
        module = loadVendorModule(vendor);
    }
    if (module == nullptr) {
        return KU_STATUS_BACKEND_UNAVAILABLE;
    }

    std::unique_ptr<KuInstanceCapabilityState> capabilityState;
    status = KuInstanceCapabilityState::create(info.capabilities, capabilityState);
    if (status != KU_STATUS_SUCCESS) {
        return status;
    }

    std::unique_ptr<KuInstance> instance;
    status =
        KuInstance::create(std::move(capabilityState), info.default_device_id, *module, instance);
    if (status != KU_STATUS_SUCCESS) {
        return status;
    }

    auto *const rawInstance = instance.get();
    {
        std::lock_guard registryLock(m_registryMutex);
        const auto [position, inserted] = m_handles.emplace(rawInstance, &slot);
        KU_ASSERT(inserted);
        (void)position;
        slot.m_instance = std::move(instance);
    }
    *out = rawInstance;
    return KU_STATUS_SUCCESS;
}

ku_status_t KuInstanceRegistry::flush(KuInstance *instance) {
    KU_ASSERT(instance != nullptr);
    auto *const slot = findSlot(instance);
    KU_ASSERT(slot != nullptr, "instance handle is not active");
    if (slot == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    std::lock_guard slotLock(slot->m_mutex);
    if (slot->m_instance.get() != instance) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    return slot->m_instance->flush();
}

ku_status_t KuInstanceRegistry::destroy(KuInstance *instance) {
    KU_ASSERT(instance != nullptr);
    auto *const slot = findSlot(instance);
    KU_ASSERT(slot != nullptr, "instance handle is not active");
    if (slot == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    std::lock_guard slotLock(slot->m_mutex);
    if (slot->m_instance.get() != instance) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    {
        std::lock_guard registryLock(m_registryMutex);
        m_handles.erase(instance);
    }
    auto       ownedInstance = std::move(slot->m_instance);
    const auto flushStatus = ownedInstance->flush();
    const auto shutdownStatus = ownedInstance->shutdown();
    ownedInstance.reset();
    return flushStatus != KU_STATUS_SUCCESS ? flushStatus : shutdownStatus;
}

KuInstanceRegistry::InstanceSlot &KuInstanceRegistry::getOrCreateSlot(const std::string &vendor) {
    std::lock_guard lock(m_registryMutex);
    if (const auto found = m_instances.find(vendor); found != m_instances.end()) {
        return *found->second;
    }
    auto slot = std::make_unique<InstanceSlot>();
    auto [position, inserted] = m_instances.emplace(vendor, std::move(slot));
    KU_ASSERT(inserted);
    return *position->second;
}

KuInstanceRegistry::InstanceSlot *KuInstanceRegistry::findSlot(KuInstance *instance) noexcept {
    std::lock_guard lock(m_registryMutex);
    const auto      found = m_handles.find(instance);
    return found != m_handles.end() ? found->second : nullptr;
}

} // namespace kuai
