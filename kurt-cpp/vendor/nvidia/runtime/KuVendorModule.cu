#include <kuai/bind/KuRegistration.h>
#include <kuai/core/KuCore.h>
#include <kuai/vendor/KuVendor.h>

#include <vendor/KuVendorState.h>

extern "C" KU_EXPORT const ku_vendor_module_t *kuVendorModule(ku_host_t host) {
    (void)host;
    try {
        kuai::bind::builtinModule();
        static kuai::vendor::cuda::KuVendorState state;
        static const ku_vendor_module_t          module{
                     .device_type = kuai::vendor::KuVendor::DeviceType,
                     .vendor_api = kuai::vendor::KuVendor::makeApi(&state),
                     .get_builtin_info =
                [](ku_builtin_info_t *out, ku_size_t capacity, ku_size_t *outCount) noexcept {
                    return kuai::bind::builtinModule().getBuiltinInfo(out, capacity, outCount);
                },
                     .get_proc_address =
                [](ku_string_view_t name, ku_call_t *out) noexcept {
                    return kuai::bind::builtinModule().getProcAddress(name, out);
                },
        };
        return &module;
    } catch (...) {
        return nullptr;
    }
}
