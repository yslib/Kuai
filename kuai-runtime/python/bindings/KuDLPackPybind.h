#pragma once

#include <memory>

#include <kuai/kuai_c/ku_runtime.h>

#include "KuPyHandle.h"
#include "pybind11/pybind11.h"

namespace kuai {

pybind11::tuple kuTensorDLPackDevice(ku_device_t device);

pybind11::capsule kuTensorToDLPack(ku_object_t           tensor,
                                   ku_device_t           device,
                                   pybind11::object      stream,
                                   pybind11::object      maxVersion,
                                   pybind11::object      dlDevice,
                                   pybind11::object      copy,
                                   std::shared_ptr<void> deviceOwner);

KuPyHandle kuTensorFromDLPack(ku_device_t device, pybind11::handle source, pybind11::object copy);

} // namespace kuai
