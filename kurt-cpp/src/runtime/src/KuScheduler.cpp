#include "runtime/KuScheduler.h"

#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <new>
#include <queue>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <kuai/core/KuCore.h>

namespace kuai {
namespace {

constexpr std::uint32_t kDefaultWorkerConcurrency = 4;

ku_status_t validateCapabilities(const ku_instance_capabilities_t &capabilities) noexcept {
    if (capabilities.devices == nullptr || capabilities.device_count == 0) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    if (capabilities.scheduler.submit == nullptr && capabilities.scheduler.ctx != nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    try {
        std::unordered_set<ku_device_id_t> ids;
        ids.reserve(capabilities.device_count);
        for (std::size_t index = 0; index < capabilities.device_count; ++index) {
            const auto &device = capabilities.devices[index];
            if (device.device_id < 0) {
                return KU_STATUS_OUT_OF_RANGE;
            }
            if (!ids.insert(device.device_id).second) {
                return KU_STATUS_INVALID_ARGUMENT;
            }
        }
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }

    if (capabilities.max_host_memory_bytes != 0) {
        return KU_STATUS_NOT_SUPPORTED;
    }
    for (std::size_t index = 0; index < capabilities.device_count; ++index) {
        const auto &device = capabilities.devices[index];
        if (device.streams.max_compute_streams != 0 || device.streams.max_copy_streams != 0
            || device.max_memory_bytes != 0) {
            return KU_STATUS_NOT_SUPPORTED;
        }
    }
    return KU_STATUS_SUCCESS;
}

class KuDefaultScheduler {
public:
    explicit KuDefaultScheduler(std::uint32_t workerCount) {
        KU_ASSERT(workerCount != 0);
        try {
            m_workers.reserve(workerCount);
            for (std::uint32_t index = 0; index < workerCount; ++index) {
                m_workers.emplace_back([this]() noexcept { workerLoop(); });
            }
        } catch (...) {
            stopAndJoin();
            throw;
        }
    }

    KuDefaultScheduler(const KuDefaultScheduler &) = delete;
    KuDefaultScheduler &operator=(const KuDefaultScheduler &) = delete;

    ~KuDefaultScheduler() {
        stopAndJoin();
    }

    [[nodiscard]] ku_scheduler_t descriptor() noexcept {
        return ku_scheduler_t{.ctx = this, .submit = &submit};
    }

private:
    static ku_status_t submit(void *ctx, ku_task_t task) noexcept {
        KU_ASSERT(ctx != nullptr);
        KU_ASSERT(task.run != nullptr);
        return static_cast<KuDefaultScheduler *>(ctx)->submit(task);
    }

    ku_status_t submit(ku_task_t task) noexcept {
        try {
            {
                std::lock_guard lock(m_mutex);
                if (m_stopping) {
                    return KU_STATUS_INVALID_STATE;
                }
                m_tasks.push(task);
            }
            m_ready.notify_one();
            return KU_STATUS_SUCCESS;
        } catch (const std::bad_alloc &) {
            return KU_STATUS_OUT_OF_HOST_MEMORY;
        } catch (...) {
            return KU_STATUS_INTERNAL_ERROR;
        }
    }

    void workerLoop() noexcept {
        for (;;) {
            ku_task_t task{};
            {
                std::unique_lock lock(m_mutex);
                m_ready.wait(lock, [this]() noexcept { return m_stopping || !m_tasks.empty(); });
                if (m_stopping && m_tasks.empty()) {
                    return;
                }
                task = m_tasks.front();
                m_tasks.pop();
            }
            try {
                task.run(task.ctx);
            } catch (...) {
                std::terminate();
            }
        }
    }

    void stopAndJoin() noexcept {
        {
            std::lock_guard lock(m_mutex);
            m_stopping = true;
        }
        m_ready.notify_all();
        for (auto &worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        m_workers.clear();
    }

    std::mutex               m_mutex;
    std::condition_variable  m_ready;
    std::queue<ku_task_t>    m_tasks;
    std::vector<std::thread> m_workers;
    bool                     m_stopping = false;
};

} // namespace

class KuInstanceCapabilityState::Impl {
    KU_DECL_API(KuInstanceCapabilityState)

public:
    Impl(KuInstanceCapabilityState *api, const ku_instance_capabilities_t &requested)
        : q_ptr(api), m_devices(requested.devices, requested.devices + requested.device_count) {
        m_effective.max_worker_concurrency = requested.max_worker_concurrency == 0
                                                 ? kDefaultWorkerConcurrency
                                                 : requested.max_worker_concurrency;
        if (requested.scheduler.submit != nullptr) {
            m_effective.scheduler = requested.scheduler;
        } else {
            m_defaultScheduler =
                std::make_unique<KuDefaultScheduler>(m_effective.max_worker_concurrency);
            m_effective.scheduler = m_defaultScheduler->descriptor();
        }
        m_effective.max_host_memory_bytes = requested.max_host_memory_bytes;
        m_effective.devices = m_devices.data();
        m_effective.device_count = m_devices.size();
    }

    std::unique_ptr<KuDefaultScheduler>   m_defaultScheduler;
    std::vector<ku_device_capabilities_t> m_devices;
    ku_instance_capabilities_t            m_effective{};
};

KuInstanceCapabilityState::KuInstanceCapabilityState(const ku_instance_capabilities_t &requested)
    : d_ptr(std::make_unique<Impl>(this, requested)) {
}

KuInstanceCapabilityState::~KuInstanceCapabilityState() = default;

ku_status_t
KuInstanceCapabilityState::validate(const ku_instance_capabilities_t &requested) noexcept {
    return validateCapabilities(requested);
}

ku_status_t
KuInstanceCapabilityState::create(const ku_instance_capabilities_t           &requested,
                                  std::unique_ptr<KuInstanceCapabilityState> &out) noexcept {
    out.reset();
    const auto validation = validate(requested);
    if (validation != KU_STATUS_SUCCESS) {
        return validation;
    }

    try {
        out.reset(new KuInstanceCapabilityState(requested));
        return KU_STATUS_SUCCESS;
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
}

const ku_instance_capabilities_t &KuInstanceCapabilityState::effective() const noexcept {
    KU_ASSERT(d_ptr != nullptr);
    return d_ptr->m_effective;
}

} // namespace kuai
