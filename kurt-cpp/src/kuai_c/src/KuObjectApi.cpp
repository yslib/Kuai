#include <kuai/core/KuObject.h>
#include <kuai/core/KuSlice.h>
#include <kuai/core/KuString.h>
#include <kuai/kuai_c/ku_object.h>

#include "kuai_c/KuCHandle.h"

extern "C" ku_status_t ku_object_get_kind(ku_object_t object, ku_object_kind_t *out) {
    KU_ASSERT(object != nullptr, "ku_object_get_kind requires a non-null object");
    KU_ASSERT(out != nullptr, "ku_object_get_kind requires a non-null output slot");

    ku_object_kind_t valueKind;
    switch (kuai::capi::fromHandle(object)->getKind().value()) {
        case kuai::kScalar.value():
            valueKind = KU_OBJECT_SCALAR;
            break;
        case kuai::kTensor.value():
            valueKind = KU_OBJECT_TENSOR;
            break;
        case kuai::kArray.value():
            valueKind = KU_OBJECT_ARRAY;
            break;
        case kuai::kString.value():
            valueKind = KU_OBJECT_STRING;
            break;
        case kuai::kSlice.value():
            valueKind = KU_OBJECT_SLICE;
            break;
        default:
            return KU_STATUS_NOT_SUPPORTED;
    }
    *out = valueKind;
    return KU_STATUS_SUCCESS;
}
