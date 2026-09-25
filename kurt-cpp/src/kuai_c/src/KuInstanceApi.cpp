#include <kuai/core/KuCore.h>
#include <kuai/kuai_c/KuCHandle.h>
#include <kuai/kuai_c/ku_builtin.h>
#include <kuai/kuai_c/ku_runtime.h>
#include <kuai/runtime/KuInstance.h>

#include "kuai_c/KuCApiMethod.h"
#include "runtime/KuInstanceRegistry.h"

extern "C" ku_status_t ku_instance_init(const ku_instance_init_info_t *info, ku_instance_t *out) {
    KU_ASSERT(info != nullptr, "ku_instance_init requires a non-null descriptor");
    KU_ASSERT(out != nullptr, "ku_instance_init requires a non-null output slot");
    *out = nullptr;
    return kuai::capi::invokeBoundary([&] {
        kuai::KuInstance *instance = nullptr;
        const auto        status = kuai::KuInstanceRegistry::getInstance().init(*info, &instance);
        if (status == KU_STATUS_SUCCESS) {
            KU_ASSERT(instance != nullptr,
                      "successful instance initialization must return an instance");
            *out = kuai::capi::toHandle<ku_instance_t>(instance);
        }
        return status;
    });
}

extern "C" ku_status_t ku_instance_flush(ku_instance_t instance) {
    KU_ASSERT(instance != nullptr, "ku_instance_flush requires a non-null instance");
    return kuai::capi::invokeBoundary([&] {
        return kuai::KuInstanceRegistry::getInstance().flush(kuai::capi::fromHandle(instance));
    });
}

extern "C" ku_status_t ku_instance_destroy(ku_instance_t instance) {
    KU_ASSERT(instance != nullptr, "ku_instance_destroy requires a non-null instance");
    return kuai::capi::invokeBoundary([&] {
        return kuai::KuInstanceRegistry::getInstance().destroy(kuai::capi::fromHandle(instance));
    });
}

KU_DEFINE_C_API_BORROWED_METHOD(ku_instance_get_default_device,
                                (ku_instance_t instance, ku_device_t *out),
                                instance,
                                out,
                                getDefaultDevice)

KU_DEFINE_C_API_BORROWED_STATUS_METHOD(ku_instance_get_device,
                                       (ku_instance_t  instance,
                                        ku_device_id_t device_id,
                                        ku_device_t   *out),
                                       instance,
                                       out,
                                       getDevice,
                                       device_id)

KU_DEFINE_C_API_STATUS_METHOD(ku_instance_get_capabilities,
                              (ku_instance_t instance, ku_instance_capabilities_t *out),
                              instance,
                              getCapabilities,
                              out)

KU_DEFINE_C_API_STATUS_METHOD(
    ku_instance_get_builtin_info,
    (ku_instance_t instance, ku_builtin_info_t *out, ku_size_t capacity, ku_size_t *out_count),
    instance,
    getBuiltinInfo,
    out,
    capacity,
    out_count)

KU_DEFINE_C_API_STATUS_METHOD(ku_instance_get_proc_address,
                              (ku_instance_t instance, ku_string_view_t name, ku_call_t *out),
                              instance,
                              getKuProcAddress,
                              name,
                              out)
