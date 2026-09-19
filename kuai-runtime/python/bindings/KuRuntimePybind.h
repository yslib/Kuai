#pragma once

#include <pybind11/pybind11.h>

namespace kuai {

void bindKuRuntime(pybind11::module_ &module);

} // namespace kuai
