#include "kuai_c/KuTensorInterop.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdckdint.h>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuDevice.h>
#include <kuai/core/KuTensor.h>
#include <kuai/kuai_c/KuCHandle.h>
#include <kuai/kuai_c/ku_tensor.h>
#include <kuai/runtime/KuCompletion.h>

namespace kuai {
namespace {

bool multiplyOverflows(ku_size_t &result, ku_size_t lhs, ku_size_t rhs) noexcept {
#if defined(_STDCKDINT_H) && defined(ckd_mul)
    // Older GCC C headers expand ckd_mul using the C-only _Bool type, including
    // when reached through Clang's include_next. Use the same builtin in C++.
    return __builtin_mul_overflow(lhs, rhs, &result);
#else
    return ckd_mul(&result, lhs, rhs);
#endif
}

bool isValidPrimitiveType(ku_primitive_type_t type) noexcept {
    switch (type) {
#define X(name, enum_value, display_name, payload_type, field) \
    case KU_PRIMITIVE_##name:                                  \
        return true;
        KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
        default:
            return false;
    }
}

} // namespace

bool KuTensorCreateBuilder::checkedShape(const ku_tensor_create_desc_t &desc,
                                         Shape                         &out,
                                         ku_size_t                     &size) noexcept {
    if (desc.ndim < 0 || desc.ndim > static_cast<int32_t>(KuTensor::MaxRank)) {
        return false;
    }
    if (desc.ndim != 0 && desc.shape == nullptr) {
        return false;
    }
    size = 1;
    for (int32_t i = 0; i < desc.ndim; ++i) {
        if (desc.shape[i] < 0) {
            return false;
        }
        const auto extent = static_cast<ku_size_t>(desc.shape[i]);
        ku_size_t  nextSize;
        if (multiplyOverflows(nextSize, size, extent)) {
            return false;
        }
        out[static_cast<std::size_t>(i)] = extent;
        size = nextSize;
    }
    return true;
}

bool KuTensorCreateBuilder::hasSupportedLayout(const ku_tensor_create_desc_t &desc,
                                               const Shape                   &shape) noexcept {
    if (desc.strides == nullptr) {
        return true;
    }
    ku_size_t stride = 1;
    for (int32_t i = 0; i < desc.ndim; ++i) {
        if (stride > static_cast<ku_size_t>(std::numeric_limits<int64_t>::max())) {
            return false;
        }
        if (desc.strides[i] != static_cast<int64_t>(stride)) {
            return false;
        }
        const auto extent = shape[static_cast<std::size_t>(i)];
        if (multiplyOverflows(stride, stride, extent)) {
            return false;
        }
    }
    return true;
}

ku_status_t KuTensorCreateBuilder::create(KuDevice                      &device,
                                          const ku_tensor_create_desc_t &desc,
                                          ku_sp<KuTensor>               &out) noexcept {
    out.reset();
    if (!isValidPrimitiveType(desc.primitive_type)) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    Shape     shape{};
    ku_size_t size = 0;
    if (!checkedShape(desc, shape, size)) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    if (!hasSupportedLayout(desc, shape)) {
        return KU_STATUS_NOT_SUPPORTED;
    }

    const auto status =
        device.createTensor(desc.primitive_type, desc.ndim == 0 ? nullptr : shape.data(),
                            static_cast<ku_size_t>(desc.ndim), size, out);
    if (status != KU_STATUS_SUCCESS) {
        return status;
    }
    KU_ASSERT(out != nullptr, "successful device tensor creation must return a tensor");
    return KU_STATUS_SUCCESS;
}

} // namespace kuai

extern "C" ku_status_t ku_tensor_create(const ku_tensor_create_desc_t *desc, ku_object_t *out) {
    KU_ASSERT(desc != nullptr, "ku_tensor_create requires a non-null descriptor");
    KU_ASSERT(desc->device != nullptr, "ku_tensor_create requires a non-null device");
    KU_ASSERT(out != nullptr, "ku_tensor_create requires a non-null output slot");
    *out = nullptr;
    ku_sp<kuai::KuTensor> tensor;
    const auto            status =
        kuai::KuTensorCreateBuilder::create(*kuai::capi::fromHandle(desc->device), *desc, tensor);
    if (status == KU_STATUS_SUCCESS) {
        KU_ASSERT(tensor != nullptr, "successful device tensor creation must return a tensor");
        *out = kuai::capi::toHandle<ku_object_t>(tensor.detach());
    }
    return status;
}

extern "C" ku_status_t ku_tensor_get_info(ku_object_t tensor, ku_tensor_info_t *out) {
    KU_ASSERT(tensor != nullptr, "ku_tensor_get_info requires a non-null tensor");
    KU_ASSERT(out != nullptr, "ku_tensor_get_info requires a non-null output slot");
    *out = {KU_PRIMITIVE_NONE, 0, nullptr, nullptr};
    const auto *typed = kuai::capi::fromHandle(tensor)->template as<kuai::KuTensor>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    out->primitive_type = typed->getType();
    out->ndim = static_cast<int32_t>(typed->rank());
    if (out->ndim != 0) {
        out->shape = typed->shape().data();
        out->strides = typed->strides().data();
    }
    return KU_STATUS_SUCCESS;
}

extern "C" ku_status_t ku_tensor_get_data(ku_object_t tensor, void **out) {
    KU_ASSERT(tensor != nullptr, "ku_tensor_get_data requires a non-null tensor");
    KU_ASSERT(out != nullptr, "ku_tensor_get_data requires a non-null output slot");
    *out = nullptr;
    auto *typed = kuai::capi::fromHandle(tensor)->template as<kuai::KuTensor>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    *out = typed->size() == 0 ? nullptr : typed->data();
    return KU_STATUS_SUCCESS;
}

extern "C" ku_status_t ku_tensor_get_device(ku_object_t tensor, ku_device_t *out) {
    KU_ASSERT(tensor != nullptr, "ku_tensor_get_device requires a non-null tensor");
    KU_ASSERT(out != nullptr, "ku_tensor_get_device requires a non-null output slot");
    *out = nullptr;
    const auto *typed = kuai::capi::fromHandle(tensor)->template as<kuai::KuTensor>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    *out = kuai::capi::toHandle<ku_device_t>(&typed->getDevice());
    return KU_STATUS_SUCCESS;
}

extern "C" ku_status_t ku_tensor_create_from_host_async(const ku_tensor_create_desc_t *desc,
                                                        const void                    *src,
                                                        ku_size_t                      bytes,
                                                        ku_object_t                   *outTensor,
                                                        ku_completion_t *outCompletion) {
    KU_ASSERT(desc != nullptr, "ku_tensor_create_from_host_async requires a non-null descriptor");
    KU_ASSERT(desc->device != nullptr,
              "ku_tensor_create_from_host_async requires a non-null device");
    KU_ASSERT(src != nullptr, "ku_tensor_create_from_host_async requires a non-null source");
    KU_ASSERT(outTensor != nullptr,
              "ku_tensor_create_from_host_async requires a non-null tensor output slot");
    KU_ASSERT(outCompletion != nullptr,
              "ku_tensor_create_from_host_async requires a non-null completion output slot");
    *outTensor = nullptr;
    *outCompletion = nullptr;

    auto                 *device = kuai::capi::fromHandle(desc->device);
    ku_sp<kuai::KuTensor> tensor;
    const ku_status_t createStatus = kuai::KuTensorCreateBuilder::create(*device, *desc, tensor);
    if (createStatus != KU_STATUS_SUCCESS) {
        return createStatus;
    }
    KU_ASSERT(tensor != nullptr, "successful device tensor creation must return a tensor");
    if (bytes != tensor->bytes()) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    ku_sp<kuai::KuCompletion> completion;
    if (bytes == 0) {
        completion = ku_sp<kuai::KuCompletion>(new (std::nothrow) kuai::KuCompletion);
        if (!completion) {
            return KU_STATUS_OUT_OF_HOST_MEMORY;
        }
        completion->signal(KU_STATUS_SUCCESS);
    } else {
        const auto copyStatus =
            device->copyAsync(tensor->data(), src, bytes, KU_MEMCPY_HOST_TO_DEVICE,
                              device->getDefaultStream(), completion);
        if (copyStatus != KU_STATUS_SUCCESS) {
            return copyStatus;
        }
        KU_ASSERT(completion != nullptr,
                  "successful asynchronous tensor copy must return a completion");
    }

    completion->retainUntilCompletion(ku_sp<kuai::KuRefCounted>(tensor));
    *outTensor = kuai::capi::toHandle<ku_object_t>(tensor.detach());
    *outCompletion = kuai::capi::toHandle<ku_completion_t>(completion.detach());
    return KU_STATUS_SUCCESS;
}
