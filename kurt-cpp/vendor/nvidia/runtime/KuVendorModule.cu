#include <kuai/bind/KuRegistration.h>
#include <kuai/core/KuCore.h>
#include <kuai/vendor/KuVendor.h>

#include <vendor/KuVendorState.h>

extern "C" KU_EXPORT const ku_vendor_module_t *kuVendorModule(ku_host_t host) {
    (void)host;
    static kuai::vendor::cuda::KuVendorState state;
    static const ku_vendor_module_t          module{
                 .device_type = kuai::vendor::KuVendor::DeviceType,
                 .vendor_api = kuai::vendor::KuVendor::makeApi(&state),
                 .records_begin = KU_VENDOR_BUILTINS_RECORD_START,
                 .records_end = KU_VENDOR_BUILTINS_RECORD_STOP,
    };
    return &module;
}
