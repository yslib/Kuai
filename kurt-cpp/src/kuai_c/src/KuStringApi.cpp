#include <kuai/core/KuCHandle.h>
#include <kuai/core/KuString.h>
#include <kuai/kuai_c/ku_string.h>

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
