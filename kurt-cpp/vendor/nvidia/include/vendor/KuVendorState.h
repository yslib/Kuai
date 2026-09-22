#pragma once

#include <cstdint>
#include <cublas_v2.h>
#include <curand.h>
#include <cusolverDn.h>

#include <kuai/vendor/KuHandlePool.h>

#include <vendor/KuVendor.h>
#include <vendor/KuVendorStatus.h>

namespace kuai::vendor::cuda {

class KuVendorState {
    using BlasCreateFn = ku_status_t (*)(ku_device_id_t, ku_stream_t, cublasHandle_t *) noexcept;
    using BlasDestroyFn = void (*)(ku_device_id_t, cublasHandle_t) noexcept;
    using BlasPool = KuHandlePool<cublasHandle_t, BlasCreateFn, BlasDestroyFn>;

    using SolverCreateFn = ku_status_t (*)(ku_device_id_t,
                                           ku_stream_t,
                                           cusolverDnHandle_t *) noexcept;
    using SolverDestroyFn = void (*)(ku_device_id_t, cusolverDnHandle_t) noexcept;
    using SolverPool = KuHandlePool<cusolverDnHandle_t, SolverCreateFn, SolverDestroyFn>;

    struct RandomCreate {
        ku_status_t operator()(ku_device_id_t     device,
                               ku_stream_t        stream,
                               curandGenerator_t *generator) noexcept {
            return KuVendorState::createRandomGenerator(device, stream, generator,
                                                        nextRandomSeed());
        }

    private:
        [[nodiscard]] std::uint64_t nextRandomSeed() noexcept {
            auto value = m_sequence++ + std::uint64_t{0x243f6a8885a308d3};
            value = (value ^ (value >> 30U)) * std::uint64_t{0xbf58476d1ce4e5b9};
            value = (value ^ (value >> 27U)) * std::uint64_t{0x94d049bb133111eb};
            return value ^ (value >> 31U);
        }

        std::uint64_t m_sequence = 0;
    };

    using RandomDestroyFn = void (*)(ku_device_id_t, curandGenerator_t) noexcept;
    using RandomPool = KuHandlePool<curandGenerator_t, RandomCreate, RandomDestroyFn>;

public:
    KuVendorState()
        : m_blasPool(&createBlasHandle, &destroyBlasHandle),
          m_solverPool(&createSolverHandle, &destroySolverHandle),
          m_randomPool(RandomCreate{}, &destroyRandomGenerator) {
    }

    KuVendorState(const KuVendorState &) = delete;
    KuVendorState &operator=(const KuVendorState &) = delete;
    KuVendorState(KuVendorState &&) = delete;
    KuVendorState &operator=(KuVendorState &&) = delete;

    [[nodiscard]] auto blasHandle(ku_device_id_t device, ku_stream_t stream) {
        return m_blasPool.acquire(device, stream);
    }

    [[nodiscard]] auto solverHandle(ku_device_id_t device, ku_stream_t stream) {
        return m_solverPool.acquire(device, stream);
    }

    [[nodiscard]] auto randomGenerator(ku_device_id_t device, ku_stream_t stream) {
        return m_randomPool.acquire(device, stream);
    }

private:
    static ku_status_t selectDevice(ku_device_id_t device) noexcept {
        return detail::toStatus(cudaSetDevice(device));
    }

    static ku_status_t
    createBlasHandle(ku_device_id_t device, ku_stream_t stream, cublasHandle_t *handle) noexcept {
        auto status = selectDevice(device);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        auto nativeStatus = cublasCreate(handle);
        if (nativeStatus != CUBLAS_STATUS_SUCCESS) {
            return detail::toStatus(nativeStatus);
        }
        nativeStatus = cublasSetStream(*handle, KuVendor::nativeStream(stream));
        if (nativeStatus == CUBLAS_STATUS_SUCCESS) {
            return KU_STATUS_SUCCESS;
        }
        (void)cublasDestroy(*handle);
        *handle = nullptr;
        return detail::toStatus(nativeStatus);
    }

    static void destroyBlasHandle(ku_device_id_t device, cublasHandle_t handle) noexcept {
        (void)cudaSetDevice(device);
        (void)cublasDestroy(handle);
    }

    static ku_status_t createSolverHandle(ku_device_id_t      device,
                                          ku_stream_t         stream,
                                          cusolverDnHandle_t *handle) noexcept {
        auto status = selectDevice(device);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        auto nativeStatus = cusolverDnCreate(handle);
        if (nativeStatus != CUSOLVER_STATUS_SUCCESS) {
            return detail::toStatus(nativeStatus);
        }
        nativeStatus = cusolverDnSetStream(*handle, KuVendor::nativeStream(stream));
        if (nativeStatus == CUSOLVER_STATUS_SUCCESS) {
            return KU_STATUS_SUCCESS;
        }
        (void)cusolverDnDestroy(*handle);
        *handle = nullptr;
        return detail::toStatus(nativeStatus);
    }

    static void destroySolverHandle(ku_device_id_t device, cusolverDnHandle_t handle) noexcept {
        (void)cudaSetDevice(device);
        (void)cusolverDnDestroy(handle);
    }

    static ku_status_t createRandomGenerator(ku_device_id_t     device,
                                             ku_stream_t        stream,
                                             curandGenerator_t *generator,
                                             std::uint64_t      seed) noexcept {
        auto status = selectDevice(device);
        if (status != KU_STATUS_SUCCESS) {
            return status;
        }
        auto nativeStatus = curandCreateGenerator(generator, CURAND_RNG_PSEUDO_PHILOX4_32_10);
        if (nativeStatus != CURAND_STATUS_SUCCESS) {
            return detail::toStatus(nativeStatus);
        }

        nativeStatus = curandSetPseudoRandomGeneratorSeed(*generator, seed);
        if (nativeStatus == CURAND_STATUS_SUCCESS) {
            nativeStatus = curandSetGeneratorOrdering(*generator, CURAND_ORDERING_PSEUDO_BEST);
        }
        if (nativeStatus == CURAND_STATUS_SUCCESS) {
            nativeStatus = curandSetStream(*generator, KuVendor::nativeStream(stream));
        }
        if (nativeStatus == CURAND_STATUS_SUCCESS) {
            return KU_STATUS_SUCCESS;
        }
        (void)curandDestroyGenerator(*generator);
        *generator = nullptr;
        return detail::toStatus(nativeStatus);
    }

    static void destroyRandomGenerator(ku_device_id_t    device,
                                       curandGenerator_t generator) noexcept {
        (void)cudaSetDevice(device);
        (void)curandDestroyGenerator(generator);
    }

    BlasPool   m_blasPool;
    SolverPool m_solverPool;
    RandomPool m_randomPool;
};

} // namespace kuai::vendor::cuda
