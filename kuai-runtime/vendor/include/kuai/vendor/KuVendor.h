#pragma once
#include <cstddef>

#include <kuai/core/KuCore.h>
#include <kuai/vendor/KuVendorTraits.h>

#include <vendor/KuVendor.h>
#include <vendor/KuVendorTraits.h>

namespace kuai {

namespace vendor::dummy {

struct KuVendor : public KuVendorBase<KuVendor> {
    using NativeResult = ku_status_t;

    static constexpr const char      *VendorName = "dummy";
    static constexpr ku_device_type_t DeviceType = KU_DEVICE_EXT;

public:
    static KU_ALWAYS_INLINE void check(NativeResult result) {
        KU_ASSERT(result == KU_STATUS_SUCCESS);
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
        (void)props;
        (void)device;
    }

    static NativeResult onKuStreamQuery(void *ctx, ku_stream_t stream) noexcept {
        (void)ctx;
        (void)stream;
        return KU_STATUS_NOT_READY;
    }
    static NativeResult onKuStreamSynchronize(void *ctx, ku_stream_t stream) noexcept {
        (void)ctx;
        (void)stream;
        return KU_STATUS_SUCCESS;
    }

    static void onKuStreamAsync(void *ctx, ku_stream_t stream, CallbackProc callback, void *data) {
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
        (void)dst;
        (void)src;
        (void)count;
        (void)kind;
        (void)stream;
        return KU_STATUS_SUCCESS;
    }

    static NativeResult onKuMemcpy(
        void *ctx, void *dst, const void *src, size_t count, ku_memcpy_kind_t kind) noexcept {
        (void)ctx;
        (void)dst;
        (void)src;
        (void)count;
        (void)kind;
        return KU_STATUS_SUCCESS;
    }

    static ku_stream_t onKuStreamPerThread(void *ctx) noexcept {
        (void)ctx;
        static std::byte defaultStreamToken{};
        return reinterpret_cast<ku_stream_t>(&defaultStreamToken);
    }

    static NativeResult onKuMalloc(void *ctx, void **ptr, size_t size) noexcept {
        (void)ctx;
        (void)ptr;
        (void)size;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static NativeResult onKuFree(void *ctx, void *ptr) noexcept {
        (void)ctx;
        (void)ptr;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static NativeResult onKuMallocHost(void *ctx, void **ptr, size_t size) noexcept {
        (void)ctx;
        (void)ptr;
        (void)size;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static NativeResult onKuFreeHost(void *ctx, void *ptr) noexcept {
        (void)ctx;
        (void)ptr;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static NativeResult onKuStreamCreate(void *ctx, ku_stream_t *stream) noexcept {
        (void)ctx;
        (void)stream;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static NativeResult onKuStreamDestroy(void *ctx, ku_stream_t stream) noexcept {
        (void)ctx;
        (void)stream;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static void onKuStreamEventCreate(void *ctx, Event *event, KuEventCreateFlags flags) {
        (void)ctx;
        (void)event;
        (void)flags;
    }

    static void onKuStreamEventDestroy(void *ctx, Event event) {
        (void)ctx;
        (void)event;
    }

    static void onKuStreamEventRecord(void *ctx, Event event, ku_stream_t stream) {
        (void)ctx;
        (void)event;
        (void)stream;
    }

    static void onKuEventElapsedTime(void *ctx, float *elapsed, Event start, Event stop) {
        (void)ctx;
        (void)elapsed;
        (void)start;
        (void)stop;
    }

    static void onKuStreamEventWait(void *ctx, Event event) {
        (void)ctx;
        (void)event;
    }

    static NativeResult
    onKuCreateMemPool(void *ctx, ku_device_id_t device, MemPool *pool) noexcept {
        (void)ctx;
        (void)device;
        *pool = nullptr;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static NativeResult onKuDestroyMemPool(void *ctx, MemPool pool) noexcept {
        (void)ctx;
        (void)pool;
        return KU_STATUS_NOT_SUPPORTED;
    }
    static NativeResult onKuMallocFromPoolAsync(
        void *ctx, void **ptr, size_t size, MemPool pool, ku_stream_t stream) noexcept {
        (void)ctx;
        (void)size;
        (void)pool;
        (void)stream;
        *ptr = nullptr;
        return KU_STATUS_NOT_SUPPORTED;
    }
    static NativeResult onKuFreeAsync(void *ctx, void *ptr, ku_stream_t stream) noexcept {
        (void)ctx;
        (void)ptr;
        (void)stream;
        return KU_STATUS_NOT_SUPPORTED;
    }

    static ku_stream_t nativeStream(ku_stream_t stream) noexcept {
        return stream;
    }
};
} // namespace vendor::dummy
} // namespace kuai
