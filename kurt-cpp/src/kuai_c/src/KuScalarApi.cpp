#include <kuai/core/KuScalar.h>
#include <kuai/kuai_c/ku_scalar.h>

#include "kuai_c/KuCApiMethod.h"
#include "kuai_c/KuCHandle.h"

namespace kuai {
namespace {

bool isValidScalarTag(ku_primitive_type_t tag) noexcept {
    switch (tag) {
#define X(name, enum_value, display_name, payload_type, field) \
    case KU_PRIMITIVE_##name:                                  \
        return true;
        KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
        default:
            return false;
    }
}

ku_union_t copyScalarValue(const KuScalar &scalar) noexcept {
    switch (scalar.getType()) {
#define X(name, enum_value, display_name, payload_type, field) \
    case KU_PRIMITIVE_##name:                                  \
        return KuTypeTraits<payload_type>::make(scalar.value<payload_type>());
        KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
        default:
            KU_ASSERT(false, "KuScalar contains an unsupported primitive tag");
            return {};
    }
}

} // namespace
} // namespace kuai

extern "C" ku_status_t ku_scalar_create(const ku_union_t *value, ku_object_t *out) {
    KU_ASSERT(value != nullptr, "ku_scalar_create requires a non-null value");
    KU_ASSERT(out != nullptr, "ku_scalar_create requires a non-null output slot");
    *out = nullptr;
    if (!kuai::isValidScalarTag(value->tag)) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    return kuai::capi::invokeBoundary([&]() -> ku_status_t {
        auto scalar = ku_make_sp<kuai::KuScalar>(*value);
        *out = kuai::capi::toHandle<ku_object_t>(scalar.detach());
        return KU_STATUS_SUCCESS;
    });
}

extern "C" ku_status_t ku_scalar_get_value(ku_object_t scalar, ku_union_t *out) {
    KU_ASSERT(scalar != nullptr, "ku_scalar_get_value requires a non-null scalar");
    KU_ASSERT(out != nullptr, "ku_scalar_get_value requires a non-null output slot");
    const auto *typed = kuai::capi::fromHandle(scalar)->template as<kuai::KuScalar>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    *out = kuai::copyScalarValue(*typed);
    return KU_STATUS_SUCCESS;
}
