#include <string_view>
#include <utility>

#include <kuai/core/KuArray.h>
#include <kuai/core/KuCHandle.h>
#include <kuai/core/KuObject.h>
#include <kuai/core/KuScalar.h>
#include <kuai/core/KuSlice.h>
#include <kuai/core/KuString.h>
#include <kuai/kuai_c/ku_array.h>
#include <kuai/kuai_c/ku_object.h>
#include <kuai/kuai_c/ku_scalar.h>
#include <kuai/kuai_c/ku_slice.h>
#include <kuai/kuai_c/ku_string.h>
#include <kuai/kuai_c/ku_tensor.h>

#include "kuai_c/KuCApiMethod.h"
#include "kuai_c/KuTensorInterop.h"

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

bool isValidSlice(const ku_slice_desc_t &desc) noexcept {
    constexpr ku_slice_flags_t flags = KU_SLICE_HAS_START | KU_SLICE_HAS_STOP | KU_SLICE_HAS_STEP;
    if ((desc.flags & ~flags) != 0) {
        return false;
    }
    return (desc.flags & KU_SLICE_HAS_STEP) == 0 || desc.step != 0;
}

} // namespace

extern "C" ku_status_t ku_object_create(ku_object_kind_t kind, const void *desc, ku_object_t *out) {
    KU_ASSERT(desc != nullptr, "ku_object_create requires a non-null descriptor");
    KU_ASSERT(out != nullptr, "ku_object_create requires a non-null output slot");
    *out = nullptr;

    return kuai::capi::invokeBoundary([&]() -> ku_status_t {
        ku_sp<kuai::KuObject> object;
        switch (kind) {
            case KU_OBJECT_SCALAR: {
                const auto &value = *static_cast<const ku_union_t *>(desc);
                if (!isValidScalarTag(value.tag)) {
                    return KU_STATUS_INVALID_ARGUMENT;
                }
                object = ku_make_sp<kuai::KuScalar>(value);
                break;
            }
            case KU_OBJECT_STRING: {
                const auto &value = *static_cast<const ku_string_view_t *>(desc);
                KU_ASSERT(value.data != nullptr, "string creation requires non-null storage");
                object = ku_make_sp<kuai::KuString>(std::string_view(value.data, value.size));
                break;
            }
            case KU_OBJECT_SLICE: {
                const auto &value = *static_cast<const ku_slice_desc_t *>(desc);
                if (!isValidSlice(value)) {
                    return KU_STATUS_INVALID_ARGUMENT;
                }
                object = ku_make_sp<kuai::KuSlice>(value);
                break;
            }
            case KU_OBJECT_ARRAY: {
                const auto &value = *static_cast<const ku_array_create_desc_t *>(desc);
                KU_ASSERT(value.items != nullptr, "array creation requires non-null items");
                auto array = ku_make_sp<kuai::KuArray>();
                array->reserve(value.count);
                for (ku_size_t index = 0; index < value.count; ++index) {
                    if (value.items[index] == nullptr) {
                        return KU_STATUS_INVALID_ARGUMENT;
                    }
                    array->append(ku_ref_sp(kuai::capi::fromHandle(value.items[index])));
                }
                object = std::move(array);
                break;
            }
            case KU_OBJECT_TENSOR: {
                const auto &value = *static_cast<const ku_tensor_create_desc_t *>(desc);
                KU_ASSERT(value.device != nullptr, "tensor creation requires a non-null device");
                ku_sp<kuai::KuTensor> tensor;
                const auto            status = kuai::KuTensorCreateBuilder::create(
                    *kuai::capi::fromHandle(value.device), value, tensor);
                if (status != KU_STATUS_SUCCESS) {
                    return status;
                }
                object = std::move(tensor);
                break;
            }
            default:
                return KU_STATUS_NOT_SUPPORTED;
        }
        *out = kuai::capi::toHandle<ku_object_t>(object.detach());
        return KU_STATUS_SUCCESS;
    });
}

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
