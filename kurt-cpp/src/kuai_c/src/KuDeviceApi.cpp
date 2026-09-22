#include <kuai/core/KuDevice.h>
#include <kuai/kuai_c/ku_runtime.h>
#include <kuai/runtime/KuCompletion.h>

#include "kuai_c/KuCApiMethod.h"

KU_DEFINE_C_API_VALUE_METHOD(
    ku_device_get_info, (ku_device_t device, ku_device_info_t *out), device, out, getDeviceInfo)

KU_DEFINE_C_API_BORROWED_METHOD(
    ku_device_get_instance, (ku_device_t device, ku_instance_t *out), device, out, getInstance)

KU_DEFINE_C_API_VALUE_METHOD(ku_device_get_capabilities,
                             (ku_device_t device, ku_device_capabilities_t *out),
                             device,
                             out,
                             getCapabilities)

KU_DEFINE_C_API_BORROWED_POINTER_METHOD(ku_device_get_vendor_api,
                                        (ku_device_t device, const ku_vendor_api_t **out),
                                        device,
                                        out,
                                        getVendorApi)

KU_DEFINE_C_API_VALUE_METHOD(ku_device_get_default_stream,
                             (ku_device_t device, ku_stream_t *out),
                             device,
                             out,
                             getDefaultStream)

KU_DEFINE_C_API_STATUS_METHOD(
    ku_device_synchronize, (ku_device_t device, ku_stream_t stream), device, synchronize, stream)

KU_DEFINE_C_API_STATUS_METHOD(ku_device_flush, (ku_device_t device), device, flush)

KU_DEFINE_C_API_OWNED_METHOD(ku_device_copy_async,
                             (ku_device_t      device,
                              void            *dst,
                              const void      *src,
                              ku_size_t        bytes,
                              ku_memcpy_kind_t kind,
                              ku_stream_t      dependency_stream,
                              ku_completion_t *out),
                             device,
                             out,
                             copyAsync,
                             dst,
                             src,
                             bytes,
                             kind,
                             dependency_stream)
