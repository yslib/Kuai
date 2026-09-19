#include <string_view>

#include <kuai/core/KuString.h>
#include <kuai/kuai_c/ku_string.h>

#include "kuai_c/KuCApiMethod.h"
#include "kuai_c/KuCHandle.h"

extern "C" ku_status_t ku_string_create(ku_string_view_t value, ku_object_t *out) {
    KU_ASSERT(value.data != nullptr, "ku_string_create requires non-null input storage");
    KU_ASSERT(out != nullptr, "ku_string_create requires a non-null output slot");
    *out = nullptr;

    return kuai::capi::invokeBoundary([&]() -> ku_status_t {
        auto string = ku_make_sp<kuai::KuString>(std::string_view(value.data, value.size));
        *out = kuai::capi::toHandle<ku_object_t>(string.detach());
        return KU_STATUS_SUCCESS;
    });
}

extern "C" ku_status_t ku_string_get_value(ku_object_t string, ku_string_view_t *out) {
    KU_ASSERT(string != nullptr, "ku_string_get_value requires a non-null string");
    KU_ASSERT(out != nullptr, "ku_string_get_value requires a non-null output slot");

    const auto *typed = kuai::capi::fromHandle(string)->template as<kuai::KuString>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    const auto value = typed->value();
    *out = ku_string_view_t{.data = value.data(), .size = value.size()};
    return KU_STATUS_SUCCESS;
}
