#pragma once

#include <memory>

#include <kuai/core/KuCore.h>
#include <kuai/kuai_c/ku_runtime.h>

namespace kuai {

class KuInstanceCapabilityState {
    KU_DECL_IMPL()

public:
    KuInstanceCapabilityState(const KuInstanceCapabilityState &) = delete;
    KuInstanceCapabilityState &operator=(const KuInstanceCapabilityState &) = delete;

    ~KuInstanceCapabilityState();

    static ku_status_t validate(const ku_instance_capabilities_t &requested) noexcept;

    static ku_status_t create(const ku_instance_capabilities_t           &requested,
                              std::unique_ptr<KuInstanceCapabilityState> &out) noexcept;

    [[nodiscard]] const ku_instance_capabilities_t &effective() const noexcept;

private:
    explicit KuInstanceCapabilityState(const ku_instance_capabilities_t &requested);
};

} // namespace kuai
