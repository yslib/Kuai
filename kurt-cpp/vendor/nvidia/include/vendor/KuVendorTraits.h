#pragma once

#include <kuai/vendor/KuVendorTraits.h>

#include <vendor/KuVendor.h>
#include <vendor/KuVendorState.h>

namespace kuai {

template <>
struct ku_vendor_traits<vendor::cuda::KuVendor> : ku_vendor_traits_base<vendor::cuda::KuVendor> {
    static auto blas_handle(void *ctx, ku_device_id_t device, ku_stream_t stream) {
        return state(ctx).blasHandle(device, stream);
    }

    static auto solver_handle(void *ctx, ku_device_id_t device, ku_stream_t stream) {
        return state(ctx).solverHandle(device, stream);
    }

    static auto random_generator(void *ctx, ku_device_id_t device, ku_stream_t stream) {
        return state(ctx).randomGenerator(device, stream);
    }

private:
    static vendor::cuda::KuVendorState &state(void *ctx) noexcept {
        KU_ASSERT(ctx != nullptr, "CUDA vendor state must not be null");
        return *static_cast<vendor::cuda::KuVendorState *>(ctx);
    }
};

} // namespace kuai
