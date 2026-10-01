#include <kuai/core/KuCHandle.h>
#include <kuai/core/KuScalar.h>
#include <kuai/kuai_c/ku_scalar.h>

namespace kuai {
namespace {

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
