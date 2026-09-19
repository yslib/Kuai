#pragma once
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/kuai_c/ku_runtime.h>
#include <kuai/profiler/KuTracy.h>

namespace kuai {

class KuMemoryResource;

namespace detail {

template <typename Vendor, auto Operation>
struct KuVendorApiThunk;

template <typename Vendor,
          typename Result,
          typename... Args,
          Result (*Operation)(void *, Args...) noexcept>
struct KuVendorApiThunk<Vendor, Operation> {
    static ku_status_t invoke(void *ctx, Args... args) noexcept {
        return Vendor::toStatus(Operation(ctx, std::forward<Args>(args)...));
    }
};

template <typename Vendor, auto Operation>
struct KuVendorApiHostAllocationThunk;

template <typename Vendor,
          typename Result,
          typename... Args,
          Result (*Operation)(void *, Args...) noexcept>
struct KuVendorApiHostAllocationThunk<Vendor, Operation> {
    static ku_status_t invoke(void *ctx, Args... args) noexcept {
        const auto status = Vendor::toStatus(Operation(ctx, std::forward<Args>(args)...));
        return status == KU_STATUS_OUT_OF_DEVICE_MEMORY ? KU_STATUS_OUT_OF_HOST_MEMORY : status;
    }
};

template <typename Vendor, auto Operation>
struct KuVendorApiValueThunk;

template <typename Vendor, typename Result, Result (*Operation)(void *) noexcept>
struct KuVendorApiValueThunk<Vendor, Operation> {
    static Result invoke(void *ctx) noexcept {
        return Operation(ctx);
    }
};

#define KU_VENDOR_API_BIND(field, operation) \
    .field = &KuVendorApiThunk<Vendor, &Vendor::operation>::invoke
#define KU_VENDOR_API_BIND_HOST_ALLOCATION(field, operation) \
    .field = &KuVendorApiHostAllocationThunk<Vendor, &Vendor::operation>::invoke
#define KU_VENDOR_API_BIND_VALUE(field, operation) \
    .field = &KuVendorApiValueThunk<Vendor, &Vendor::operation>::invoke

template <typename Vendor>
struct KuVendorApiBuilder {
    static ku_vendor_api_t make(void *ctx) noexcept {
        return {
            .ctx = ctx,
            KU_VENDOR_API_BIND(get_device_count, onKuGetDeviceCount),
            KU_VENDOR_API_BIND(get_device, onKuGetDevice),
            KU_VENDOR_API_BIND(set_device, onKuSetDevice),
            KU_VENDOR_API_BIND_VALUE(get_per_thread_stream, onKuStreamPerThread),
            KU_VENDOR_API_BIND(stream_create, onKuStreamCreate),
            KU_VENDOR_API_BIND(stream_destroy, onKuStreamDestroy),
            KU_VENDOR_API_BIND(stream_query, onKuStreamQuery),
            KU_VENDOR_API_BIND(stream_synchronize, onKuStreamSynchronize),
            KU_VENDOR_API_BIND(malloc_device, onKuMalloc),
            KU_VENDOR_API_BIND(free_device, onKuFree),
            KU_VENDOR_API_BIND(memory_pool_create, onKuCreateMemPool),
            KU_VENDOR_API_BIND(memory_pool_destroy, onKuDestroyMemPool),
            KU_VENDOR_API_BIND(malloc_from_pool_async, onKuMallocFromPoolAsync),
            KU_VENDOR_API_BIND(free_async, onKuFreeAsync),
            KU_VENDOR_API_BIND_HOST_ALLOCATION(malloc_host, onKuMallocHost),
            KU_VENDOR_API_BIND(free_host, onKuFreeHost),
            KU_VENDOR_API_BIND(memcpy, onKuMemcpy),
            KU_VENDOR_API_BIND(memcpy_async, onKuMemcpyAsync),
        };
    }
};

#undef KU_VENDOR_API_BIND
#undef KU_VENDOR_API_BIND_HOST_ALLOCATION
#undef KU_VENDOR_API_BIND_VALUE

} // namespace detail

// just common part of different vendors
struct KuVendorProperties {
    std::string m_name;
    int         m_major;              // major version of the device
    int         m_minor;              // minor version of the device
    int         m_maxThreadsPerBlock; // maximum number of threads per block
    int         m_warpSize;
    size_t      m_totalGlobalMem;       // total global memory available on the device
    size_t      m_sharedMemPerBlock;    // shared memory available per block
    size_t      m_maxSharedMemPerBlock; // maximum shared memory available per block
    size_t      m_maxRegistersPerBlock; // maximum registers available per block

    std::string m_runtimeVersion;
    std::string m_driverVersion;
};

using KuEventCreateFlags = unsigned int;

enum KuEventCreateFlagsBits {
    KuEventDefault = 0x00,       // default event creation
    KuEventBlockingSync = 0x01,  // blocking synchronization
    KuEventDisableTiming = 0x02, // disable timing for the event
    KuEventInterprocess = 0x04,  // interprocess event
};

using KuStreamCreateFlags = unsigned int;
enum KuStreamCreateFlagsBits {
    KuStreamDefault = 0x00,     // default stream creation
    KuStreamNonBlocking = 0x01, // non-blocking stream
};

template <typename Impl>
struct KuVendorBase {
    using Event = ku_event_t;
    using MemPool = ku_memory_pool_t;

    using CallbackProc = void (*)(void *data);

    static ku_status_t kuGetDeviceCount(void *ctx, int *count) noexcept {
        return Impl::toStatus(Impl::onKuGetDeviceCount(ctx, count));
    }

    static ku_status_t kuGetDevice(void *ctx, ku_device_id_t *device) noexcept {
        return Impl::toStatus(Impl::onKuGetDevice(ctx, device));
    }

    static ku_status_t kuSetDevice(void *ctx, ku_device_id_t device) noexcept {
        return Impl::toStatus(Impl::onKuSetDevice(ctx, device));
    }

    static void kuGetVendorProperties(void *ctx, KuVendorProperties &props, ku_device_id_t device) {
        KU_ZONE_SCOPED_N("vendor::getDeviceProperties");
        return Impl::onKuGetVendorProperties(ctx, props, device);
    }

    static bool kuStreamQuery(void *ctx, ku_stream_t stream) {
        KU_ZONE_SCOPED_N("vendor::queryStreamStatus");
        const auto result = Impl::onKuStreamQuery(ctx, stream);
        const auto status = Impl::toStatus(result);
        if (status == KU_STATUS_SUCCESS) {
            return true;
        }
        if (status == KU_STATUS_NOT_READY) {
            return false;
        }
        Impl::check(result);
        return false;
    }

    static KU_ALWAYS_INLINE void kuStreamSynchronize(void *ctx, ku_stream_t stream) {
        KU_ZONE_SCOPED_N("vendor::sync");
        Impl::check(Impl::onKuStreamSynchronize(ctx, stream));
    }

    static void kuStreamAsync(void *ctx, ku_stream_t stream, CallbackProc callback, void *data) {
        KU_ZONE_SCOPED_N("vendor::streamAsync");
        return Impl::onKuStreamAsync(ctx, stream, callback, data);
    }

    static KU_ALWAYS_INLINE void kuMemcpyAsync(void            *ctx,
                                               void            *dst,
                                               const void      *src,
                                               size_t           size,
                                               ku_memcpy_kind_t kind,
                                               ku_stream_t      stream) {
        if (kind == KU_MEMCPY_DEVICE_TO_HOST) {
            KU_ZONE_SCOPED_NC("vendor::kuMemcpyAsync", MEMCPY_D2H_COLOR);
        } else if (kind == KU_MEMCPY_HOST_TO_DEVICE) {
            KU_ZONE_SCOPED_NC("vendor::kuMemcpyAsync", MEMCPY_H2D_COLOR);
        } else if (kind == KU_MEMCPY_DEVICE_TO_DEVICE) {
            KU_ZONE_SCOPED_NC("vendor::kuMemcpyAsync", MEMCPY_H2D_COLOR);
        } else {
            KU_UNREACHABLE();
        }
        Impl::check(Impl::onKuMemcpyAsync(ctx, dst, src, size, kind, stream));
    }

    static KU_ALWAYS_INLINE void
    kuMemcpy(void *ctx, void *dst, const void *src, size_t size, ku_memcpy_kind_t kind) {
        if (kind == KU_MEMCPY_DEVICE_TO_HOST) {
            KU_ZONE_SCOPED_NC("vendor::kuMemcpy", MEMCPY_D2H_COLOR);
        } else if (kind == KU_MEMCPY_HOST_TO_DEVICE) {
            KU_ZONE_SCOPED_NC("vendor::kuMemcpy", MEMCPY_H2D_COLOR);
        } else if (kind == KU_MEMCPY_DEVICE_TO_DEVICE) {
            KU_ZONE_SCOPED_NC("vendor::kuMemcpy", MEMCPY_H2D_COLOR);
        } else {
            KU_UNREACHABLE();
        }
        Impl::check(Impl::onKuMemcpy(ctx, dst, src, size, kind));
    }

    static ku_vendor_api_t makeApi(void *ctx) noexcept {
        return detail::KuVendorApiBuilder<Impl>::make(ctx);
    }

    static ku_stream_t kuStreamPerThread(void *ctx) noexcept {
        KU_ZONE_SCOPED_N("vendor::kuStreamPerThread");
        return Impl::onKuStreamPerThread(ctx);
    }

    static void kuMallocHost(void *ctx, void **ptr, size_t size) {
        KU_ZONE_SCOPED_N("vendor::kuMallocHost");
        Impl::check(Impl::onKuMallocHost(ctx, ptr, size));
    }

    static void kuFreeHost(void *ctx, void *ptr) {
        KU_ZONE_SCOPED_N("vendor::kuFreeHost");
        Impl::check(Impl::onKuFreeHost(ctx, ptr));
    }

    static void kuMalloc(void *ctx, void **ptr, size_t size) {
        KU_ZONE_SCOPED_N("vendor::kuMalloc");
        Impl::check(Impl::onKuMalloc(ctx, ptr, size));
    }

    static void kuFree(void *ctx, void *ptr) {
        KU_ZONE_SCOPED_N("vendor::kuFree");
        Impl::check(Impl::onKuFree(ctx, ptr));
    }

    // pool memory resource
    static ku_status_t kuCreateMemPool(void *ctx, ku_device_id_t device, MemPool *pool) noexcept {
        KU_ZONE_SCOPED_N("vendor::kuCreateMemPool");
        return Impl::toStatus(Impl::onKuCreateMemPool(ctx, device, pool));
    }

    static ku_status_t kuDestroyMemPool(void *ctx, MemPool pool) noexcept {
        KU_ZONE_SCOPED_N("vendor::kuDestroyMemPool");
        return Impl::toStatus(Impl::onKuDestroyMemPool(ctx, pool));
    }
    static ku_status_t kuMallocFromPoolAsync(
        void *ctx, void **ptr, size_t size, MemPool pool, ku_stream_t stream) noexcept {
        KU_ZONE_SCOPED_N("vendor::kuMallocFromPoolAsync");
        return Impl::toStatus(Impl::onKuMallocFromPoolAsync(ctx, ptr, size, pool, stream));
    }
    static ku_status_t kuFreeAsync(void *ctx, void *ptr, ku_stream_t stream) noexcept {
        KU_ZONE_SCOPED_N("vendor::kuFreeAsync");
        return Impl::toStatus(Impl::onKuFreeAsync(ctx, ptr, stream));
    }

    static void kuStreamCreate(void *ctx, ku_stream_t *stream) {
        KU_ZONE_SCOPED_N("vendor::kuStreamCreate");
        Impl::check(Impl::onKuStreamCreate(ctx, stream));
    }

    static void kuStreamDestroy(void *ctx, ku_stream_t stream) {
        KU_ZONE_SCOPED_N("vendor::kuStreamDestroy");
        Impl::check(Impl::onKuStreamDestroy(ctx, stream));
    }

    static void kuStreamEventCreate(void *ctx, Event *event, KuEventCreateFlags flags) {
        KU_ZONE_SCOPED_N("vendor::kuStreamEventCreate");
        return Impl::onKuStreamEventCreate(ctx, event, flags);
    }

    static void kuEventElapsedTime(void *ctx, float *elapsed, Event start, Event stop) {
        KU_ZONE_SCOPED_N("vendor::kuEventElapsedTime");
        return Impl::onKuEventElapsedTime(ctx, elapsed, start, stop);
    }

    static void kuStreamEventDestroy(void *ctx, Event event) {
        KU_ZONE_SCOPED_N("vendor::kuStreamEventDestroy");
        return Impl::onKuStreamEventDestroy(ctx, event);
    }

    static void kuStreamEventRecord(void *ctx, Event event, ku_stream_t stream) {
        KU_ZONE_SCOPED_N("vendor::kuStreamEventRecord");
        return Impl::onKuStreamEventRecord(ctx, event, stream);
    }

    static void kuStreamEventWait(void *ctx, Event event) {
        KU_ZONE_SCOPED_N("vendor::kuStreamEventWait");
        return Impl::onKuStreamEventWait(ctx, event);
    }
};

template <typename Vendor>
struct ku_vendor_traits_base {
    using vendor_t = Vendor;

    static constexpr ku_device_type_t device_type_v = vendor_t::DeviceType;
    static constexpr ku_device_id_t   default_device_id_v = 0;
};

// Backend specializations may add optional typed capabilities such as
// blas_handle(stream) and solver_handle(stream).
template <typename Vendor>
struct ku_vendor_traits : ku_vendor_traits_base<Vendor> {};

} // namespace kuai
