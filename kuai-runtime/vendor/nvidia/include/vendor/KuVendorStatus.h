#pragma once

#include <cublas_v2.h>
#include <curand.h>
#include <cusolver_common.h>

#include <kuai/kuai_c/ku_types.h>

namespace kuai::vendor::cuda::detail {

[[nodiscard]] constexpr ku_status_t toStatus(cublasStatus_t status) noexcept {
    switch (status) {
        case CUBLAS_STATUS_SUCCESS:
            return KU_STATUS_SUCCESS;
        case CUBLAS_STATUS_NOT_INITIALIZED:
            return KU_STATUS_BACKEND_UNAVAILABLE;
        case CUBLAS_STATUS_ALLOC_FAILED:
            return KU_STATUS_OUT_OF_DEVICE_MEMORY;
        case CUBLAS_STATUS_INVALID_VALUE:
            return KU_STATUS_INVALID_ARGUMENT;
        case CUBLAS_STATUS_ARCH_MISMATCH:
        case CUBLAS_STATUS_NOT_SUPPORTED:
            return KU_STATUS_NOT_SUPPORTED;
        case CUBLAS_STATUS_MAPPING_ERROR:
        case CUBLAS_STATUS_EXECUTION_FAILED:
            return KU_STATUS_DEVICE_ERROR;
        case CUBLAS_STATUS_INTERNAL_ERROR:
            return KU_STATUS_INTERNAL_ERROR;
        case CUBLAS_STATUS_LICENSE_ERROR:
            return KU_STATUS_BACKEND_UNAVAILABLE;
    }
    return KU_STATUS_DEVICE_ERROR;
}

[[nodiscard]] constexpr ku_status_t toStatus(cusolverStatus_t status) noexcept {
    switch (status) {
        case CUSOLVER_STATUS_SUCCESS:
            return KU_STATUS_SUCCESS;
        case CUSOLVER_STATUS_NOT_INITIALIZED:
        case CUSOLVER_STATUS_INVALID_LICENSE:
            return KU_STATUS_BACKEND_UNAVAILABLE;
        case CUSOLVER_STATUS_ALLOC_FAILED:
            return KU_STATUS_OUT_OF_DEVICE_MEMORY;
        case CUSOLVER_STATUS_INVALID_VALUE:
            return KU_STATUS_INVALID_ARGUMENT;
        case CUSOLVER_STATUS_ARCH_MISMATCH:
        case CUSOLVER_STATUS_MATRIX_TYPE_NOT_SUPPORTED:
        case CUSOLVER_STATUS_NOT_SUPPORTED:
            return KU_STATUS_NOT_SUPPORTED;
        case CUSOLVER_STATUS_MAPPING_ERROR:
        case CUSOLVER_STATUS_EXECUTION_FAILED:
            return KU_STATUS_DEVICE_ERROR;
        case CUSOLVER_STATUS_INTERNAL_ERROR:
            return KU_STATUS_INTERNAL_ERROR;
        default:
            return KU_STATUS_DEVICE_ERROR;
    }
}

[[nodiscard]] constexpr ku_status_t toStatus(curandStatus_t status) noexcept {
    switch (status) {
        case CURAND_STATUS_SUCCESS:
            return KU_STATUS_SUCCESS;
        case CURAND_STATUS_VERSION_MISMATCH:
        case CURAND_STATUS_NOT_INITIALIZED:
        case CURAND_STATUS_INITIALIZATION_FAILED:
            return KU_STATUS_BACKEND_UNAVAILABLE;
        case CURAND_STATUS_ALLOCATION_FAILED:
            return KU_STATUS_OUT_OF_DEVICE_MEMORY;
        case CURAND_STATUS_TYPE_ERROR:
        case CURAND_STATUS_LENGTH_NOT_MULTIPLE:
            return KU_STATUS_INVALID_ARGUMENT;
        case CURAND_STATUS_OUT_OF_RANGE:
            return KU_STATUS_OUT_OF_RANGE;
        case CURAND_STATUS_DOUBLE_PRECISION_REQUIRED:
        case CURAND_STATUS_ARCH_MISMATCH:
            return KU_STATUS_NOT_SUPPORTED;
        case CURAND_STATUS_LAUNCH_FAILURE:
        case CURAND_STATUS_PREEXISTING_FAILURE:
            return KU_STATUS_DEVICE_ERROR;
        case CURAND_STATUS_INTERNAL_ERROR:
            return KU_STATUS_INTERNAL_ERROR;
    }
    return KU_STATUS_DEVICE_ERROR;
}

} // namespace kuai::vendor::cuda::detail
