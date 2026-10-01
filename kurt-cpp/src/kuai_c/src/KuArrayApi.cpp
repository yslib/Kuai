#include <kuai/core/KuArray.h>
#include <kuai/core/KuCHandle.h>
#include <kuai/kuai_c/ku_array.h>

extern "C" ku_status_t ku_array_get_size(ku_object_t array, ku_size_t *out) {
    KU_ASSERT(array != nullptr, "ku_array_get_size requires a non-null array");
    KU_ASSERT(out != nullptr, "ku_array_get_size requires a non-null output slot");
    const auto *typed = kuai::capi::fromHandle(array)->template as<kuai::KuArray>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    *out = typed->size();
    return KU_STATUS_SUCCESS;
}

extern "C" ku_status_t ku_array_get(ku_object_t array, ku_size_t index, ku_object_t *out) {
    KU_ASSERT(array != nullptr, "ku_array_get requires a non-null array");
    KU_ASSERT(out != nullptr, "ku_array_get requires a non-null output slot");
    *out = nullptr;

    const auto *typed = kuai::capi::fromHandle(array)->template as<kuai::KuArray>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    if (index >= typed->size()) {
        return KU_STATUS_OUT_OF_RANGE;
    }
    auto item = typed->retain(index);
    if (item == nullptr) {
        return KU_STATUS_INTERNAL_ERROR;
    }
    *out = kuai::capi::toHandle<ku_object_t>(item.detach());
    return KU_STATUS_SUCCESS;
}
