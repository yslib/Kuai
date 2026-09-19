#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <kuai/kuai_c/ku_runtime.h>

namespace kuai {

class KuInstance;

class KuInstanceRegistry final {
public:
    static KuInstanceRegistry &getInstance();

    ku_status_t init(const ku_instance_init_info_t &info, KuInstance **out);
    ku_status_t flush(KuInstance *instance);
    ku_status_t destroy(KuInstance *instance);

    KuInstanceRegistry(const KuInstanceRegistry &) = delete;
    KuInstanceRegistry &operator=(const KuInstanceRegistry &) = delete;

private:
    struct InstanceSlot;

    using InstanceMap = std::unordered_map<std::string, std::unique_ptr<InstanceSlot>>;
    using HandleMap = std::unordered_map<KuInstance *, InstanceSlot *>;

    KuInstanceRegistry() = default;
    ~KuInstanceRegistry();

    InstanceSlot &getOrCreateSlot(const std::string &vendor);
    InstanceSlot *findSlot(KuInstance *instance) noexcept;

    std::mutex  m_registryMutex;
    std::mutex  m_moduleMutex;
    InstanceMap m_instances;
    HandleMap   m_handles;
};

} // namespace kuai
