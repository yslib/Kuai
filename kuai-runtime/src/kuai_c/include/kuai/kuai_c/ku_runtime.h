#ifndef KURT_C_KU_RUNTIME_H_
#define KURT_C_KU_RUNTIME_H_

/*
 * Public C API for KuInstance, KuDevice, and backend vendor runtime capabilities.
 * Context, object, scalar, string, array, completion, tensor, and builtin-call APIs live in their
 * corresponding public headers. This header is intentionally not an umbrella.
 */

#include "ku_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t ku_device_id_t;
typedef int32_t ku_device_type_t;

enum {
    KU_DEVICE_CPU = 1,
    KU_DEVICE_CUDA = 2,
    KU_DEVICE_CUDA_HOST = 3,
    KU_DEVICE_CUDA_MANAGED = 4,
    KU_DEVICE_EXT = 12
};

typedef struct ku_device_info_t {
    ku_device_type_t device_type;
    ku_device_id_t   device_id;
} ku_device_info_t;

typedef int32_t ku_memcpy_kind_t;

enum {
    KU_MEMCPY_HOST_TO_DEVICE = 1,
    KU_MEMCPY_DEVICE_TO_HOST = 2,
    KU_MEMCPY_DEVICE_TO_DEVICE = 3
};

/*
 * Returned device handles are borrowed and remain valid for the owning instance lifetime.
 * Instance/device handles and output slots are trusted non-NULL contract arguments.
 * An unsupported device_id is an ordinary runtime error reported as KU_STATUS_OUT_OF_RANGE.
 */
ku_status_t ku_instance_get_default_device(ku_instance_t instance, ku_device_t *out);

ku_status_t
ku_instance_get_device(ku_instance_t instance, ku_device_id_t device_id, ku_device_t *out);

ku_status_t ku_device_get_info(ku_device_t device, ku_device_info_t *out);

/*
 * Returns the device's exact owning instance as a borrowed, stable handle, valid
 * until that instance is destroyed. This query does not retain, allocate, or
 * synchronize. Callers must not destroy the borrowed instance handle.
 * device must be valid and non-NULL; out must be valid, writable, and non-NULL.
 */
ku_status_t ku_device_get_instance(ku_device_t device, ku_instance_t *out);

/*
 * Borrowed, type-erased access to the backend primitives required by the
 * host instance. The descriptor and its ctx do not carry ownership. Every
 * function pointer is required to be non-NULL once installed in an instance.
 */
typedef struct ku_vendor_api_t {
    void *ctx;
    ku_status_t (*get_device_count)(void *ctx, int *count);
    ku_status_t (*get_device)(void *ctx, ku_device_id_t *device);
    ku_status_t (*set_device)(void *ctx, ku_device_id_t device);
    ku_stream_t (*get_per_thread_stream)(void *ctx);
    ku_status_t (*stream_create)(void *ctx, ku_stream_t *stream);
    ku_status_t (*stream_destroy)(void *ctx, ku_stream_t stream);
    /* KU_STATUS_SUCCESS means ready; KU_STATUS_NOT_READY is an ordinary incomplete state. */
    ku_status_t (*stream_query)(void *ctx, ku_stream_t stream);
    ku_status_t (*stream_synchronize)(void *ctx, ku_stream_t stream);
    /* Allocation exhaustion is reported as KU_STATUS_OUT_OF_DEVICE_MEMORY. */
    ku_status_t (*malloc_device)(void *ctx, void **ptr, ku_size_t bytes);
    ku_status_t (*free_device)(void *ctx, void *ptr);
    ku_status_t (*memory_pool_create)(void *ctx, ku_device_id_t device, ku_memory_pool_t *out);
    ku_status_t (*memory_pool_destroy)(void *ctx, ku_memory_pool_t pool);
    ku_status_t (*malloc_from_pool_async)(
        void *ctx, void **ptr, ku_size_t bytes, ku_memory_pool_t pool, ku_stream_t stream);
    ku_status_t (*free_async)(void *ctx, void *ptr, ku_stream_t stream);
    /* Allocation exhaustion is reported as KU_STATUS_OUT_OF_HOST_MEMORY. */
    ku_status_t (*malloc_host)(void *ctx, void **ptr, ku_size_t bytes);
    ku_status_t (*free_host)(void *ctx, void *ptr);
    ku_status_t (*memcpy)(
        void *ctx, void *dst, const void *src, ku_size_t bytes, ku_memcpy_kind_t kind);
    ku_status_t (*memcpy_async)(void            *ctx,
                                void            *dst,
                                const void      *src,
                                ku_size_t        bytes,
                                ku_memcpy_kind_t kind,
                                ku_stream_t      stream);
} ku_vendor_api_t;

/*
 * Borrowed table and context, valid until the owning instance is destroyed.
 * The table is backend-bound but not device-bound; direct callers must select
 * the intended device and obey each thunk's preconditions.
 */
ku_status_t ku_device_get_vendor_api(ku_device_t device, const ku_vendor_api_t **out);

typedef struct ku_builtin_builder_t ku_builtin_builder_t;

/*
 * A vendor builtin loader receives a borrowed builder callback view. Neither
 * the view nor anything reachable through its context may be retained after
 * the loader returns.
 */
typedef ku_status_t (*ku_vendor_builtin_loader_t)(const ku_builtin_builder_t *builder);

typedef struct ku_vendor_builtin_record_t {
    const char                *name;
    ku_vendor_builtin_loader_t loader;
} ku_vendor_builtin_record_t;

/* Borrowed descriptor exported by each libkurt_<vendor> module. */
typedef struct ku_vendor_module_t {
    ku_device_type_t                  device_type;
    ku_vendor_api_t                   vendor_api;
    const ku_vendor_builtin_record_t *records_begin;
    const ku_vendor_builtin_record_t *records_end;
} ku_vendor_module_t;

/*
 * The host handle is borrowed, non-NULL, and has process lifetime. The returned
 * descriptor and every pointer reachable from it have module lifetime.
 */
const ku_vendor_module_t *kuVendorModule(ku_host_t host);

typedef void (*ku_task_fn_t)(void *task_ctx);

typedef struct ku_task_t {
    ku_task_fn_t run;
    void        *ctx;
} ku_task_t;

typedef struct ku_scheduler_t {
    void *ctx;
    ku_status_t (*submit)(void *ctx, ku_task_t task);
} ku_scheduler_t;

typedef struct ku_device_stream_capabilities_t {
    uint32_t max_compute_streams;
    uint32_t max_copy_streams;
} ku_device_stream_capabilities_t;

typedef struct ku_device_capabilities_t {
    ku_device_id_t                  device_id;
    ku_device_stream_capabilities_t streams;
    uint64_t                        max_memory_bytes;
} ku_device_capabilities_t;

typedef struct ku_instance_capabilities_t {
    ku_scheduler_t                  scheduler;
    uint32_t                        max_worker_concurrency;
    uint64_t                        max_host_memory_bytes;
    const ku_device_capabilities_t *devices;
    ku_size_t                       device_count;
} ku_instance_capabilities_t;

typedef struct ku_instance_init_info_t {
    ku_string_view_t           vendor;
    ku_device_id_t             default_device_id;
    ku_instance_capabilities_t capabilities;
} ku_instance_init_info_t;

ku_status_t ku_instance_get_capabilities(ku_instance_t instance, ku_instance_capabilities_t *out);

ku_status_t ku_device_get_capabilities(ku_device_t device, ku_device_capabilities_t *out);

/*
 * Borrows the device's explicit owned stream until instance destruction.
 * This is independent of get_per_thread_stream(); callers must not destroy it.
 * Teardown drains host transfers and this stream before releasing the device
 * memory pool and destroying the stream. During default-stream and memory-pool
 * cleanup, failed selection or synchronization is diagnostic: those resources
 * are abandoned when safe release cannot be established.
 */
ku_status_t ku_device_get_default_stream(ku_device_t device, ku_stream_t *out);

ku_status_t ku_device_synchronize(ku_device_t device, ku_stream_t stream);

ku_status_t ku_device_flush(ku_device_t device);

/* Returns one owned completion reference on success and NULL on failure. */
ku_status_t ku_device_copy_async(ku_device_t      device,
                                 void            *dst,
                                 const void      *src,
                                 ku_size_t        bytes,
                                 ku_memcpy_kind_t kind,
                                 ku_stream_t      dependency_stream,
                                 ku_completion_t *out);

/*
 * One instance may be active for each lower-case backend vendor tag. The device
 * list is exact and is created atomically by ku_instance_init(). Instance handles
 * are uniquely owned lifecycle tokens: copying a handle does not retain it.
 *
 * ku_instance_destroy() always invalidates the instance and every handle derived
 * from it. Its status is diagnostic only; after it returns the same vendor tag
 * may be initialized again. kuai does not reset or validate process-global
 * state owned by the native backend runtime.
 */
ku_status_t ku_instance_init(const ku_instance_init_info_t *info, ku_instance_t *out);

ku_status_t ku_instance_flush(ku_instance_t instance);

ku_status_t ku_instance_destroy(ku_instance_t instance);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_RUNTIME_H_
