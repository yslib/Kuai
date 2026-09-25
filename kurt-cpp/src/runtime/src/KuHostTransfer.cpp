#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuDevice.h>
#include <kuai/core/KuDeviceGuard.h>
#include <kuai/core/KuHostTransfer.h>
#include <kuai/runtime/KuCompletion.h>

#include "runtime/KuHostTransferFactory.h"

namespace kuai {

namespace {

class KuNativeHostTransfer final : public KuHostTransfer {
    class State;

public:
    static ku_status_t create(const ku_instance_capabilities_t &capabilities,
                              KuDevice                         &device,
                              std::unique_ptr<KuHostTransfer>  &out) noexcept {
        KU_ASSERT(capabilities.scheduler.submit != nullptr);
        KU_ASSERT(capabilities.max_worker_concurrency != 0);
        out.reset();
        try {
            const auto info = normalize(capabilities);
            auto       state = std::make_shared<State>(info, capabilities.scheduler, device);
            out.reset(new KuNativeHostTransfer(device, info, std::move(state)));
            return KU_STATUS_SUCCESS;
        } catch (const detail::KuStatusError &error) {
            return error.status();
        } catch (const std::bad_alloc &) {
            return KU_STATUS_OUT_OF_HOST_MEMORY;
        } catch (...) {
            return KU_STATUS_INTERNAL_ERROR;
        }
    }

    ~KuNativeHostTransfer() override {
        m_state->stopAndWait();
        m_state->releaseResources();
    }

private:
    struct Lane {
        Lane(KuDevice &device, std::size_t stagingBytes) : m_device(device) {
            KuDeviceGuard guard(m_device);
            if (guard.status() != KU_STATUS_SUCCESS) {
                throw detail::KuStatusError(guard.status());
            }
            const auto &api = m_device.getVendorApi();
            auto        status = api.malloc_host(api.ctx, &m_staging, stagingBytes);
            if (status != KU_STATUS_SUCCESS) {
                throw detail::KuStatusError(status);
            }
            status = api.stream_create(api.ctx, &m_stream);
            if (status != KU_STATUS_SUCCESS) {
                (void)api.free_host(api.ctx, m_staging);
                m_staging = nullptr;
                throw detail::KuStatusError(status);
            }
        }

        Lane(const Lane &) = delete;
        Lane &operator=(const Lane &) = delete;

        ~Lane() {
            KuDeviceGuard guard(m_device);
            KU_ASSERT(guard.status() == KU_STATUS_SUCCESS,
                      "failed to select a host-transfer device");
            const auto &api = m_device.getVendorApi();
            KU_VERIFY(api.stream_destroy(api.ctx, m_stream) == KU_STATUS_SUCCESS,
                      "failed to destroy a host-transfer stream");
            KU_VERIFY(api.free_host(api.ctx, m_staging) == KU_STATUS_SUCCESS,
                      "failed to release a host-transfer staging buffer");
        }

        void       *m_staging = nullptr;
        ku_stream_t m_stream = nullptr;

    private:
        KuDevice &m_device;
    };

    struct Task {
        Task(std::shared_ptr<State> owner,
             void                  *destination,
             const void            *source,
             ku_size_t              byteCount,
             ku_memcpy_kind_t       copyKind,
             ku_stream_t            dependency,
             KuCompletion          &completionObject)
            : m_state(std::move(owner)), m_completion(&completionObject), m_dst(destination),
              m_src(source), m_bytes(byteCount), m_kind(copyKind), m_dependencyStream(dependency) {
        }

        std::shared_ptr<State> m_state;
        ku_sp<KuCompletion>    m_completion;
        void                  *m_dst;
        const void            *m_src;
        ku_size_t              m_bytes;
        ku_memcpy_kind_t       m_kind;
        ku_stream_t            m_dependencyStream;
    };

    class State final : public std::enable_shared_from_this<State> {
    public:
        State(KuHostTransferInfo info, ku_scheduler_t scheduler, KuDevice &device)
            : m_info(info), m_scheduler(scheduler), m_device(device) {
            m_lanes.reserve(info.m_parallelism);
            m_availableLanes.reserve(info.m_parallelism);
            for (std::size_t index = 0; index < info.m_parallelism; ++index) {
                m_lanes.push_back(std::make_unique<Lane>(m_device, info.m_stagingBytesPerLane));
                m_availableLanes.push_back(info.m_parallelism - index - 1);
            }
        }

        ku_status_t submit(void            *dst,
                           const void      *src,
                           ku_size_t        bytes,
                           ku_memcpy_kind_t kind,
                           ku_stream_t      dependencyStream,
                           KuCompletion    &completion) noexcept {
            auto task = std::unique_ptr<Task>(new (std::nothrow) Task(
                shared_from_this(), dst, src, bytes, kind, dependencyStream, completion));
            if (!task) {
                return KU_STATUS_OUT_OF_HOST_MEMORY;
            }
            {
                std::lock_guard lock(m_mutex);
                if (!m_accepting) {
                    return KU_STATUS_INVALID_STATE;
                }
                ++m_activeTasks;
            }

            const ku_task_t schedulerTask{.run = &runTask, .ctx = task.get()};
            const auto      status = m_scheduler.submit(m_scheduler.ctx, schedulerTask);
            if (status != KU_STATUS_SUCCESS) {
                taskFinished();
                return status;
            }
            (void)task.release();
            return KU_STATUS_SUCCESS;
        }

        void stopAndWait() noexcept {
            std::unique_lock lock(m_mutex);
            m_accepting = false;
            m_idle.wait(lock, [this]() noexcept { return m_activeTasks == 0; });
        }

        void waitIdle() noexcept {
            std::unique_lock lock(m_mutex);
            m_idle.wait(lock, [this]() noexcept { return m_activeTasks == 0; });
        }

        void releaseResources() noexcept {
            {
                std::lock_guard lock(m_mutex);
                KU_ASSERT(!m_accepting);
                KU_ASSERT(m_activeTasks == 0);
                m_availableLanes.clear();
            }
            m_lanes.clear();
        }

    private:
        static void runTask(void *ctx) noexcept {
            std::unique_ptr<Task> task(static_cast<Task *>(ctx));
            const auto            status = task->m_state->copy(*task);
            task->m_completion->prepareToSignal();
            task->m_state->taskFinished();
            task->m_completion->signal(status);
            task->m_completion.reset();
            task->m_state.reset();
        }

        ku_status_t copy(const Task &task) noexcept {
            KuDeviceGuard guard(m_device);
            if (guard.status() != KU_STATUS_SUCCESS) {
                return guard.status();
            }

            const auto &api = m_device.getVendorApi();
            auto        status = api.stream_query(api.ctx, task.m_dependencyStream);
            if (status == KU_STATUS_NOT_READY) {
                status = api.stream_synchronize(api.ctx, task.m_dependencyStream);
            }
            if (status != KU_STATUS_SUCCESS) {
                return status;
            }

            const auto laneIndex = acquireLane();
            status = copyWithLane(*m_lanes[laneIndex], task);
            releaseLane(laneIndex);
            return status;
        }

        ku_status_t copyWithLane(Lane &lane, const Task &task) noexcept {
            const auto &api = m_device.getVendorApi();
            auto       *dst = static_cast<std::byte *>(task.m_dst);
            const auto *src = static_cast<const std::byte *>(task.m_src);
            for (ku_size_t offset = 0; offset < task.m_bytes;) {
                const auto bytes =
                    std::min<ku_size_t>(m_info.m_stagingBytesPerLane, task.m_bytes - offset);
                ku_status_t status;
                if (task.m_kind == KU_MEMCPY_HOST_TO_DEVICE) {
                    std::memcpy(lane.m_staging, src + offset, bytes);
                    status = api.memcpy_async(api.ctx, dst + offset, lane.m_staging, bytes,
                                              KU_MEMCPY_HOST_TO_DEVICE, lane.m_stream);
                    if (status == KU_STATUS_SUCCESS) {
                        status = api.stream_synchronize(api.ctx, lane.m_stream);
                    }
                } else {
                    status = api.memcpy_async(api.ctx, lane.m_staging, src + offset, bytes,
                                              KU_MEMCPY_DEVICE_TO_HOST, lane.m_stream);
                    if (status == KU_STATUS_SUCCESS) {
                        status = api.stream_synchronize(api.ctx, lane.m_stream);
                    }
                    if (status == KU_STATUS_SUCCESS) {
                        std::memcpy(dst + offset, lane.m_staging, bytes);
                    }
                }
                if (status != KU_STATUS_SUCCESS) {
                    return status;
                }
                offset += bytes;
            }
            return KU_STATUS_SUCCESS;
        }

        std::size_t acquireLane() noexcept {
            std::unique_lock lock(m_mutex);
            m_laneReady.wait(lock, [this]() noexcept { return !m_availableLanes.empty(); });
            const auto result = m_availableLanes.back();
            m_availableLanes.pop_back();
            return result;
        }

        void releaseLane(std::size_t laneIndex) noexcept {
            {
                std::lock_guard lock(m_mutex);
                m_availableLanes.push_back(laneIndex);
            }
            m_laneReady.notify_one();
        }

        void taskFinished() noexcept {
            {
                std::lock_guard lock(m_mutex);
                KU_ASSERT(m_activeTasks != 0);
                --m_activeTasks;
                if (m_activeTasks != 0) {
                    return;
                }
            }
            m_idle.notify_all();
        }

        KuHostTransferInfo                 m_info;
        ku_scheduler_t                     m_scheduler;
        KuDevice                          &m_device;
        std::vector<std::unique_ptr<Lane>> m_lanes;
        std::vector<std::size_t>           m_availableLanes;
        std::mutex                         m_mutex;
        std::condition_variable            m_laneReady;
        std::condition_variable            m_idle;
        std::size_t                        m_activeTasks = 0;
        bool                               m_accepting = true;
    };

    KuNativeHostTransfer(KuDevice              &device,
                         KuHostTransferInfo     info,
                         std::shared_ptr<State> state) noexcept
        : KuHostTransfer(device, info), m_state(std::move(state)) {
    }

    ku_status_t onCopyAsync(void            *dst,
                            const void      *src,
                            ku_size_t        bytes,
                            ku_memcpy_kind_t kind,
                            ku_stream_t      dependencyStream,
                            KuCompletion    &completion) noexcept override {
        return m_state->submit(dst, src, bytes, kind, dependencyStream, completion);
    }

    void onWaitIdle() noexcept override {
        m_state->waitIdle();
    }

    static KuHostTransferInfo normalize(const ku_instance_capabilities_t &capabilities) noexcept {
        const auto parallelism = std::max<std::size_t>(1, capabilities.max_worker_concurrency);
        return {
            .m_parallelism = parallelism,
            .m_stagingBytesPerLane = std::size_t{1024 * 1024},
            .m_copyStreamCount = parallelism,
        };
    }

    std::shared_ptr<State> m_state;
};

} // namespace

KuHostTransfer::KuHostTransfer(KuDevice &device, KuHostTransferInfo info) noexcept
    : m_device(device), m_info(info) {
}

KuHostTransfer::~KuHostTransfer() = default;

ku_status_t KuHostTransfer::copyAsync(void                *dst,
                                      const void          *src,
                                      ku_size_t            bytes,
                                      ku_memcpy_kind_t     kind,
                                      ku_stream_t          dependencyStream,
                                      ku_sp<KuCompletion> &outCompletion) noexcept {
    KU_ASSERT(dst != nullptr, "copyAsync requires a non-null destination");
    KU_ASSERT(src != nullptr, "copyAsync requires a non-null source");
    KU_ASSERT(bytes != 0, "copyAsync requires a non-zero size");
    outCompletion.reset();
    if (kind != KU_MEMCPY_HOST_TO_DEVICE && kind != KU_MEMCPY_DEVICE_TO_HOST) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    ku_sp<KuCompletion> completion(new (std::nothrow) KuCompletion);
    if (!completion) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    }
    const auto result = onCopyAsync(dst, src, bytes, kind, dependencyStream, *completion);
    if (result != KU_STATUS_SUCCESS) {
        return result;
    }
    outCompletion = std::move(completion);
    return KU_STATUS_SUCCESS;
}

namespace detail {

ku_status_t createDefaultHostTransfer(const ku_instance_capabilities_t &capabilities,
                                      KuDevice                         &device,
                                      std::unique_ptr<KuHostTransfer>  &out) noexcept {
    return KuNativeHostTransfer::create(capabilities, device, out);
}

} // namespace detail

} // namespace kuai
