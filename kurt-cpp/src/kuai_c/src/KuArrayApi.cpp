#include <kuai/core/KuArray.h>
#include <kuai/kuai_c/KuCHandle.h>
#include <kuai/kuai_c/ku_array.h>

#include "kuai_c/KuCApiMethod.h"

extern "C" ku_status_t
ku_array_create(const ku_object_t *items, ku_size_t count, ku_object_t *out) {
    KU_ASSERT(items != nullptr, "ku_array_create requires a non-null item array");
    KU_ASSERT(out != nullptr, "ku_array_create requires a non-null output slot");
    *out = nullptr;

    return kuai::capi::invokeBoundary([&]() -> ku_status_t {
        auto array = ku_make_sp<kuai::KuArray>();
        array->reserve(count);
        for (ku_size_t index = 0; index < count; ++index) {
            if (items[index] == nullptr) {
                return KU_STATUS_INVALID_ARGUMENT;
            }
            array->append(ku_ref_sp(kuai::capi::fromHandle(items[index])));
        }
        *out = kuai::capi::toHandle<ku_object_t>(array.detach());
        return KU_STATUS_SUCCESS;
    });
}

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
