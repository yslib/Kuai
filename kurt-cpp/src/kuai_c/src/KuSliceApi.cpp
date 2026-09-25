#include <kuai/core/KuSlice.h>
#include <kuai/kuai_c/KuCHandle.h>
#include <kuai/kuai_c/ku_slice.h>

#include "kuai_c/KuCApiMethod.h"

namespace kuai {
namespace {

constexpr ku_slice_flags_t kuSliceFlags =
    KU_SLICE_HAS_START | KU_SLICE_HAS_STOP | KU_SLICE_HAS_STEP;

bool isValidSlice(const ku_slice_desc_t &desc) noexcept {
    if ((desc.flags & ~kuSliceFlags) != 0) {
        return false;
    }
    return (desc.flags & KU_SLICE_HAS_STEP) == 0 || desc.step != 0;
}

} // namespace
} // namespace kuai

extern "C" ku_status_t ku_slice_create(const ku_slice_desc_t *desc, ku_object_t *out) {
    KU_ASSERT(desc != nullptr, "ku_slice_create requires a non-null descriptor");
    KU_ASSERT(out != nullptr, "ku_slice_create requires a non-null output slot");
    *out = nullptr;
    if (!kuai::isValidSlice(*desc)) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    return kuai::capi::invokeBoundary([&]() -> ku_status_t {
        auto slice = ku_make_sp<kuai::KuSlice>(*desc);
        *out = kuai::capi::toHandle<ku_object_t>(slice.detach());
        return KU_STATUS_SUCCESS;
    });
}

extern "C" ku_status_t ku_slice_get_value(ku_object_t slice, ku_slice_desc_t *out) {
    KU_ASSERT(slice != nullptr, "ku_slice_get_value requires a non-null slice");
    KU_ASSERT(out != nullptr, "ku_slice_get_value requires a non-null output slot");

    const auto *typed = kuai::capi::fromHandle(slice)->template as<kuai::KuSlice>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    *out = typed->value();
    return KU_STATUS_SUCCESS;
}
