#include "KuRuntimePybind.h"
#include "pybind11/pybind11.h"

namespace kuai {

PYBIND11_MODULE(_native, module) {
    bindKuRuntime(module);
}

} // namespace kuai
