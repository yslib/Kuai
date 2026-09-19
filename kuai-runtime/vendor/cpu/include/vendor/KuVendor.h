#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

#include <kuai/profiler/KuTracy.h>
#include <kuai/vendor/KuVendorTraits.h>

#include "kuai/core/KuCore.h"

#define KU_THROW_VENDOR_NOT_IMPELEMENT                                             \
    do {                                                                           \
        throw std::runtime_error(std::string(__FUNCTION__) + " not implement for " \
                                 + kuai::vendor::KuVendor::VendorName);            \
    } while (0);

#define KU_DEV_CALL_CHECK(EXPR)
#define KU_DEV_CALL_CHECK_WITH_LOG(EXPR)
#define KU_DEV_CALL_CHECK_KERNEL_LAUNCH_CHECK()     KU_DEV_CALL_CHECK(cudaGetLastError())
#define KU_DEV_CALL_CHECK_KERNEL_LANUNCH_WITH_LOG() KU_DEV_CALL_CHECK_WITH_LOG(cudaGetLastError())
#define KU_DEV_CALL_CHECK_KERNEL_LANUNCH_SYNC_CHECK() \
    KU_DEV_CALL_CHECK(cudaDeviceSynchronize(); cudaGetLastError())

namespace kuai::vendor::cpu {

struct KuVendor : public KuVendorBase<KuVendor> {
    using NativeResult = ku_status_t;

    static constexpr const char      *VendorName = "cpu";
    static constexpr ku_device_type_t DeviceType = KU_DEVICE_CPU;

public:
    static KU_ALWAYS_INLINE void check(NativeResult result) {
        if (result != KU_STATUS_SUCCESS) {
            throw ::kuai::detail::KuStatusError(result);
        }
    }

    static KU_ALWAYS_INLINE ku_status_t toStatus(NativeResult result) noexcept {
        return result;
    }

    static NativeResult onKuGetDeviceCount(void *ctx, int *count) noexcept {
        (void)ctx;
        *count = 1;
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuGetDevice(void *ctx, ku_device_id_t *device) noexcept {
        (void)ctx;
        *device = 0;
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuSetDevice(void *ctx, ku_device_id_t device) noexcept {
        (void)ctx;
        return device == 0 ? KU_STATUS_SUCCESS : KU_STATUS_OUT_OF_RANGE;
    }

    static void
    onKuGetVendorProperties(void *ctx, KuVendorProperties &props, ku_device_id_t device) {
        (void)ctx;
        (void)device;
        (void)props; // No-op in CPU, as there are no devices
    }
    static NativeResult onKuStreamQuery(void *ctx, ku_stream_t stream) noexcept {
        // No-op in CPU, as there are no streams
        (void)ctx;
        (void)stream;
        return KU_STATUS_SUCCESS;
    }
    static NativeResult onKuStreamSynchronize(void *ctx, ku_stream_t stream) noexcept {
        // No-op in CPU, as there are no streams
        (void)ctx;
        (void)stream;
        return KU_STATUS_SUCCESS;
    }

    static void onKuStreamAsync(void *ctx, ku_stream_t stream, CallbackProc callback, void *data) {
        // No-op in CPU, as there are no streams
        (void)ctx;
        (void)stream;
        (void)callback;
        (void)data;
    }

    static NativeResult onKuMemcpyAsync(void            *ctx,
                                        void            *dst,
                                        const void      *src,
                                        size_t           count,
                                        ku_memcpy_kind_t kind,
                                        ku_stream_t      stream) noexcept {
        (void)ctx;
        (void)kind;
        (void)stream;
        std::memcpy(dst, src, count);
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuMemcpy(
        void *ctx, void *dst, const void *src, size_t count, ku_memcpy_kind_t kind) noexcept {
        (void)ctx;
        (void)kind;
        std::memcpy(dst, src, count);
        return KU_STATUS_SUCCESS;
    }

    static ku_stream_t onKuStreamPerThread(void *ctx) noexcept {
        (void)ctx;
        static std::byte defaultStreamToken{};
        return reinterpret_cast<ku_stream_t>(&defaultStreamToken);
    }

    static NativeResult onKuMalloc(void *ctx, void **ptr, size_t size) noexcept {
        (void)ctx;
        *ptr = std::malloc(size);
        return *ptr != nullptr || size == 0 ? KU_STATUS_SUCCESS : KU_STATUS_OUT_OF_DEVICE_MEMORY;
    }

    static NativeResult onKuFree(void *ctx, void *ptr) noexcept {
        (void)ctx;
        std::free(ptr);
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuMallocHost(void *ctx, void **ptr, size_t size) noexcept {
        (void)ctx;
        *ptr = std::malloc(size);
        return *ptr != nullptr || size == 0 ? KU_STATUS_SUCCESS : KU_STATUS_OUT_OF_HOST_MEMORY;
    }

    static NativeResult onKuFreeHost(void *ctx, void *ptr) noexcept {
        (void)ctx;
        std::free(ptr);
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuStreamCreate(void *ctx, ku_stream_t *stream) noexcept {
        (void)ctx;
        *stream = nullptr; // No stream in CPU
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuStreamDestroy(void *ctx, ku_stream_t stream) noexcept {
        // No stream to destroy in CPU
        (void)ctx;
        (void)stream;
        return KU_STATUS_SUCCESS;
    }

    static void onKuStreamEventCreate(void *ctx, Event *event, KuEventCreateFlags flags) {
        (void)ctx;
        (void)flags;
        *event = nullptr; // No event in CPU
    }

    static void onKuStreamEventDestroy(void *ctx, Event event) {
        // No event to destroy in CPU
        (void)ctx;
        (void)event;
    }

    static void onKuStreamEventRecord(void *ctx, Event event, ku_stream_t stream) {
        (void)ctx;
        (void)event;
        (void)stream; // No stream in CPU
        // No event to record in CPU
    }

    static void onKuEventElapsedTime(void *ctx, float *elapsed, Event start, Event stop) {
        (void)ctx;
        (void)start;     // No start event in CPU
        (void)stop;      // No stop event in CPU
        *elapsed = 0.0f; // No elapsed time in CPU
        // No event to elapse in CPU
    }

    static void onKuStreamEventWait(void *ctx, Event event) {
        (void)ctx;
        (void)event; // No event to wait in CPU
        // No event to wait in CPU
    }

    static NativeResult
    onKuCreateMemPool(void *ctx, ku_device_id_t device, MemPool *pool) noexcept {
        (void)ctx;
        (void)device;
        static std::byte poolToken{};
        *pool = reinterpret_cast<MemPool>(&poolToken);
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuDestroyMemPool(void *ctx, MemPool pool) noexcept {
        (void)ctx;
        (void)pool;
        return KU_STATUS_SUCCESS;
    }
    static NativeResult onKuMallocFromPoolAsync(
        void *ctx, void **ptr, size_t size, MemPool pool, ku_stream_t stream) noexcept {
        (void)pool;
        (void)stream;
        return onKuMalloc(ctx, ptr, size);
    }
    static NativeResult onKuFreeAsync(void *ctx, void *ptr, ku_stream_t stream) noexcept {
        (void)stream;
        return onKuFree(ctx, ptr);
    }

    static ku_stream_t nativeStream(ku_stream_t stream) noexcept {
        return stream;
    }

private:
};
} // namespace kuai::vendor::cpu

namespace kuai::vendor {
using cpu::KuVendor;
}
