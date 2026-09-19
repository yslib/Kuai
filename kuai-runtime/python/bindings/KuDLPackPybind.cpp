#include "KuDLPackPybind.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <kuai/kuai_c/ku_object.h>
#include <kuai/kuai_c/ku_runtime.h>
#include <kuai/kuai_c/ku_tensor.h>

namespace kuai {
namespace py = pybind11;

namespace {

constexpr char kLegacyCapsuleName[] = "dltensor";
constexpr char kUsedLegacyCapsuleName[] = "used_dltensor";
constexpr char kVersionedCapsuleName[] = "dltensor_versioned";
constexpr char kUsedVersionedCapsuleName[] = "used_dltensor_versioned";

struct LegacyExportContext {
    DLManagedTensor           m_managed{};
    DLManagedTensorVersioned *m_versioned = nullptr;
    std::shared_ptr<void>     m_deviceOwner;

    ~LegacyExportContext() {
        if (m_versioned != nullptr && m_versioned->deleter != nullptr) {
            m_versioned->deleter(m_versioned);
        }
    }
};

struct VersionedExportContext {
    DLManagedTensorVersioned  m_managed{};
    DLManagedTensorVersioned *m_source = nullptr;
    std::shared_ptr<void>     m_deviceOwner;

    ~VersionedExportContext() {
        if (m_source != nullptr && m_source->deleter != nullptr) {
            m_source->deleter(m_source);
        }
    }
};

struct ManagedTensorDeleter {
    void operator()(DLManagedTensorVersioned *managed) const noexcept {
        if (managed != nullptr && managed->deleter != nullptr) {
            managed->deleter(managed);
        }
    }
};

struct LegacyImportContext {
    DLManagedTensorVersioned m_managed{};
    DLManagedTensor         *m_legacy = nullptr;
    std::vector<int64_t>     m_strides;
};

[[noreturn]] void throwInvalidLegacyDescriptor(const char *reason) {
    throw py::value_error(std::string("invalid legacy DLPack tensor descriptor: ") + reason);
}

void validateLegacyDescriptor(const DLTensor &tensor) {
    constexpr int32_t maxRank = 8;
    if (tensor.ndim < 0 || tensor.ndim > maxRank) {
        throwInvalidLegacyDescriptor("rank must be between 0 and 8");
    }
    if (tensor.ndim > 0 && tensor.shape == nullptr) {
        throwInvalidLegacyDescriptor("shape is required for non-scalar tensors");
    }
    for (int32_t index = 0; index < tensor.ndim; ++index) {
        if (tensor.shape[index] < 0) {
            throwInvalidLegacyDescriptor("extents must be nonnegative");
        }
    }
}

void synthesizeLegacyStrides(LegacyImportContext &context, const DLTensor &tensor) {
    context.m_strides.resize(static_cast<std::size_t>(tensor.ndim));
    context.m_strides[static_cast<std::size_t>(tensor.ndim - 1)] = 1;
    for (int index = tensor.ndim - 2; index >= 0; --index) {
        const int64_t nextStride = context.m_strides[static_cast<std::size_t>(index + 1)];
        const int64_t nextExtent = tensor.shape[index + 1];
        if (nextExtent != 0 && nextStride > std::numeric_limits<int64_t>::max() / nextExtent) {
            throwInvalidLegacyDescriptor("row-major stride synthesis overflows int64");
        }
        context.m_strides[static_cast<std::size_t>(index)] = nextStride * nextExtent;
    }
}

bool isTrue(py::handle value, const char *name) {
    if (value.is_none()) {
        return false;
    }
    if (!py::isinstance<py::bool_>(value)) {
        throw py::type_error(std::string(name) + " must be bool or None");
    }
    return py::cast<bool>(value);
}

template <typename Integer>
Integer parseExactNonnegativeInteger(py::handle value, const char *name, const char *rangeName) {
    if (PyLong_CheckExact(value.ptr()) == 0) {
        throw py::type_error(std::string(name) + " must be an exact int");
    }

    const auto parsed = PyLong_AsUnsignedLongLong(value.ptr());
    if (PyErr_Occurred() != nullptr) {
        if (!PyErr_ExceptionMatches(PyExc_OverflowError)) {
            throw py::error_already_set();
        }
        PyErr_Clear();
        throw py::value_error(std::string(name) + " must be in the nonnegative " + rangeName
                              + " range");
    }
    if (parsed > static_cast<unsigned long long>(std::numeric_limits<Integer>::max())) {
        throw py::value_error(std::string(name) + " must be in the nonnegative " + rangeName
                              + " range");
    }
    return static_cast<Integer>(parsed);
}

void validateStream(py::handle stream) {
    if (stream.is_none()) {
        return;
    }
    if (PyLong_CheckExact(stream.ptr()) == 0) {
        throw py::type_error("stream must be an exact int or None");
    }

    int             overflow = 0;
    const long long signedValue = PyLong_AsLongLongAndOverflow(stream.ptr(), &overflow);
    if (PyErr_Occurred() != nullptr) {
        throw py::error_already_set();
    }
    if (overflow < 0 || (overflow == 0 && signedValue < -1)) {
        throw py::value_error("stream is outside the supported DLPack integer range");
    }
    if (overflow == 0) {
        if (signedValue >= 0
            && static_cast<unsigned long long>(signedValue)
                   > static_cast<unsigned long long>(std::numeric_limits<std::uintptr_t>::max())) {
            throw py::value_error("stream is outside the supported DLPack integer range");
        }
        return;
    }

    const auto unsignedValue = PyLong_AsUnsignedLongLong(stream.ptr());
    if (PyErr_Occurred() != nullptr) {
        if (!PyErr_ExceptionMatches(PyExc_OverflowError)) {
            throw py::error_already_set();
        }
        PyErr_Clear();
        throw py::value_error("stream is outside the supported DLPack integer range");
    }
    if (unsignedValue
        > static_cast<unsigned long long>(std::numeric_limits<std::uintptr_t>::max())) {
        throw py::value_error("stream is outside the supported DLPack integer range");
    }
}

DLDevice toDLPackDevice(ku_device_info_t device) {
    DLDevice out{.device_type = kDLExtDev, .device_id = device.device_id};
    switch (device.device_type) {
        case KU_DEVICE_CPU:
            out.device_type = kDLCPU;
            break;
        case KU_DEVICE_CUDA:
            out.device_type = kDLCUDA;
            break;
        case KU_DEVICE_CUDA_HOST:
            out.device_type = kDLCUDAHost;
            break;
        case KU_DEVICE_CUDA_MANAGED:
            out.device_type = kDLCUDAManaged;
            break;
        case KU_DEVICE_EXT:
            out.device_type = kDLExtDev;
            break;
        default:
            throw py::buffer_error("kuai instance device cannot be represented by DLPack");
    }
    return out;
}

DLDevice tensorDLPackDevice(ku_device_t device) {
    ku_device_info_t  info{};
    const ku_status_t status = ku_device_get_info(device, &info);
    if (status != KU_STATUS_SUCCESS) {
        throw std::runtime_error("unable to query the kuai tensor device");
    }
    return toDLPackDevice(info);
}

py::tuple deviceTuple(DLDevice device) {
    return py::make_tuple(static_cast<int>(device.device_type), device.device_id);
}

DLDevice parseDevice(py::handle value, const char *name) {
    if (!py::isinstance<py::tuple>(value)) {
        throw py::type_error(std::string(name) + " must be a (device_type, device_id) tuple");
    }
    const auto tuple = py::reinterpret_borrow<py::tuple>(value);
    if (tuple.size() != 2) {
        throw py::type_error(std::string(name) + " must contain exactly two integers");
    }
    const std::string deviceTypeName = std::string(name) + "[0]";
    const std::string deviceIdName = std::string(name) + "[1]";
    const auto        deviceType =
        parseExactNonnegativeInteger<int32_t>(tuple[0], deviceTypeName.c_str(), "int32");
    const auto deviceId =
        parseExactNonnegativeInteger<int32_t>(tuple[1], deviceIdName.c_str(), "int32");
    return {
        .device_type = static_cast<DLDeviceType>(deviceType),
        .device_id = deviceId,
    };
}

bool sameDevice(DLDevice left, DLDevice right) noexcept {
    return left.device_type == right.device_type && left.device_id == right.device_id;
}

DLPackVersion parseMaxVersion(py::handle value, bool &versioned) {
    if (value.is_none()) {
        versioned = false;
        return {0, 0};
    }
    if (!py::isinstance<py::tuple>(value)) {
        throw py::type_error("max_version must be a (major, minor) tuple or None");
    }
    const auto tuple = py::reinterpret_borrow<py::tuple>(value);
    if (tuple.size() != 2) {
        throw py::type_error("max_version must contain exactly two integers");
    }
    const auto major = parseExactNonnegativeInteger<uint32_t>(tuple[0], "max_version[0]", "uint32");
    const auto minor = parseExactNonnegativeInteger<uint32_t>(tuple[1], "max_version[1]", "uint32");
    versioned = major >= 1;
    if (!versioned) {
        return {major, minor};
    }
    if (major > DLPACK_MAJOR_VERSION) {
        return {DLPACK_MAJOR_VERSION, DLPACK_MINOR_VERSION};
    }
    return {major, std::min(minor, static_cast<uint32_t>(DLPACK_MINOR_VERSION))};
}

void deleteLegacyExport(DLManagedTensor *managed) noexcept {
    if (managed != nullptr) {
        delete static_cast<LegacyExportContext *>(managed->manager_ctx);
    }
}

void deleteVersionedExport(DLManagedTensorVersioned *managed) noexcept {
    if (managed != nullptr) {
        delete static_cast<VersionedExportContext *>(managed->manager_ctx);
    }
}

void deleteLegacyImport(DLManagedTensorVersioned *managed) noexcept {
    if (managed == nullptr) {
        return;
    }
    auto *context = static_cast<LegacyImportContext *>(managed->manager_ctx);
    if (context->m_legacy != nullptr && context->m_legacy->deleter != nullptr) {
        context->m_legacy->deleter(context->m_legacy);
    }
    delete context;
}

void deleteDLPackCapsule(PyObject *capsule) noexcept {
    const char *name = PyCapsule_GetName(capsule);
    if (name == nullptr) {
        PyErr_Clear();
        return;
    }
    if (std::strcmp(name, kUsedLegacyCapsuleName) == 0
        || std::strcmp(name, kUsedVersionedCapsuleName) == 0) {
        return;
    }
    if (std::strcmp(name, kLegacyCapsuleName) == 0) {
        auto *managed =
            static_cast<DLManagedTensor *>(PyCapsule_GetPointer(capsule, kLegacyCapsuleName));
        if (managed != nullptr && managed->deleter != nullptr) {
            managed->deleter(managed);
        }
    } else if (std::strcmp(name, kVersionedCapsuleName) == 0) {
        auto *managed = static_cast<DLManagedTensorVersioned *>(
            PyCapsule_GetPointer(capsule, kVersionedCapsuleName));
        if (managed != nullptr && managed->deleter != nullptr) {
            managed->deleter(managed);
        }
    }
    if (PyErr_Occurred() != nullptr) {
        PyErr_Clear();
    }
}

void validateCapsuleDevice(const DLTensor &tensor, DLDevice expected) {
    if (!sameDevice(tensor.device, expected)) {
        throw py::buffer_error(
            "DLPack producer and kuai runtime are on different devices; explicit copy is required");
    }
}

void throwImportStatus(ku_status_t status) {
    switch (status) {
        case KU_STATUS_INVALID_ARGUMENT:
            throw py::value_error("invalid DLPack tensor descriptor");
        case KU_STATUS_OUT_OF_RANGE:
        case KU_STATUS_TYPE_MISMATCH:
        case KU_STATUS_INVALID_STATE:
        case KU_STATUS_ALREADY_INITIALIZED:
        case KU_STATUS_NOT_SUPPORTED:
            throw py::value_error(
                "DLPack dtype, device, rank, or strides are not supported without copying");
        case KU_STATUS_OUT_OF_HOST_MEMORY:
        case KU_STATUS_OUT_OF_DEVICE_MEMORY:
            PyErr_SetString(PyExc_MemoryError, "unable to allocate storage for a DLPack tensor");
            throw py::error_already_set();
        case KU_STATUS_INTERNAL_ERROR:
            throw std::runtime_error("unable to construct a kuai DLPack tensor view");
        case KU_STATUS_SUCCESS:
            return;
        default:
            throw std::runtime_error(std::string("unable to import a DLPack tensor: ")
                                     + ku_status_string(status));
    }
}

py::object callDLPackProducer(ku_device_t device, py::handle source, DLDevice target) {
    ku_stream_t defaultStream = nullptr;
    if (target.device_type != kDLCPU) {
        const ku_status_t status = ku_device_get_default_stream(device, &defaultStream);
        if (status != KU_STATUS_SUCCESS) {
            throw std::runtime_error("unable to query the kuai DLPack stream");
        }
    }
    const auto stream = target.device_type == kDLCPU
                            ? py::none()
                            : py::cast(reinterpret_cast<std::uintptr_t>(defaultStream));
    const auto method = source.attr("__dlpack__");
    try {
        return method(py::arg("stream") = stream, py::arg("max_version") = py::make_tuple(1, 3),
                      py::arg("dl_device") = deviceTuple(target), py::arg("copy") = false);
    } catch (py::error_already_set &error) {
        if (!error.matches(PyExc_TypeError)) {
            throw;
        }
        error.restore();
        PyErr_Clear();
    }

    try {
        return method(py::arg("stream") = stream);
    } catch (py::error_already_set &error) {
        if (!error.matches(PyExc_TypeError)) {
            throw;
        }
        error.restore();
        PyErr_Clear();
    }
    return method();
}

KuPyHandle importVersionedCapsule(ku_device_t device, py::handle capsule, DLDevice target) {
    auto *managed = static_cast<DLManagedTensorVersioned *>(
        PyCapsule_GetPointer(capsule.ptr(), kVersionedCapsuleName));
    if (managed == nullptr) {
        throw py::error_already_set();
    }
    if (managed->version.major != DLPACK_MAJOR_VERSION) {
        throw py::buffer_error("unsupported DLPack major version");
    }
    validateCapsuleDevice(managed->dl_tensor, target);
    if ((managed->flags & DLPACK_FLAG_BITMASK_IS_COPIED) != 0) {
        throw py::buffer_error("DLPack producer returned a copied tensor");
    }

    ku_object_t raw = nullptr;
    const auto  status = ku_tensor_from_dlpack(device, managed, &raw);
    if (status != KU_STATUS_SUCCESS) {
        throwImportStatus(status);
    }

    KuPyHandle tensor = KuPyHandle::adopt(raw);
    if (PyCapsule_SetName(capsule.ptr(), kUsedVersionedCapsuleName) != 0) {
        PyCapsule_SetDestructor(capsule.ptr(), nullptr);
        throw py::error_already_set();
    }
    return tensor;
}

KuPyHandle importLegacyCapsule(ku_device_t device, py::handle capsule, DLDevice target) {
    auto *legacy =
        static_cast<DLManagedTensor *>(PyCapsule_GetPointer(capsule.ptr(), kLegacyCapsuleName));
    if (legacy == nullptr) {
        throw py::error_already_set();
    }
    validateLegacyDescriptor(legacy->dl_tensor);
    validateCapsuleDevice(legacy->dl_tensor, target);

    auto context = std::make_unique<LegacyImportContext>();
    context->m_legacy = legacy;
    context->m_managed.version = {DLPACK_MAJOR_VERSION, DLPACK_MINOR_VERSION};
    context->m_managed.manager_ctx = context.get();
    context->m_managed.deleter = &deleteLegacyImport;
    context->m_managed.flags = 0;
    context->m_managed.dl_tensor = legacy->dl_tensor;
    if (legacy->dl_tensor.strides == nullptr && legacy->dl_tensor.ndim > 0) {
        synthesizeLegacyStrides(*context, legacy->dl_tensor);
        context->m_managed.dl_tensor.strides = context->m_strides.data();
    }

    ku_object_t raw = nullptr;
    const auto  status = ku_tensor_from_dlpack(device, &context->m_managed, &raw);
    if (status != KU_STATUS_SUCCESS) {
        throwImportStatus(status);
    }
    context.release();

    KuPyHandle tensor = KuPyHandle::adopt(raw);
    if (PyCapsule_SetName(capsule.ptr(), kUsedLegacyCapsuleName) != 0) {
        PyCapsule_SetDestructor(capsule.ptr(), nullptr);
        throw py::error_already_set();
    }
    return tensor;
}

} // namespace

py::tuple kuTensorDLPackDevice(ku_device_t device) {
    return deviceTuple(tensorDLPackDevice(device));
}

py::capsule kuTensorToDLPack(ku_object_t           tensor,
                             ku_device_t           device,
                             py::object            stream,
                             py::object            maxVersion,
                             py::object            dlDevice,
                             py::object            copy,
                             std::shared_ptr<void> deviceOwner) {
    if (isTrue(copy, "copy")) {
        throw py::buffer_error("kuai DLPack export does not perform implicit copies");
    }
    validateStream(stream);

    const auto sourceDevice = tensorDLPackDevice(device);
    if (!dlDevice.is_none() && !sameDevice(sourceDevice, parseDevice(dlDevice, "dl_device"))) {
        throw py::buffer_error("requested DLPack device requires a copy");
    }
    if (sourceDevice.device_type != kDLCPU) {
        ku_stream_t       defaultStream = nullptr;
        const ku_status_t streamStatus = ku_device_get_default_stream(device, &defaultStream);
        if (streamStatus != KU_STATUS_SUCCESS) {
            throw std::runtime_error("unable to synchronize the kuai DLPack producer stream");
        }
        ku_status_t synchronizeStatus = KU_STATUS_INTERNAL_ERROR;
        {
            py::gil_scoped_release release;
            synchronizeStatus = ku_device_synchronize(device, defaultStream);
        }
        if (synchronizeStatus != KU_STATUS_SUCCESS) {
            throw std::runtime_error("unable to synchronize the kuai DLPack producer stream");
        }
    }

    bool       useVersioned = false;
    const auto negotiatedVersion = parseMaxVersion(maxVersion, useVersioned);

    DLManagedTensorVersioned *rawManaged = nullptr;
    const auto                status = ku_tensor_to_dlpack(tensor, &rawManaged);
    if (status != KU_STATUS_SUCCESS) {
        throwImportStatus(status);
    }
    std::unique_ptr<DLManagedTensorVersioned, ManagedTensorDeleter> managed(rawManaged);

    if (useVersioned) {
        auto context = std::make_unique<VersionedExportContext>();
        context->m_managed = *managed;
        context->m_managed.version = negotiatedVersion;
        context->m_managed.manager_ctx = context.get();
        context->m_managed.deleter = &deleteVersionedExport;
        context->m_source = managed.release();
        context->m_deviceOwner = std::move(deviceOwner);
        auto capsule =
            py::capsule(&context->m_managed, kVersionedCapsuleName, &deleteDLPackCapsule);
        context.release();
        return capsule;
    }

    auto context = std::make_unique<LegacyExportContext>();
    context->m_managed.dl_tensor = managed->dl_tensor;
    context->m_managed.manager_ctx = context.get();
    context->m_managed.deleter = &deleteLegacyExport;
    context->m_versioned = managed.release();
    context->m_deviceOwner = std::move(deviceOwner);
    auto capsule = py::capsule(&context->m_managed, kLegacyCapsuleName, &deleteDLPackCapsule);
    context.release();
    return capsule;
}

KuPyHandle kuTensorFromDLPack(ku_device_t device, py::handle source, py::object copy) {
    if (isTrue(copy, "copy")) {
        throw py::buffer_error("kuai DLPack import does not perform implicit copies");
    }

    const auto target = tensorDLPackDevice(device);
    py::object capsule;
    if (PyCapsule_CheckExact(source.ptr()) != 0) {
        capsule = py::reinterpret_borrow<py::object>(source);
    } else {
        if (!py::hasattr(source, "__dlpack__") || !py::hasattr(source, "__dlpack_device__")) {
            throw py::type_error("from_dlpack expects an object implementing the DLPack protocol");
        }
        const auto sourceDevice =
            parseDevice(source.attr("__dlpack_device__")(), "__dlpack_device__ result");
        if (!sameDevice(sourceDevice, target)) {
            throw py::buffer_error("DLPack producer and kuai runtime are on different devices; use "
                                   "to_device for an explicit copy");
        }
        capsule = callDLPackProducer(device, source, target);
    }

    if (!PyCapsule_CheckExact(capsule.ptr())) {
        throw py::type_error("__dlpack__ must return a PyCapsule");
    }
    if (PyCapsule_IsValid(capsule.ptr(), kVersionedCapsuleName) != 0) {
        return importVersionedCapsule(device, capsule, target);
    }
    if (PyCapsule_IsValid(capsule.ptr(), kLegacyCapsuleName) != 0) {
        return importLegacyCapsule(device, capsule, target);
    }
    throw py::value_error("DLPack capsule is invalid or has already been consumed");
}

} // namespace kuai
