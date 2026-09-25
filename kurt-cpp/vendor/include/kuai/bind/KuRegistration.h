#pragma once

#include <vector>

#include <kuai/bind/KuModule.h>
#include <kuai/core/KuCore.h>
#include <kuai/ktl/KuDeviceMarker.h>

namespace kuai::bind {

using KuModuleLoader = void (*)(KuModule &);

inline std::vector<KuModuleLoader> &moduleLoaders() {
    static std::vector<KuModuleLoader> loaders;
    return loaders;
}

inline bool registerModuleLoader(KuModuleLoader loader) {
    moduleLoaders().push_back(loader);
    return true;
}

// Registration and captured state belong to this vendor, not to a host instance.
inline KuModule &builtinModule() {
    static auto module = [] {
        KuModule result;
        for (auto loader : moduleLoaders()) {
            loader(result);
        }
        return result;
    }();
    return module;
}

} // namespace kuai::bind

// CUDA device passes must not emit host registration objects.
#if KU_DEVICE_COMPILE_PASS
#define KU_REGISTER_MODULE_LOADER(ident)
#else
#define KU_REGISTER_MODULE_LOADER(ident) \
    static const bool KU_CONCAT(ident, _registered) = ::kuai::bind::registerModuleLoader(&ident);
#endif

#define __KU_VENDOR_MODULE(ident, variable)              \
    static void ident(::kuai::bind::KuModule &variable); \
    KU_REGISTER_MODULE_LOADER(ident)                     \
    static void ident(::kuai::bind::KuModule &variable)

#define KU_FUNC(variable) __KU_VENDOR_MODULE(KU_UNIQUE_NAME(_ku_vendor_builtin_init_), variable)
