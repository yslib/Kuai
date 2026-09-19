#pragma once

#include <cstddef>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/kuai_c/ku_runtime.h>
#include <kuai/profiler/KuTracy.h>

namespace kuai {

class KuDevice;
namespace detail {
class KuMemoryResourceAccess;
}

// Like std::pmr::memory_resource. Naming follows the standard library's
// lower-case convention for memory-resource facilities.
class KuMemoryResource {
public:
    inline static constexpr std::size_t DefaultAlignment = 256;

    using Stream = ku_stream_t;

    virtual ~KuMemoryResource() = default;
    KuMemoryResource(KuMemoryResource const &) = delete;
    KuMemoryResource(KuMemoryResource &&) noexcept = delete;
    KuMemoryResource &operator=(KuMemoryResource const &) = delete;
    KuMemoryResource &operator=(KuMemoryResource &&) noexcept = delete;

    [[nodiscard]] KuDevice &getDevice() const noexcept {
        return m_device;
    }

    ku_status_t allocate(std::size_t bytes, Stream stream, void **out) noexcept {
        return allocateImpl(bytes, DefaultAlignment, stream, out, "dev::stream");
    }

    void deallocate(void *ptr, std::size_t bytes, Stream stream) noexcept {
        deallocateImpl(ptr, bytes, DefaultAlignment, stream, "dev::stream");
    }

    ku_status_t
    allocate_async(std::size_t bytes, std::size_t alignment, Stream stream, void **out) noexcept {
        return allocateImpl(bytes, alignment, stream, out, "dev::async_align");
    }

    void
    deallocate_async(void *ptr, std::size_t bytes, std::size_t alignment, Stream stream) noexcept {
        deallocateImpl(ptr, bytes, alignment, stream, "dev::async_align");
    }

    ku_status_t allocate_async(std::size_t bytes, Stream stream, void **out) noexcept {
        return allocateImpl(bytes, DefaultAlignment, stream, out, "dev::async");
    }

    void deallocate_async(void *ptr, std::size_t bytes, Stream stream) noexcept {
        deallocateImpl(ptr, bytes, DefaultAlignment, stream, "dev::async");
    }

    [[nodiscard]] bool is_equal(KuMemoryResource const &other) const noexcept {
        return do_is_equal(other);
    }

    [[nodiscard]] bool operator==(KuMemoryResource const &other) const noexcept {
        return do_is_equal(other);
    }

    [[nodiscard]] bool operator!=(KuMemoryResource const &other) const noexcept {
        return !do_is_equal(other);
    }

    [[nodiscard]] virtual bool supports_get_mem_info() const noexcept {
        return false;
    }

    [[nodiscard]] std::pair<std::size_t, std::size_t> get_mem_info(Stream stream) const {
        KU_ASSERT(stream != nullptr, "device memory queries require a non-null stream");
        return do_get_mem_info(stream);
    }

protected:
    explicit KuMemoryResource(KuDevice &device) noexcept : m_device(device) {
    }

private:
    friend class detail::KuMemoryResourceAccess;

    // Releases resource-owned backend state. Instance teardown calls this
    // explicitly so cleanup failures can remain diagnostic rather than being
    // hidden in a destructor.
    ku_status_t shutdown() noexcept {
        return do_shutdown();
    }

    virtual ku_status_t do_allocate(std::size_t bytes, Stream stream, void **out) noexcept = 0;
    virtual void        do_deallocate(void *ptr, std::size_t bytes, Stream stream) noexcept = 0;
    virtual ku_status_t do_shutdown() noexcept {
        return KU_STATUS_SUCCESS;
    }

    [[nodiscard]] static constexpr std::size_t alignAllocationSize(std::size_t bytes,
                                                                   std::size_t alignment) noexcept {
        return (bytes + (alignment - 1)) & ~(alignment - 1);
    }

    ku_status_t allocateImpl(std::size_t bytes,
                             std::size_t alignment,
                             Stream      stream,
                             void      **out,
                             const char *traceName) noexcept {
        KU_ASSERT(stream != nullptr, "device allocation requires a non-null stream");
        KU_ASSERT(out != nullptr, "device allocation requires a non-null output");
        *out = nullptr;
        const auto status = do_allocate(alignAllocationSize(bytes, alignment), stream, out);
        if (status != KU_STATUS_SUCCESS) {
            *out = nullptr;
            return status;
        }
        if (bytes != 0 && *out == nullptr) {
            return KU_STATUS_INVALID_STATE;
        }
        if (*out != nullptr) {
            KU_TRACE_MEM_ALLOC_N(*out, bytes, traceName);
        }
        return KU_STATUS_SUCCESS;
    }

    void deallocateImpl(void       *ptr,
                        std::size_t bytes,
                        std::size_t alignment,
                        Stream      stream,
                        const char *traceName) noexcept {
        KU_ASSERT(stream != nullptr, "device deallocation requires a non-null stream");
        KU_TRACE_MEM_FREE_N(ptr, traceName);
        do_deallocate(ptr, alignAllocationSize(bytes, alignment), stream);
    }

    [[nodiscard]] virtual bool do_is_equal(KuMemoryResource const &other) const noexcept {
        return this == &other;
    }

    [[nodiscard]] virtual std::pair<std::size_t, std::size_t> do_get_mem_info(Stream stream) const {
        return {0, 0};
    }

    KuDevice &m_device;
};

} // namespace kuai
