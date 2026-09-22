#include "KuRuntimePybind.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <kuai/kuai_c/ku_builtin.h>
#include <kuai/kuai_c/ku_completion.h>
#include <kuai/kuai_c/ku_context.h>
#include <kuai/kuai_c/ku_object.h>
#include <kuai/kuai_c/ku_runtime.h>
#include <kuai/kuai_c/ku_scalar.h>
#include <kuai/kuai_c/ku_slice.h>
#include <kuai/kuai_c/ku_string.h>
#include <kuai/kuai_c/ku_tensor.h>

#include "KuPyHandle.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"

namespace kuai {
namespace py = pybind11;
namespace {

[[noreturn]] void throwStatusMessage(ku_status_t status, std::string message) {
    switch (status) {
        case KU_STATUS_OUT_OF_HOST_MEMORY:
        case KU_STATUS_OUT_OF_DEVICE_MEMORY:
            PyErr_SetString(PyExc_MemoryError, message.c_str());
            throw py::error_already_set();
        case KU_STATUS_INVALID_ARGUMENT:
        case KU_STATUS_OUT_OF_RANGE:
        case KU_STATUS_TYPE_MISMATCH:
        case KU_STATUS_INVALID_STATE:
        case KU_STATUS_ALREADY_INITIALIZED:
        case KU_STATUS_NOT_SUPPORTED:
            throw py::value_error(std::move(message));
        default:
            throw std::runtime_error(std::move(message));
    }
}

[[noreturn]] void throwStatus(ku_status_t status, std::string_view context) {
    std::string message(context);
    message.append(": ");
    message.append(ku_status_string(status));
    throwStatusMessage(status, std::move(message));
}

void requireSuccess(ku_status_t status, std::string_view context) {
    if (status != KU_STATUS_SUCCESS) {
        throwStatus(status, context);
    }
}

[[nodiscard]] std::string
builtinStatusMessage(std::string_view name, std::string_view stage, ku_status_t status) {
    std::string message("builtin '");
    message.append(name);
    message.append("' ");
    message.append(stage);
    message.append(": ");
    message.append(ku_status_string(status));
    return message;
}

[[noreturn]] void
throwBuiltinStatus(std::string_view name, std::string_view stage, ku_status_t status) {
    std::string message = builtinStatusMessage(name, stage, status);
    if (status == KU_STATUS_NOT_FOUND) {
        PyErr_SetString(PyExc_NotImplementedError, message.c_str());
        throw py::error_already_set();
    }
    throwStatusMessage(status, std::move(message));
}

ku_status_t invokeCallTarget(const ku_call_t &target, ku_frame_t *frame) noexcept {
    switch (target.kind) {
        case KU_CALL_FFI:
            return target.value.ffi(frame);
        case KU_CALL_CALLABLE:
            return target.value.closure.ffi(target.value.closure.capture, frame);
        default:
            frame->result_count = 0;
            return KU_STATUS_INVALID_ARGUMENT;
    }
}

struct PrimitiveInfo {
    const char *m_name;
    std::size_t m_size;
};

PrimitiveInfo primitiveInfo(ku_primitive_type_t type) {
    switch (type) {
#define X(name, value, display_name, payload_type, field) \
    case KU_PRIMITIVE_##name:                             \
        return {display_name, sizeof(payload_type)};
        KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
        default:
            throw py::value_error("unsupported kuai primitive type");
    }
}

py::dtype primitiveNumpyDType(ku_primitive_type_t type) {
    switch (type) {
        case KU_PRIMITIVE_BOOLEAN:
            return py::dtype::of<bool>();
        case KU_PRIMITIVE_BYTE:
            return py::dtype::of<ku_char8_t>();
        case KU_PRIMITIVE_I16:
            return py::dtype::of<ku_i16_t>();
        case KU_PRIMITIVE_I32:
            return py::dtype::of<ku_i32_t>();
        case KU_PRIMITIVE_I64:
            return py::dtype::of<ku_i64_t>();
        case KU_PRIMITIVE_F32:
            return py::dtype::of<ku_f32_t>();
        case KU_PRIMITIVE_F64:
            return py::dtype::of<ku_f64_t>();
        case KU_PRIMITIVE_NONE:
            break;
    }
    throw py::value_error("unsupported kuai tensor primitive type");
}

[[nodiscard]] bool checkedMultiply(std::size_t left, std::size_t right, std::size_t &out) noexcept {
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        return false;
    }
    out = left * right;
    return true;
}

class KuPyCompletion final {
public:
    explicit KuPyCompletion(ku_completion_t value = nullptr) noexcept : m_value(value) {
    }

    KuPyCompletion(const KuPyCompletion &) = delete;
    KuPyCompletion &operator=(const KuPyCompletion &) = delete;

    ~KuPyCompletion() {
        if (m_value != nullptr) {
            (void)ku_completion_release(m_value);
        }
    }

    [[nodiscard]] ku_completion_t get() const noexcept {
        return m_value;
    }

private:
    ku_completion_t m_value;
};

// The caller keeps the native tensor alive while reading the borrowed arrays.
ku_tensor_info_t inspectTensor(ku_object_t tensor) {
    ku_tensor_info_t info{};
    requireSuccess(ku_tensor_get_info(tensor, &info), "unable to inspect the kuai tensor");
    return info;
}

class KuPyInstance;
class KuPyDevice;

class KuPyTensor final {
public:
    KuPyTensor(KuPyHandle handle, std::shared_ptr<KuPyDevice> device)
        : m_device(std::move(device)), m_handle(std::move(handle)) {
        if (!m_device || !m_handle) {
            throw std::invalid_argument("Tensor requires a device and a non-null object");
        }
    }

    KuPyTensor(const KuPyTensor &) = delete;
    KuPyTensor &operator=(const KuPyTensor &) = delete;
    KuPyTensor(KuPyTensor &&) = delete;
    KuPyTensor &operator=(KuPyTensor &&) = delete;

    [[nodiscard]] ku_object_t handle() const noexcept {
        return m_handle.get();
    }

    [[nodiscard]] const std::shared_ptr<KuPyDevice> &device() const noexcept {
        return m_device;
    }

private:
    // Declaration order is intentional: handle is destroyed before device.
    std::shared_ptr<KuPyDevice> m_device;
    KuPyHandle                  m_handle;
};

class KuPyDevice final {
public:
    KuPyDevice(std::shared_ptr<KuPyInstance> instance, ku_device_t device);
    ~KuPyDevice();

    KuPyDevice(const KuPyDevice &) = delete;
    KuPyDevice &operator=(const KuPyDevice &) = delete;
    KuPyDevice(KuPyDevice &&) = delete;
    KuPyDevice &operator=(KuPyDevice &&) = delete;

    [[nodiscard]] ku_device_t handle() const noexcept {
        return m_device;
    }

    [[nodiscard]] ku_frame_ctx_t context() const noexcept {
        return m_context;
    }

    [[nodiscard]] const std::shared_ptr<KuPyInstance> &instance() const noexcept {
        return m_instance;
    }

private:
    std::shared_ptr<KuPyInstance> m_instance;
    ku_device_t                   m_device = nullptr;
    ku_frame_ctx_t                m_context = nullptr;
};

class KuPyInstance final : public std::enable_shared_from_this<KuPyInstance> {
public:
    KuPyInstance(std::string                        vendor,
                 const std::vector<ku_device_id_t> &deviceIds,
                 ku_device_id_t                     defaultDeviceId);
    ~KuPyInstance();

    KuPyInstance(const KuPyInstance &) = delete;
    KuPyInstance &operator=(const KuPyInstance &) = delete;
    KuPyInstance(KuPyInstance &&) = delete;
    KuPyInstance &operator=(KuPyInstance &&) = delete;

    [[nodiscard]] ku_instance_t handle() const noexcept {
        return m_instance;
    }

    [[nodiscard]] const std::string &vendor() const noexcept {
        return m_vendor;
    }

    [[nodiscard]] std::shared_ptr<KuPyDevice> device(ku_device_id_t id);
    [[nodiscard]] std::shared_ptr<KuPyDevice> defaultDevice();
    void                                      flush();

private:
    std::string                                                   m_vendor;
    ku_instance_t                                                 m_instance = nullptr;
    ku_device_id_t                                                m_defaultDeviceId = 0;
    std::unordered_map<ku_device_id_t, std::weak_ptr<KuPyDevice>> m_devices;
};

KuPyDevice::KuPyDevice(std::shared_ptr<KuPyInstance> instance, ku_device_t device)
    : m_instance(std::move(instance)), m_device(device) {
    requireSuccess(ku_frame_ctx_create(m_device, &m_context),
                   "unable to create the kuai frame context");
}

KuPyDevice::~KuPyDevice() {
    if (m_context != nullptr) {
        ku_frame_ctx_destroy(m_context);
    }
}

KuPyInstance::KuPyInstance(std::string                        vendor,
                           const std::vector<ku_device_id_t> &deviceIds,
                           ku_device_id_t                     defaultDeviceId)
    : m_vendor(std::move(vendor)), m_defaultDeviceId(defaultDeviceId) {
    if (deviceIds.empty()) {
        throw std::invalid_argument("Instance requires at least one device ID");
    }

    std::unordered_set<ku_device_id_t> uniqueIds;
    uniqueIds.reserve(deviceIds.size());
    std::vector<ku_device_capabilities_t> deviceCapabilities;
    deviceCapabilities.reserve(deviceIds.size());
    for (const ku_device_id_t id : deviceIds) {
        if (!uniqueIds.insert(id).second) {
            throw std::invalid_argument("Instance device IDs must be unique");
        }
        deviceCapabilities.push_back(ku_device_capabilities_t{.device_id = id});
    }

    const ku_instance_capabilities_t capabilities{
        .devices = deviceCapabilities.data(),
        .device_count = deviceCapabilities.size(),
    };
    const ku_instance_init_info_t info{
        .vendor = {.data = m_vendor.data(), .size = m_vendor.size()},
        .default_device_id = m_defaultDeviceId,
        .capabilities = capabilities,
    };
    const ku_status_t status = ku_instance_init(&info, &m_instance);
    requireSuccess(status, "unable to create the kuai runtime instance");
}

KuPyInstance::~KuPyInstance() {
    if (m_instance != nullptr) {
        (void)ku_instance_destroy(m_instance);
    }
}

std::shared_ptr<KuPyDevice> KuPyInstance::device(ku_device_id_t id) {
    if (const auto found = m_devices.find(id); found != m_devices.end()) {
        if (auto device = found->second.lock()) {
            return device;
        }
    }

    ku_device_t rawDevice = nullptr;
    requireSuccess(ku_instance_get_device(m_instance, id, &rawDevice),
                   "unable to query the kuai device");
    auto device = std::make_shared<KuPyDevice>(shared_from_this(), rawDevice);
    m_devices.insert_or_assign(id, device);
    return device;
}

std::shared_ptr<KuPyDevice> KuPyInstance::defaultDevice() {
    return device(m_defaultDeviceId);
}

void KuPyInstance::flush() {
    requireSuccess(ku_instance_flush(m_instance), "unable to flush the kuai runtime instance");
}

ku_device_info_t deviceInfo(const KuPyDevice &device) {
    ku_device_info_t info{};
    requireSuccess(ku_device_get_info(device.handle(), &info),
                   "unable to query the kuai device information");
    return info;
}

std::shared_ptr<KuPyTensor> tensorFromHost(const std::shared_ptr<KuPyDevice> &device,
                                           py::buffer                         host,
                                           ku_primitive_type_t                primitiveType,
                                           const std::vector<std::int64_t>   &shape) {
    if (!device) {
        throw py::value_error("kuai tensor upload requires a device");
    }
    if (shape.size() > 8) {
        throw py::value_error("kuai tensor rank exceeds the supported maximum of 8");
    }

    const PrimitiveInfo primitive = primitiveInfo(primitiveType);
    if (primitiveType == KU_PRIMITIVE_NONE) {
        throw py::value_error("unsupported kuai tensor primitive type");
    }

    const py::buffer_info info = host.request();
    if (info.ndim < 0 || static_cast<std::size_t>(info.ndim) != shape.size()
        || info.shape.size() != shape.size() || info.strides.size() != shape.size()) {
        throw py::value_error("host buffer shape does not match requested tensor shape");
    }
    if (info.itemsize < 0 || static_cast<std::size_t>(info.itemsize) != primitive.m_size) {
        throw py::value_error("host buffer itemsize does not match requested kuai dtype "
                              + std::string(primitive.m_name));
    }

    std::size_t              elementCount = 1;
    std::vector<std::size_t> expectedByteStrides;
    expectedByteStrides.reserve(shape.size());
    for (std::size_t index = 0; index < shape.size(); ++index) {
        const std::int64_t extent = shape[index];
        if (extent < 0 || info.shape[index] < 0 || info.shape[index] != extent) {
            throw py::value_error("host buffer shape does not match requested tensor shape");
        }

        std::size_t expectedStride = 0;
        if (!checkedMultiply(elementCount, primitive.m_size, expectedStride)) {
            throw py::value_error("host buffer byte strides exceed the supported range");
        }
        expectedByteStrides.push_back(expectedStride);

        std::size_t nextCount = 0;
        if (!checkedMultiply(elementCount, static_cast<std::size_t>(extent), nextCount)) {
            throw py::value_error("host buffer element count exceeds the supported range");
        }
        elementCount = nextCount;
    }

    if (elementCount > static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max())
        || info.size < 0 || static_cast<std::size_t>(info.size) != elementCount) {
        throw py::value_error("host buffer element count does not match requested tensor shape");
    }
    if (elementCount != 0) {
        for (std::size_t index = 0; index < shape.size(); ++index) {
            if (shape[index] > 1
                && (expectedByteStrides[index]
                        > static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max())
                    || info.strides[index]
                           != static_cast<py::ssize_t>(expectedByteStrides[index]))) {
                throw py::value_error("host buffer must use column-major contiguous storage");
            }
        }
    }

    std::size_t byteCount = 0;
    if (!checkedMultiply(elementCount, primitive.m_size, byteCount)) {
        throw py::value_error("host buffer byte count exceeds the supported range");
    }
    if (byteCount != 0 && info.ptr == nullptr) {
        throw py::value_error("nonempty host buffer has a null data pointer");
    }

    const ku_tensor_create_desc_t descriptor{
        .device = device->handle(),
        .primitive_type = primitiveType,
        .ndim = static_cast<std::int32_t>(shape.size()),
        .shape = shape.empty() ? nullptr : shape.data(),
        .strides = nullptr,
    };
    static constexpr std::byte emptySource{};
    const void                *source = byteCount == 0 ? &emptySource : info.ptr;
    ku_object_t                rawTensor = nullptr;
    ku_completion_t            rawCompletion = nullptr;
    const ku_status_t status = ku_tensor_create_from_host_async(&descriptor, source, byteCount,
                                                                &rawTensor, &rawCompletion);
    KuPyHandle        tensor = KuPyHandle::adopt(rawTensor);
    KuPyCompletion    completion(rawCompletion);
    requireSuccess(status, "unable to create a kuai tensor from host data");
    if (!tensor || completion.get() == nullptr) {
        throw std::runtime_error("kuai tensor upload returned incomplete ownership");
    }

    ku_status_t completionStatus = KU_STATUS_INTERNAL_ERROR;
    {
        py::gil_scoped_release release;
        completionStatus = ku_completion_wait(completion.get());
    }
    requireSuccess(completionStatus, "kuai host tensor upload failed");
    return std::make_shared<KuPyTensor>(std::move(tensor), device);
}

ku_primitive_type_t tensorPrimitiveType(ku_object_t tensor) {
    return inspectTensor(tensor).primitive_type;
}

py::tuple tensorShape(ku_object_t tensor) {
    const auto descriptor = inspectTensor(tensor);
    py::tuple  shape(descriptor.ndim);
    for (std::int32_t index = 0; index < descriptor.ndim; ++index) {
        shape[index] = descriptor.shape[index];
    }
    return shape;
}

py::tuple tensorStrides(ku_object_t tensor) {
    const auto descriptor = inspectTensor(tensor);
    py::tuple  strides(descriptor.ndim);
    for (std::int32_t index = 0; index < descriptor.ndim; ++index) {
        strides[index] = descriptor.strides[index];
    }
    return strides;
}

ku_size_t tensorSize(ku_object_t tensor) {
    const auto info = inspectTensor(tensor);
    ku_size_t  size = 1;
    // Native tensor construction validates these prefix products for overflow.
    for (int32_t index = 0; index < info.ndim; ++index) {
        size *= info.shape[index];
    }
    return size;
}

KuPyHandle boxPythonScalar(py::handle value, std::string_view builtinName) {
    ku_union_t scalar{};
    if (value.is_none()) {
        scalar.tag = KU_PRIMITIVE_NONE;
    } else if (PyBool_Check(value.ptr()) != 0) {
        scalar.tag = KU_PRIMITIVE_BOOLEAN;
        scalar.boolean.value = py::cast<bool>(value) ? KU_BOOL_TRUE : KU_BOOL_FALSE;
    } else if (PyLong_CheckExact(value.ptr()) != 0) {
        scalar.tag = KU_PRIMITIVE_I64;
        scalar.i64 = py::cast<ku_i64_t>(value);
    } else if (PyFloat_CheckExact(value.ptr()) != 0) {
        scalar.tag = KU_PRIMITIVE_F64;
        scalar.f64 = py::cast<ku_f64_t>(value);
    } else {
        throw py::type_error("builtin scalar must be bool, int, or float");
    }

    ku_object_t       rawObject = nullptr;
    const ku_status_t status = ku_scalar_create(&scalar, &rawObject);
    KuPyHandle        object = KuPyHandle::adopt(rawObject);
    if (status != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(builtinName, "scalar boxing failed", status);
    }
    if (!object) {
        throw std::runtime_error("builtin '" + std::string(builtinName)
                                 + "' scalar boxing returned no object");
    }
    return object;
}

int64_t sliceFieldValue(py::handle value, std::string_view field) {
    PyObject *rawIndex = PyNumber_Index(value.ptr());
    if (rawIndex == nullptr) {
        throw py::error_already_set();
    }
    py::object index = py::reinterpret_steal<py::object>(rawIndex);

    int             overflow = 0;
    const long long converted = PyLong_AsLongLongAndOverflow(index.ptr(), &overflow);
    if (PyErr_Occurred() != nullptr) {
        throw py::error_already_set();
    }
    if (overflow != 0) {
        throw py::value_error("slice " + std::string(field) + " is outside the int64_t range");
    }
    static_assert(sizeof(long long) == sizeof(int64_t));
    return static_cast<int64_t>(converted);
}

void readSliceField(py::handle        slice,
                    const char       *name,
                    ku_slice_flags_t  flag,
                    int64_t          &value,
                    ku_slice_flags_t &flags) {
    py::object field = py::reinterpret_borrow<py::object>(slice).attr(name);
    if (!field.is_none()) {
        value = sliceFieldValue(field, name);
        flags |= flag;
    }
}

KuPyHandle boxPythonSlice(py::handle value, std::string_view builtinName) {
    ku_slice_desc_t slice{};
    readSliceField(value, "start", KU_SLICE_HAS_START, slice.start, slice.flags);
    readSliceField(value, "stop", KU_SLICE_HAS_STOP, slice.stop, slice.flags);
    readSliceField(value, "step", KU_SLICE_HAS_STEP, slice.step, slice.flags);

    ku_object_t       rawObject = nullptr;
    const ku_status_t status = ku_slice_create(&slice, &rawObject);
    KuPyHandle        object = KuPyHandle::adopt(rawObject);
    if (status != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(builtinName, "slice boxing failed", status);
    }
    if (!object) {
        throw std::runtime_error("builtin '" + std::string(builtinName)
                                 + "' slice boxing returned no object");
    }
    return object;
}

KuPyHandle boxPythonString(py::handle value, std::string_view builtinName) {
    Py_ssize_t  byteCount = 0;
    const char *bytes = PyUnicode_AsUTF8AndSize(value.ptr(), &byteCount);
    if (bytes == nullptr) {
        throw py::error_already_set();
    }
    if (byteCount < 0) {
        throw py::value_error("builtin string has an invalid UTF-8 byte count");
    }

    const ku_string_view_t stringValue{bytes, static_cast<ku_size_t>(byteCount)};
    ku_object_t            rawObject = nullptr;
    const ku_status_t      status = ku_string_create(stringValue, &rawObject);
    KuPyHandle             object = KuPyHandle::adopt(rawObject);
    if (status != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(builtinName, "string boxing failed", status);
    }
    if (!object) {
        throw std::runtime_error("builtin '" + std::string(builtinName)
                                 + "' string boxing returned no object");
    }
    return object;
}

py::object scalarToPython(ku_object_t object, std::string_view builtinName) {
    ku_union_t        value{};
    const ku_status_t status = ku_scalar_get_value(object, &value);
    if (status != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(builtinName, "scalar result conversion failed", status);
    }

    switch (value.tag) {
        case KU_PRIMITIVE_NONE:
            return py::none();
        case KU_PRIMITIVE_BOOLEAN:
            switch (value.boolean.value) {
                case KU_BOOL_TRUE:
                    return py::bool_(true);
                case KU_BOOL_FALSE:
                    return py::bool_(false);
                case KU_BOOL_NULL:
                    return py::none();
                default:
                    throw py::value_error("builtin '" + std::string(builtinName)
                                          + "' returned an invalid boolean value");
            }
        case KU_PRIMITIVE_BYTE:
            return py::int_(value.ch);
        case KU_PRIMITIVE_I16:
            return py::int_(value.i16);
        case KU_PRIMITIVE_I32:
            return py::int_(value.i32);
        case KU_PRIMITIVE_I64:
            return py::int_(value.i64);
        case KU_PRIMITIVE_F32:
            return py::float_(value.f32);
        case KU_PRIMITIVE_F64:
            return py::float_(value.f64);
        default:
            throw py::value_error("builtin '" + std::string(builtinName)
                                  + "' returned an unsupported scalar type");
    }
}

py::object stringToPython(ku_object_t object, std::string_view builtinName) {
    ku_string_view_t  value{};
    const ku_status_t status = ku_string_get_value(object, &value);
    if (status != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(builtinName, "string result conversion failed", status);
    }
    if (value.size > static_cast<ku_size_t>(std::numeric_limits<Py_ssize_t>::max())) {
        throw py::value_error("builtin '" + std::string(builtinName)
                              + "' returned a string that exceeds the Python size range");
    }

    PyObject *decoded =
        PyUnicode_DecodeUTF8(value.data, static_cast<Py_ssize_t>(value.size), "strict");
    if (decoded == nullptr) {
        throw py::error_already_set();
    }
    return py::reinterpret_steal<py::object>(decoded);
}

py::object invokeBuiltin(const std::shared_ptr<KuPyDevice> &device,
                         const std::string                 &name,
                         const py::tuple                   &operands) {
    if (!device) {
        throw py::value_error("builtin '" + name + "' requires a device");
    }

    std::vector<ku_object_t> arguments;
    arguments.reserve(operands.size());
    std::vector<KuPyHandle> boxedObjects;
    boxedObjects.reserve(operands.size());
    for (py::handle operand : operands) {
        if (py::isinstance<KuPyTensor>(operand)) {
            const auto &tensor = py::cast<const KuPyTensor &>(operand);
            arguments.push_back(tensor.handle());
        } else if (operand.is_none() || PyBool_Check(operand.ptr()) != 0
                   || PyLong_CheckExact(operand.ptr()) != 0
                   || PyFloat_CheckExact(operand.ptr()) != 0) {
            boxedObjects.push_back(boxPythonScalar(operand, name));
            arguments.push_back(boxedObjects.back().get());
        } else if (PySlice_Check(operand.ptr()) != 0) {
            boxedObjects.push_back(boxPythonSlice(operand, name));
            arguments.push_back(boxedObjects.back().get());
        } else if (PyUnicode_CheckExact(operand.ptr()) != 0) {
            boxedObjects.push_back(boxPythonString(operand, name));
            arguments.push_back(boxedObjects.back().get());
        } else {
            throw py::type_error(
                "builtin operands must be Tensor, bool, int, float, str, None, or slice");
        }
    }

    ku_call_t              target{};
    const ku_string_view_t nameView{name.data(), name.size()};
    const ku_status_t      lookupStatus =
        ku_instance_get_proc_address(device->instance()->handle(), nameView, &target);
    if (lookupStatus != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(name, "lookup failed", lookupStatus);
    }

    ku_object_t argumentSentinel = nullptr;
    ku_object_t resultSlot = nullptr;
    ku_frame_t  frame{
         .ctx = device->context(),
         .argv = arguments.empty() ? &argumentSentinel : arguments.data(),
         .pargn = arguments.size(),
         .knames = nullptr,
         .kargn = 0,
         .results = &resultSlot,
         .result_capacity = 1,
         .result_count = 0,
    };
    ku_status_t invocationStatus = KU_STATUS_INTERNAL_ERROR;
    {
        py::gil_scoped_release release;
        invocationStatus = invokeCallTarget(target, &frame);
    }
    KuPyHandle result = KuPyHandle::adopt(resultSlot);

    if (invocationStatus == KU_STATUS_BUFFER_TOO_SMALL) {
        throw std::runtime_error(builtinStatusMessage(name, "invocation failed", invocationStatus)
                                 + "; the Python binding supports exactly one result");
    }
    if (invocationStatus != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(name, "invocation failed", invocationStatus);
    }
    if (frame.result_count != 1) {
        throw std::runtime_error("builtin '" + name + "' violated the single-result protocol");
    }
    if (!result) {
        throw std::runtime_error("builtin '" + name + "' returned a null result");
    }

    ku_object_kind_t  resultKind = 0;
    const ku_status_t kindStatus = ku_object_get_kind(result.get(), &resultKind);
    if (kindStatus != KU_STATUS_SUCCESS) {
        throwBuiltinStatus(name, "result kind query failed", kindStatus);
    }
    switch (resultKind) {
        case KU_OBJECT_TENSOR:
            return py::cast(std::make_shared<KuPyTensor>(std::move(result), device));
        case KU_OBJECT_SCALAR:
            return scalarToPython(result.get(), name);
        case KU_OBJECT_STRING:
            return stringToPython(result.get(), name);
        case KU_OBJECT_ARRAY:
        default:
            throw std::runtime_error("builtin '" + name + "' returned an unsupported result kind");
    }
}

py::array tensorToHost(const std::shared_ptr<KuPyTensor> &tensor) {
    if (!tensor) {
        throw py::value_error("kuai host transfer requires a tensor");
    }

    const auto                descriptor = inspectTensor(tensor->handle());
    const ku_primitive_type_t primitiveType = descriptor.primitive_type;
    const PrimitiveInfo       primitive = primitiveInfo(primitiveType);
    const py::dtype           numpyDType = primitiveNumpyDType(primitiveType);
    if (numpyDType.itemsize() < 0
        || static_cast<std::size_t>(numpyDType.itemsize()) != primitive.m_size) {
        throw std::runtime_error("inconsistent NumPy and kuai primitive type sizes");
    }

    const std::size_t        rank = static_cast<std::size_t>(descriptor.ndim);
    std::vector<py::ssize_t> numpyShape;
    numpyShape.reserve(rank);
    std::size_t elementCount = 1;
    for (std::size_t index = 0; index < rank; ++index) {
        const ku_size_t extent = descriptor.shape[index];
        if (extent > static_cast<ku_size_t>(std::numeric_limits<py::ssize_t>::max())) {
            throw py::value_error("kuai tensor extent exceeds the NumPy range");
        }
        numpyShape.push_back(static_cast<py::ssize_t>(extent));
        std::size_t nextCount = 0;
        if (!checkedMultiply(elementCount, static_cast<std::size_t>(extent), nextCount)) {
            throw py::value_error("kuai tensor element count exceeds the supported range");
        }
        elementCount = nextCount;
    }
    if (elementCount > static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max())) {
        throw py::value_error("kuai tensor element count exceeds the NumPy range");
    }

    std::vector<py::ssize_t> numpyStrides;
    numpyStrides.reserve(rank);
    std::size_t canonicalStride = 1;
    for (std::size_t index = 0; index < rank; ++index) {
        const ku_size_t stride = descriptor.strides[index];
        if (elementCount != 0 && descriptor.shape[index] > 1 && stride != canonicalStride) {
            throw py::value_error("noncontiguous kuai tensor strides are unsupported");
        }

        std::size_t byteStride = 0;
        if (!checkedMultiply(stride, primitive.m_size, byteStride)
            || byteStride > static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max())) {
            throw py::value_error("kuai tensor byte strides exceed the NumPy range");
        }
        numpyStrides.push_back(static_cast<py::ssize_t>(byteStride));

        std::size_t nextCanonicalStride = 0;
        if (!checkedMultiply(canonicalStride, static_cast<std::size_t>(descriptor.shape[index]),
                             nextCanonicalStride)) {
            throw py::value_error("kuai tensor canonical strides exceed the supported range");
        }
        canonicalStride = nextCanonicalStride;
    }

    std::size_t byteCount = 0;
    if (!checkedMultiply(elementCount, primitive.m_size, byteCount)) {
        throw py::value_error("kuai tensor byte count exceeds the supported range");
    }
    void *source = nullptr;
    requireSuccess(ku_tensor_get_data(tensor->handle(), &source),
                   "unable to query the kuai tensor data");

    py::array storage(numpyDType, static_cast<py::ssize_t>(elementCount));
    if (byteCount != 0) {
        ku_stream_t defaultStream = nullptr;
        requireSuccess(ku_device_get_default_stream(tensor->device()->handle(), &defaultStream),
                       "unable to query the kuai device default stream");

        ku_completion_t   rawCompletion = nullptr;
        const ku_status_t copyStatus = ku_device_copy_async(
            tensor->device()->handle(), storage.mutable_data(), source, byteCount,
            KU_MEMCPY_DEVICE_TO_HOST, defaultStream, &rawCompletion);
        KuPyCompletion completion(rawCompletion);
        requireSuccess(copyStatus, "unable to copy the kuai tensor to host memory");
        if (completion.get() == nullptr) {
            throw std::runtime_error("kuai tensor download returned no completion");
        }

        ku_status_t completionStatus = KU_STATUS_INTERNAL_ERROR;
        {
            py::gil_scoped_release release;
            completionStatus = ku_completion_wait(completion.get());
        }
        requireSuccess(completionStatus, "kuai tensor download failed");
    }

    return py::array(numpyDType, numpyShape, numpyStrides, storage.mutable_data(), storage);
}

} // namespace

void bindKuRuntime(py::module_ &module) {
    py::dict primitiveTypes;
    primitiveTypes["boolean"] = static_cast<std::int32_t>(KU_PRIMITIVE_BOOLEAN);
    primitiveTypes["byte"] = static_cast<std::int32_t>(KU_PRIMITIVE_BYTE);
    primitiveTypes["i16"] = static_cast<std::int32_t>(KU_PRIMITIVE_I16);
    primitiveTypes["i32"] = static_cast<std::int32_t>(KU_PRIMITIVE_I32);
    primitiveTypes["i64"] = static_cast<std::int32_t>(KU_PRIMITIVE_I64);
    primitiveTypes["f32"] = static_cast<std::int32_t>(KU_PRIMITIVE_F32);
    primitiveTypes["f64"] = static_cast<std::int32_t>(KU_PRIMITIVE_F64);
    module.attr("_primitive_types") = std::move(primitiveTypes);

    module.def("_to_device", &tensorFromHost, py::arg("device"), py::arg("host"),
               py::arg("primitive_type"), py::arg("shape"));
    module.def("_to_host", &tensorToHost, py::arg("tensor"));
    module.def("_invoke_builtin", &invokeBuiltin, py::arg("device"), py::arg("name"),
               py::arg("operands"));

    py::class_<KuPyInstance, std::shared_ptr<KuPyInstance>>(module, "Instance")
        .def(py::init<std::string, const std::vector<ku_device_id_t> &, ku_device_id_t>(),
             py::arg("vendor"), py::kw_only(), py::arg("device_ids") = py::make_tuple(0),
             py::arg("default_device_id") = 0)
        .def_property_readonly("vendor", &KuPyInstance::vendor)
        .def_property_readonly("default_device", &KuPyInstance::defaultDevice)
        .def("device", &KuPyInstance::device, py::arg("id"))
        .def("flush", &KuPyInstance::flush, py::call_guard<py::gil_scoped_release>());

    py::class_<KuPyDevice, std::shared_ptr<KuPyDevice>>(module, "Device")
        .def_property_readonly(
            "id", [](const KuPyDevice &device) { return deviceInfo(device).device_id; })
        .def_property_readonly(
            "type", [](const KuPyDevice &device) { return deviceInfo(device).device_type; })
        .def_property_readonly("instance", &KuPyDevice::instance)
        .def(
            "flush",
            [](const KuPyDevice &device) {
                requireSuccess(ku_device_flush(device.handle()), "unable to flush the kuai device");
            },
            py::call_guard<py::gil_scoped_release>());

    py::class_<KuPyTensor, std::shared_ptr<KuPyTensor>>(module, "Tensor")
        .def_property_readonly("device", &KuPyTensor::device)
        .def_property_readonly("dtype",
                               [](const KuPyTensor &tensor) {
                                   return primitiveNumpyDType(tensorPrimitiveType(tensor.handle()));
                               })
        .def_property_readonly(
            "shape", [](const KuPyTensor &tensor) { return tensorShape(tensor.handle()); })
        .def_property_readonly(
            "strides", [](const KuPyTensor &tensor) { return tensorStrides(tensor.handle()); })
        .def_property_readonly(
            "ndim", [](const KuPyTensor &tensor) { return tensorShape(tensor.handle()).size(); })
        .def_property_readonly(
            "size", [](const KuPyTensor &tensor) { return tensorSize(tensor.handle()); });
}

} // namespace kuai
