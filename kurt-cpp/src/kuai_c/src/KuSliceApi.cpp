#include <kuai/core/KuCHandle.h>
#include <kuai/core/KuSlice.h>
#include <kuai/kuai_c/ku_slice.h>

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
