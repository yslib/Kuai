#include "kuai_c/KuTensorInterop.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdckdint.h>

#include <kuai/core/KuCore.h>
#include <kuai/core/KuDevice.h>
#include <kuai/core/KuTensor.h>
#include <kuai/kuai_c/ku_tensor.h>
#include <kuai/runtime/KuCompletion.h>

#include "kuai_c/KuCHandle.h"

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

bool toDLPackDevice(ku_device_info_t device, DLDevice &out) noexcept {
    out.device_id = device.device_id;
    switch (device.device_type) {
        case KU_DEVICE_CPU:
            out.device_type = kDLCPU;
            return true;
        case KU_DEVICE_CUDA:
            out.device_type = kDLCUDA;
            return true;
        case KU_DEVICE_CUDA_HOST:
            out.device_type = kDLCUDAHost;
            return true;
        case KU_DEVICE_CUDA_MANAGED:
            out.device_type = kDLCUDAManaged;
            return true;
        case KU_DEVICE_EXT:
            out.device_type = kDLExtDev;
            return true;
        default:
            return false;
    }
}

} // namespace

bool KuDLPackBuilder::toDataType(ku_primitive_type_t type, DLDataType &out) noexcept {
    out.lanes = 1;
    switch (type) {
        case KU_PRIMITIVE_BOOLEAN:
            out.code = kDLBool;
            out.bits = 8;
            return true;
        case KU_PRIMITIVE_BYTE:
            out.code = kDLInt;
            out.bits = 8;
            return true;
        case KU_PRIMITIVE_I16:
            out.code = kDLInt;
            out.bits = 16;
            return true;
        case KU_PRIMITIVE_I32:
            out.code = kDLInt;
            out.bits = 32;
            return true;
        case KU_PRIMITIVE_I64:
            out.code = kDLInt;
            out.bits = 64;
            return true;
        case KU_PRIMITIVE_F32:
            out.code = kDLFloat;
            out.bits = 32;
            return true;
        case KU_PRIMITIVE_F64:
            out.code = kDLFloat;
            out.bits = 64;
            return true;
        case KU_PRIMITIVE_NONE:
            return false;
    }
    return false;
}

bool KuDLPackBuilder::fillDevice(const KuTensor &tensor, DLDevice &out) noexcept {
    return toDLPackDevice(tensor.getDevice().getDeviceInfo(), out);
}

void KuDLPackBuilder::deleteExport(DLManagedTensorVersioned *managed) noexcept {
    if (managed != nullptr) {
        delete static_cast<ExportContext *>(managed->manager_ctx);
    }
}

ku_status_t KuDLPackBuilder::fillExport(ExportContext &context) noexcept {
    const auto &tensor = *context.m_tensor;
    auto       &dlTensor = context.m_managed.dl_tensor;

    if (!toDataType(tensor.getType(), dlTensor.dtype)) {
        return KU_STATUS_NOT_SUPPORTED;
    }
    if (!fillDevice(tensor, dlTensor.device)) {
        return KU_STATUS_NOT_SUPPORTED;
    }

    dlTensor.data = tensor.size() == 0 ? nullptr : const_cast<void *>(tensor.data());
    dlTensor.byte_offset = 0;

    dlTensor.ndim = static_cast<int32_t>(tensor.rank());
    if (tensor.rank() == 0) {
        dlTensor.ndim = 0;
        dlTensor.shape = nullptr;
        dlTensor.strides = nullptr;
        return KU_STATUS_SUCCESS;
    }

    for (std::size_t i = 0; i < tensor.rank(); ++i) {
        if (tensor.extent(i) > static_cast<ku_size_t>(std::numeric_limits<int64_t>::max())
            || tensor.stride(i) > static_cast<ku_size_t>(std::numeric_limits<int64_t>::max())) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
        context.m_shape[i] = static_cast<int64_t>(tensor.extent(i));
        context.m_strides[i] = static_cast<int64_t>(tensor.stride(i));
    }
    dlTensor.shape = context.m_shape.data();
    dlTensor.strides = context.m_strides.data();
    return KU_STATUS_SUCCESS;
}

ku_status_t KuDLPackBuilder::toDLPack(KuTensor &tensor, DLManagedTensorVersioned **out) noexcept {
    *out = nullptr;
    try {
        auto context = std::make_unique<ExportContext>();
        context->m_tensor = ku_ref_sp(&tensor);
        context->m_managed.version = {DLPACK_MAJOR_VERSION, DLPACK_MINOR_VERSION};
        context->m_managed.manager_ctx = context.get();
        context->m_managed.deleter = &deleteExport;
        context->m_managed.flags = 0;
        if (const auto status = fillExport(*context); status != KU_STATUS_SUCCESS) {
            return status;
        }
        *out = &context.release()->m_managed;
        return KU_STATUS_SUCCESS;
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (const std::invalid_argument &) {
        return KU_STATUS_INVALID_ARGUMENT;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
}

bool KuTensorViewBuilder::fromDataType(const DLDataType &type, ku_primitive_type_t &out) noexcept {
    if (type.lanes != 1) {
        return false;
    }
    if (type.code == kDLBool && type.bits == 8) {
        out = KU_PRIMITIVE_BOOLEAN;
        return true;
    }
    if (type.code == kDLInt) {
        switch (type.bits) {
            case 8:
                out = KU_PRIMITIVE_BYTE;
                return true;
            case 16:
                out = KU_PRIMITIVE_I16;
                return true;
            case 32:
                out = KU_PRIMITIVE_I32;
                return true;
            case 64:
                out = KU_PRIMITIVE_I64;
                return true;
            default:
                return false;
        }
    }
    if (type.code == kDLFloat) {
        if (type.bits == 32) {
            out = KU_PRIMITIVE_F32;
            return true;
        }
        if (type.bits == 64) {
            out = KU_PRIMITIVE_F64;
            return true;
        }
    }
    return false;
}

bool KuTensorViewBuilder::requiresExplicitStrides(const DLPackVersion &version) noexcept {
    return version.major > 1 || (version.major == 1 && version.minor >= 2);
}

bool KuTensorViewBuilder::checkedExtent(int64_t extent, ku_size_t &out) noexcept {
    if (extent < 0 || static_cast<uint64_t>(extent) > std::numeric_limits<ku_size_t>::max()) {
        return false;
    }
    out = static_cast<ku_size_t>(extent);
    return true;
}

void *KuTensorViewBuilder::offsetData(const DLTensor &tensor) noexcept {
    if (tensor.data == nullptr) {
        return nullptr;
    }
    return static_cast<void *>(static_cast<std::byte *>(tensor.data) + tensor.byte_offset);
}

ku_status_t KuTensorViewBuilder::importTensor(KuDevice                 &device,
                                              DLManagedTensorVersioned *managed,
                                              ku_sp<KuTensor>          &out) {
    if (managed->version.major != DLPACK_MAJOR_VERSION) {
        return KU_STATUS_NOT_SUPPORTED;
    }
    const auto &tensor = managed->dl_tensor;
    if ((managed->flags & DLPACK_FLAG_BITMASK_READ_ONLY) != 0) {
        return KU_STATUS_NOT_SUPPORTED;
    }

    ku_primitive_type_t dataType;
    if (!fromDataType(tensor.dtype, dataType)) {
        return KU_STATUS_NOT_SUPPORTED;
    }
    if (tensor.ndim < 0) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    if (tensor.ndim > static_cast<int32_t>(KuTensor::MaxRank)) {
        return KU_STATUS_NOT_SUPPORTED;
    }
    if ((tensor.ndim != 0 && tensor.shape == nullptr)
        || (tensor.ndim != 0 && tensor.strides == nullptr
            && requiresExplicitStrides(managed->version))) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    if (tensor.device.device_type != kDLCUDA || tensor.device.device_id != 0) {
        return KU_STATUS_NOT_SUPPORTED;
    }
    DLDevice expectedDevice{};
    if (!toDLPackDevice(device.getDeviceInfo(), expectedDevice)
        || tensor.device.device_type != expectedDevice.device_type
        || tensor.device.device_id != expectedDevice.device_id) {
        return KU_STATUS_INVALID_ARGUMENT;
    }

    auto                                     owner = std::make_shared<ImportOwner>();
    void                                    *data = offsetData(tensor);
    std::array<ku_size_t, KuTensor::MaxRank> shape{};
    std::array<ku_size_t, KuTensor::MaxRank> strides{};
    ku_size_t                                size = 1;
    ku_size_t                                canonicalStride = 1;
    for (int32_t i = 0; i < tensor.ndim; ++i) {
        auto &extent = shape[static_cast<std::size_t>(i)];
        if (!checkedExtent(tensor.shape[i], extent)) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
        if (multiplyOverflows(size, size, extent)) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
        if (tensor.strides != nullptr) {
            if (canonicalStride > static_cast<ku_size_t>(std::numeric_limits<int64_t>::max())) {
                return KU_STATUS_INVALID_ARGUMENT;
            }
            if (tensor.strides[i] != static_cast<int64_t>(canonicalStride)) {
                return KU_STATUS_NOT_SUPPORTED;
            }
        }
        strides[static_cast<std::size_t>(i)] = canonicalStride;
        if (multiplyOverflows(canonicalStride, canonicalStride, extent)) {
            return KU_STATUS_INVALID_ARGUMENT;
        }
    }
    if (size != 0 && data == nullptr) {
        return KU_STATUS_INVALID_ARGUMENT;
    }
    const auto rank = static_cast<std::size_t>(tensor.ndim);
    const auto shapeView = std::span<const ku_size_t>(shape.data(), rank);
    if (tensor.strides == nullptr) {
        out = ku_make_sp<KuTensor>(device, dataType, data, shapeView, size, owner);
    } else {
        out = ku_make_sp<KuTensor>(device, dataType, data, shapeView,
                                   std::span<const ku_size_t>(strides.data(), rank), size, owner);
    }
    owner->adopt(managed);
    return KU_STATUS_SUCCESS;
}

ku_status_t KuTensorViewBuilder::fromDLPack(KuDevice                 &device,
                                            DLManagedTensorVersioned *managed,
                                            ku_sp<KuTensor>          &out) noexcept {
    out.reset();
    try {
        return importTensor(device, managed, out);
    } catch (const std::bad_alloc &) {
        return KU_STATUS_OUT_OF_HOST_MEMORY;
    } catch (const std::invalid_argument &) {
        return KU_STATUS_INVALID_ARGUMENT;
    } catch (...) {
        return KU_STATUS_INTERNAL_ERROR;
    }
}

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

extern "C" ku_status_t ku_tensor_get_size(ku_object_t tensor, ku_size_t *out) {
    KU_ASSERT(tensor != nullptr, "ku_tensor_get_size requires a non-null tensor");
    KU_ASSERT(out != nullptr, "ku_tensor_get_size requires a non-null output slot");
    const auto *typed = kuai::capi::fromHandle(tensor)->template as<kuai::KuTensor>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    *out = typed->size();
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

#ifdef KURT_ENABLE_DLPACK
extern "C" ku_status_t ku_tensor_to_dlpack(ku_object_t tensor, DLManagedTensorVersioned **out) {
    KU_ASSERT(tensor != nullptr, "ku_tensor_to_dlpack requires a non-null tensor");
    KU_ASSERT(out != nullptr, "ku_tensor_to_dlpack requires a non-null output slot");
    *out = nullptr;
    auto *typed = kuai::capi::fromHandle(tensor)->template as<kuai::KuTensor>();
    if (typed == nullptr) {
        return KU_STATUS_TYPE_MISMATCH;
    }
    return kuai::KuDLPackBuilder::toDLPack(*typed, out);
}

extern "C" ku_status_t
ku_tensor_from_dlpack(ku_device_t device, DLManagedTensorVersioned *dlpack, ku_object_t *out) {
    KU_ASSERT(device != nullptr, "ku_tensor_from_dlpack requires a non-null device");
    KU_ASSERT(dlpack != nullptr, "ku_tensor_from_dlpack requires a non-null DLPack tensor");
    KU_ASSERT(out != nullptr, "ku_tensor_from_dlpack requires a non-null output slot");
    *out = nullptr;
    ku_sp<kuai::KuTensor> tensor;
    const auto            status =
        kuai::KuTensorViewBuilder::fromDLPack(*kuai::capi::fromHandle(device), dlpack, tensor);
    if (status == KU_STATUS_SUCCESS) {
        KU_ASSERT(tensor != nullptr, "successful DLPack import must return a tensor");
        *out = kuai::capi::toHandle<ku_object_t>(tensor.detach());
    }
    return status;
}
#endif
