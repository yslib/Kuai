#pragma once

#include <cstdint>
#include <cstdio>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <kuai/core/KuCore.h>
#include <kuai/profiler/KuTracy.h>
#include <kuai/vendor/KuVendorTraits.h>

#define KU_THROW_VENDOR_NOT_IMPELEMENT                                             \
    do {                                                                           \
        throw std::runtime_error(std::string(__FUNCTION__) + " not implement for " \
                                 + kuai::vendor::KuVendor::VendorName);            \
    } while (0);

namespace kuai::vendor::cuda::detail {

[[nodiscard]] constexpr ku_status_t toStatus(cudaError_t result) noexcept {
    switch (result) {
        case cudaSuccess:
            return KU_STATUS_SUCCESS;
        case cudaErrorNotReady:
            return KU_STATUS_NOT_READY;
        case cudaErrorNotSupported:
        case cudaErrorCompatNotSupportedOnDevice:
            return KU_STATUS_NOT_SUPPORTED;
        case cudaErrorMemoryAllocation:
            return KU_STATUS_OUT_OF_DEVICE_MEMORY;
        case cudaErrorInitializationError:
        case cudaErrorStubLibrary:
        case cudaErrorInsufficientDriver:
        case cudaErrorCallRequiresNewerDriver:
        case cudaErrorStartupFailure:
        case cudaErrorSystemNotReady:
        case cudaErrorSystemDriverMismatch:
            return KU_STATUS_BACKEND_UNAVAILABLE;
        case cudaErrorNoDevice:
        case cudaErrorInvalidDevice:
        case cudaErrorDevicesUnavailable:
        case cudaErrorDeviceNotLicensed:
            return KU_STATUS_DEVICE_UNAVAILABLE;
        case cudaErrorCudartUnloading:
        case cudaErrorDeviceUninitialized:
        case cudaErrorECCUncorrectable:
        case cudaErrorIllegalAddress:
        case cudaErrorLaunchTimeout:
        case cudaErrorContextIsDestroyed:
        case cudaErrorAssert:
        case cudaErrorHardwareStackError:
        case cudaErrorIllegalInstruction:
        case cudaErrorMisalignedAddress:
        case cudaErrorInvalidAddressSpace:
        case cudaErrorInvalidPc:
        case cudaErrorLaunchFailure:
            return KU_STATUS_DEVICE_LOST;
        default:
            return KU_STATUS_DEVICE_ERROR;
    }
}

inline void
check(cudaError_t error, const char *filename, const char *functionName, std::uint32_t lineNumber) {
    if (error == cudaSuccess) {
        return;
    }
    std::fprintf(stderr, "[kuai] CUDA error: %s, file: %s, func: %s, line: %u\n",
                 cudaGetErrorString(error), filename, functionName, lineNumber);
    std::fflush(stderr);
    throw ::kuai::detail::KuStatusError(toStatus(error));
}

} // namespace kuai::vendor::cuda::detail

#define KU_DEV_CALL_CHECK(EXPR)                                                    \
    do {                                                                           \
        ::kuai::vendor::cuda::detail::check(({ EXPR; }), __FILE__, __func__,       \
                                            static_cast<std::uint32_t>(__LINE__)); \
    } while (0)

#define KU_DEV_CALL_CHECK_WITH_LOG(EXPR)                                           \
    do {                                                                           \
        ::kuai::vendor::cuda::detail::check(({ EXPR; }), __FILE__, __func__,       \
                                            static_cast<std::uint32_t>(__LINE__)); \
    } while (0)

#define KU_DEV_CALL_CHECK_KERNEL_LAUNCH_CHECK()     KU_DEV_CALL_CHECK(cudaGetLastError())
#define KU_DEV_CALL_CHECK_KERNEL_LANUNCH_WITH_LOG() KU_DEV_CALL_CHECK_WITH_LOG(cudaGetLastError())
#define KU_DEV_CALL_CHECK_KERNEL_LANUNCH_SYNC_CHECK() \
    KU_DEV_CALL_CHECK(cudaDeviceSynchronize(); cudaGetLastError())

namespace kuai::vendor::cuda {

struct KuVendor : public KuVendorBase<KuVendor> {
    using NativeResult = cudaError_t;

    static constexpr const char      *VendorName = "cuda";
    static constexpr ku_device_type_t DeviceType = KU_DEVICE_CUDA;

public:
    static KU_ALWAYS_INLINE void check(NativeResult result) {
        KU_DEV_CALL_CHECK(result);
    }

    static constexpr ku_status_t toStatus(NativeResult result) noexcept {
        return detail::toStatus(result);
    }

    static NativeResult onKuGetDeviceCount(void *ctx, int *count) noexcept {
        (void)ctx;
        return cudaGetDeviceCount(count);
    }

    static NativeResult onKuGetDevice(void *ctx, ku_device_id_t *device) noexcept {
        (void)ctx;
        return cudaGetDevice(device);
    }

    static NativeResult onKuSetDevice(void *ctx, ku_device_id_t device) noexcept {
        (void)ctx;
        return cudaSetDevice(device);
    }

    static void
    onKuGetVendorProperties(void *ctx, KuVendorProperties &props, ku_device_id_t device) {
        (void)ctx;
        cudaDeviceProp deviceProp;
        KU_DEV_CALL_CHECK(cudaGetDeviceProperties(&deviceProp, device));
        props.m_name = deviceProp.name;
        props.m_major = deviceProp.major;
        props.m_minor = deviceProp.minor;
        props.m_maxThreadsPerBlock = deviceProp.maxThreadsPerBlock;
        props.m_warpSize = deviceProp.warpSize;
        props.m_totalGlobalMem = deviceProp.totalGlobalMem;
        props.m_sharedMemPerBlock = deviceProp.sharedMemPerBlock;
        props.m_maxRegistersPerBlock = deviceProp.regsPerBlock;
        int driverVersion = 0;
        int runtimeVersion = 0;
        KU_DEV_CALL_CHECK(cudaDriverGetVersion(&driverVersion));
        KU_DEV_CALL_CHECK(cudaRuntimeGetVersion(&runtimeVersion));
        props.m_driverVersion = std::to_string(driverVersion / 1000) + "."
                                + std::to_string((driverVersion % 1000) / 10);
        props.m_runtimeVersion = std::to_string(runtimeVersion / 1000) + "."
                                 + std::to_string((runtimeVersion % 1000) / 10);
    }

    static NativeResult onKuStreamQuery(void *ctx, ku_stream_t stream) noexcept {
        (void)ctx;
        return cudaStreamQuery(toCudaHandle<cudaStream_t>(stream));
    }
    static NativeResult onKuStreamSynchronize(void *ctx, ku_stream_t stream) noexcept {
        (void)ctx;
        return cudaStreamSynchronize(toCudaHandle<cudaStream_t>(stream));
    }

    static void onKuStreamAsync(void *ctx, ku_stream_t stream, CallbackProc callback, void *data) {
        (void)ctx;
        KU_DEV_CALL_CHECK(cudaLaunchHostFunc(toCudaHandle<cudaStream_t>(stream), callback, data));
    }

    static NativeResult onKuMemcpyAsync(void            *ctx,
                                        void            *dst,
                                        const void      *src,
                                        size_t           count,
                                        ku_memcpy_kind_t kind,
                                        ku_stream_t      stream) noexcept {
        (void)ctx;
        return cudaMemcpyAsync(dst, src, count, toCuda(kind), toCudaHandle<cudaStream_t>(stream));
    }

    static NativeResult onKuMemcpy(
        void *ctx, void *dst, const void *src, size_t count, ku_memcpy_kind_t kind) noexcept {
        (void)ctx;
        return cudaMemcpy(dst, src, count, toCuda(kind));
    }

    static ku_stream_t onKuStreamPerThread(void *ctx) noexcept {
        (void)ctx;
        return toCudaHandle<ku_stream_t>(cudaStreamPerThread);
    }

    static NativeResult onKuMalloc(void *ctx, void **ptr, size_t size) noexcept {
        (void)ctx;
        return cudaMalloc(ptr, size);
    }

    static NativeResult onKuFree(void *ctx, void *ptr) noexcept {
        (void)ctx;
        return cudaFree(ptr);
    }

    static NativeResult onKuMallocHost(void *ctx, void **ptr, size_t size) noexcept {
        (void)ctx;
        return cudaHostAlloc(ptr, size, cudaHostAllocDefault);
    }

    static NativeResult onKuFreeHost(void *ctx, void *ptr) noexcept {
        (void)ctx;
        return cudaFreeHost(ptr);
    }

    static NativeResult onKuStreamCreate(void *ctx, ku_stream_t *stream) noexcept {
        (void)ctx;
        return cudaStreamCreateWithFlags(toCudaHandle<cudaStream_t *>(stream),
                                         cudaStreamNonBlocking);
    }

    static NativeResult onKuStreamDestroy(void *ctx, ku_stream_t stream) noexcept {
        (void)ctx;
        return cudaStreamDestroy(toCudaHandle<cudaStream_t>(stream));
    }

    static void onKuStreamEventCreate(void *ctx, Event *event, KuEventCreateFlags flags) {
        (void)ctx;
        KU_DEV_CALL_CHECK(cudaEventCreateWithFlags(toCudaHandle<cudaEvent_t *>(event),
                                                   toCudaEventCreateFlags(flags)));
    }

    static void onKuStreamEventDestroy(void *ctx, Event event) {
        (void)ctx;
        KU_DEV_CALL_CHECK(cudaEventDestroy(toCudaHandle<cudaEvent_t>(event)));
    }

    static void onKuStreamEventRecord(void *ctx, Event event, ku_stream_t stream) {
        (void)ctx;
        KU_DEV_CALL_CHECK(
            cudaEventRecord(toCudaHandle<cudaEvent_t>(event), toCudaHandle<cudaStream_t>(stream)));
    }

    static void onKuEventElapsedTime(void *ctx, float *elapsed, Event start, Event stop) {
        (void)ctx;
        KU_DEV_CALL_CHECK(cudaEventElapsedTime(elapsed, toCudaHandle<cudaEvent_t>(start),
                                               toCudaHandle<cudaEvent_t>(stop)));
    }

    static void onKuStreamEventWait(void *ctx, Event event) {
        (void)ctx;
        KU_DEV_CALL_CHECK(cudaEventSynchronize(toCudaHandle<cudaEvent_t>(event)));
    }

    static NativeResult
    onKuCreateMemPool(void *ctx, ku_device_id_t device, MemPool *pool) noexcept {
        (void)ctx;
        cudaMemPoolProps properties{};
        properties.allocType = cudaMemAllocationTypePinned;
        properties.handleTypes = cudaMemHandleTypeNone;
        properties.location.type = cudaMemLocationTypeDevice;
        properties.location.id = device;

        auto result = cudaMemPoolCreate(toCudaHandle<cudaMemPool_t *>(pool), &properties);
        if (result != cudaSuccess) {
            return result;
        }

        auto releaseThreshold = std::numeric_limits<std::uint64_t>::max();
        result = cudaMemPoolSetAttribute(toCudaHandle<cudaMemPool_t>(*pool),
                                         cudaMemPoolAttrReleaseThreshold, &releaseThreshold);
        if (result != cudaSuccess) {
            (void)cudaMemPoolDestroy(toCudaHandle<cudaMemPool_t>(*pool));
            *pool = nullptr;
        }
        return result;
    }

    static NativeResult onKuDestroyMemPool(void *ctx, MemPool pool) noexcept {
        (void)ctx;
        return cudaMemPoolDestroy(toCudaHandle<cudaMemPool_t>(pool));
    }
    static NativeResult onKuMallocFromPoolAsync(
        void *ctx, void **ptr, size_t size, MemPool pool, ku_stream_t stream) noexcept {
        (void)ctx;
        return cudaMallocFromPoolAsync(ptr, size, toCudaHandle<cudaMemPool_t>(pool),
                                       toCudaHandle<cudaStream_t>(stream));
    }
    static NativeResult onKuFreeAsync(void *ctx, void *ptr, ku_stream_t stream) noexcept {
        (void)ctx;
        return cudaFreeAsync(ptr, toCudaHandle<cudaStream_t>(stream));
    }

    static cudaStream_t nativeStream(ku_stream_t stream) noexcept {
        return toCudaHandle<cudaStream_t>(stream);
    }

private:
    template <typename T, typename F>
    static KU_ALWAYS_INLINE T toCudaHandle(F handle) noexcept {
        return reinterpret_cast<T>(handle);
    }

    static KU_ALWAYS_INLINE cudaMemcpyKind toCuda(ku_memcpy_kind_t kind) noexcept {
        static_assert(static_cast<int>(KU_MEMCPY_HOST_TO_DEVICE)
                      == static_cast<int>(cudaMemcpyHostToDevice));
        static_assert(static_cast<int>(KU_MEMCPY_DEVICE_TO_HOST)
                      == static_cast<int>(cudaMemcpyDeviceToHost));
        static_assert(static_cast<int>(KU_MEMCPY_DEVICE_TO_DEVICE)
                      == static_cast<int>(cudaMemcpyDeviceToDevice));
        return static_cast<cudaMemcpyKind>(kind);
    }

    static unsigned int toCudaEventCreateFlags(KuEventCreateFlags flags) {
        unsigned int cudaFlags = 0;
        if (flags & KuEventDisableTiming) {
            cudaFlags |= cudaEventDisableTiming;
        }
        if (flags & KuEventInterprocess) {
            cudaFlags |= cudaEventInterprocess;
        }
        return cudaFlags;
    }

    static unsigned int toCudaStreamCreateFlags(KuStreamCreateFlags flags) {
        unsigned int cudaFlags = 0;
        if (flags & KuStreamNonBlocking) {
            cudaFlags |= cudaStreamNonBlocking;
        }
        return cudaFlags;
    }
};
} // namespace kuai::vendor::cuda

namespace kuai::vendor {
using cuda::KuVendor;
}
