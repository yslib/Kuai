#pragma once

#include <memory>
#include <new>

#include <kuai/core/KuDevice.h>
#include <kuai/core/KuMemoryResource.h>

namespace kuai {

// Stream-ordered host adapter over one non-null backend-native pool.
class KuNativeMemoryResource final : public KuMemoryResource {
public:
    static ku_status_t create(KuDevice &device, std::unique_ptr<KuMemoryResource> &out) noexcept {
        const auto &api = device.getVendorApi();
        KU_ASSERT(api.memory_pool_create != nullptr);
        KU_ASSERT(api.memory_pool_destroy != nullptr);
        KU_ASSERT(api.malloc_from_pool_async != nullptr);
        KU_ASSERT(api.free_async != nullptr);
        out.reset();

        ku_memory_pool_t pool = nullptr;
        const auto       status =
            api.memory_pool_create(api.ctx, device.getDeviceInfo().device_id, &pool);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        KU_ASSERT(pool != nullptr, "successful pool creation must publish a pool handle");

        try {
            out.reset(new KuNativeMemoryResource(device, pool));
            return KU_STATUS_SUCCESS;
        } catch (const std::bad_alloc &) {
            (void)api.memory_pool_destroy(api.ctx, pool);
            return KU_STATUS_OUT_OF_HOST_MEMORY;
        } catch (...) {
            (void)api.memory_pool_destroy(api.ctx, pool);
            return KU_STATUS_INTERNAL_ERROR;
        }
    }

    ~KuNativeMemoryResource() override {
        (void)do_shutdown();
    }

private:
    KuNativeMemoryResource(KuDevice &device, ku_memory_pool_t pool) noexcept
        : KuMemoryResource(device), m_pool(pool) {
        KU_ASSERT(m_pool != nullptr);
    }

    ku_status_t do_allocate(std::size_t bytes, Stream stream, void **out) noexcept override {
        KU_ASSERT(m_pool != nullptr, "cannot allocate from a shut down native memory resource");
        const auto &api = getDevice().getVendorApi();
        *out = nullptr;
        return api.malloc_from_pool_async(api.ctx, out, bytes, m_pool, stream);
    }

    void do_deallocate(void *ptr, std::size_t bytes, Stream stream) noexcept override {
        (void)bytes;
        KU_ASSERT(m_pool != nullptr,
                  "cannot deallocate through a shut down native memory resource");
        const auto &api = getDevice().getVendorApi();
        KU_VERIFY(api.free_async(api.ctx, ptr, stream) == KU_STATUS_SUCCESS,
                  "failed to release pooled device memory");
    }

    ku_status_t do_shutdown() noexcept override {
        if (m_pool == nullptr) {
            return KU_STATUS_SUCCESS;
        }
        const auto pool = m_pool;
        m_pool = nullptr;
        const auto &api = getDevice().getVendorApi();
        return api.memory_pool_destroy(api.ctx, pool);
    }

    ku_memory_pool_t m_pool;
};

} // namespace kuai
