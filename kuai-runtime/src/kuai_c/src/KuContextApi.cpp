#include <memory>

#include <kuai/core/KuDevice.h>
#include <kuai/core/KuFrameContext.h>
#include <kuai/kuai_c/ku_context.h>

#include "kuai_c/KuCApiMethod.h"
#include "kuai_c/KuCHandle.h"

extern "C" ku_status_t ku_frame_ctx_create(ku_device_t device, ku_frame_ctx_t *out_ctx) {
    KU_ASSERT(device != nullptr, "ku_frame_ctx_create requires a non-null device");
    KU_ASSERT(out_ctx != nullptr, "ku_frame_ctx_create requires a non-null output slot");
    *out_ctx = nullptr;
    return kuai::capi::invokeBoundary([&]() -> ku_status_t {
        auto context = std::make_unique<kuai::KuFrameContext>(*kuai::capi::fromHandle(device));
        *out_ctx = kuai::capi::toHandle<ku_frame_ctx_t>(context.release());
        return KU_STATUS_SUCCESS;
    });
}

KU_DEFINE_C_API_BORROWED_METHOD(ku_frame_ctx_get_device,
                                (ku_frame_ctx_t ctx, ku_device_t *out_device),
                                ctx,
                                out_device,
                                getDevice)

extern "C" void ku_frame_ctx_destroy(ku_frame_ctx_t ctx) {
    KU_ASSERT(ctx != nullptr, "ku_frame_ctx_destroy requires a non-null context");
    delete kuai::capi::fromHandle(ctx);
}
