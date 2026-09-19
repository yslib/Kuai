#ifndef KURT_C_KU_TENSOR_H_
#define KURT_C_KU_TENSOR_H_

#include "ku_completion.h"
#include "ku_object.h"
#include "ku_runtime.h"

#define KURT_ENABLE_DLPACK 1
#ifdef KURT_ENABLE_DLPACK
#include "dlpack/dlpack.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ku_tensor_create_desc_t {
    ku_device_t         device;
    ku_primitive_type_t primitive_type;
    int32_t             ndim;
    const int64_t      *shape;
    const int64_t      *strides;
} ku_tensor_create_desc_t;

/*
 * Tensor API pointer contract:
 *
 * - Every opaque handle and pointer passed directly to the tensor APIs below is
 *   non-NULL, including input handles, descriptors, host buffers, and output slots.
 * - Output slots address writable storage. Their pointees are initialized by the callee.
 * - Violations are caller bugs and may only be diagnosed by debug assertions; a function
 *   does not return KU_STATUS_INVALID_ARGUMENT solely because a direct pointer argument is NULL.
 *
 * Pointer-valued fields inside descriptors or DLPack tensors retain their individual
 * semantic contracts and may still be validated at runtime.
 */
/*
 * Creates an owning contiguous column-major kuai tensor on desc->device.
 * ndim must be in [0, 8]. shape may be NULL only when ndim is zero. strides may be
 * NULL for the canonical layout; when supplied, it must describe that same canonical
 * layout in element units. The returned object has KU_VALUE_TENSOR kind, owns one reference,
 * and must be released with ku_object_release().
 */
ku_status_t ku_tensor_create(const ku_tensor_create_desc_t *desc, ku_object_t *out);

/* Returns the tensor's logical element count, or KU_STATUS_TYPE_MISMATCH for another object kind.
 */
ku_status_t ku_tensor_get_size(ku_object_t tensor, ku_size_t *out);

/*
 * Creates a tensor and asynchronously copies exactly bytes from borrowed host storage.
 * bytes must equal the tensor's logical byte size. On success both outputs own one reference;
 * release them through ku_object_release() and ku_completion_release(). On failure both outputs
 * are NULL. The runtime retains the tensor while the copy is in flight. src must remain valid,
 * and the tensor must not be consumed or observed, until the completion reports KU_STATUS_SUCCESS.
 */
ku_status_t ku_tensor_create_from_host_async(const ku_tensor_create_desc_t *desc,
                                             const void                    *src,
                                             ku_size_t                      bytes,
                                             ku_object_t                   *out_tensor,
                                             ku_completion_t               *out_completion);

#ifdef KURT_ENABLE_DLPACK
/* Creates a new owning DLPack export. The caller must invoke out->deleter exactly once. */
ku_status_t ku_tensor_to_dlpack(ku_object_t tensor, DLManagedTensorVersioned **out);
/*
 * Consumes dlpack on success and binds the resulting tensor to device. The DLPack descriptor
 * must identify that same device. Ownership remains with the caller on failure.
 */
ku_status_t
ku_tensor_from_dlpack(ku_device_t device, DLManagedTensorVersioned *dlpack, ku_object_t *out);
#endif

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_TENSOR_H_
