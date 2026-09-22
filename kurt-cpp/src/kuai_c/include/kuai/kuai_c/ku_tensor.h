#ifndef KURT_C_KU_TENSOR_H_
#define KURT_C_KU_TENSOR_H_

#include "ku_completion.h"
#include "ku_object.h"
#include "ku_runtime.h"

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

/* A borrowed view of the tensor's native type and actual layout. */
typedef struct ku_tensor_info_t {
    ku_primitive_type_t primitive_type;
    int32_t             ndim;
    const ku_size_t    *shape;
    const ku_size_t    *strides;
} ku_tensor_info_t;

/*
 * Tensor API pointer contract:
 *
 * - Every opaque handle and pointer passed directly to the tensor APIs below is
 *   non-NULL, including input handles, descriptors, host buffers, and output slots.
 * - Output slots address writable storage. Their pointees are initialized by the callee.
 * - Violations are caller bugs and may only be diagnosed by debug assertions; a function
 *   does not return KU_STATUS_INVALID_ARGUMENT solely because a direct pointer argument is NULL.
 *
 * Pointer-valued fields inside descriptors retain their individual
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

/*
 * Returns KU_STATUS_SUCCESS and the tensor's native type, rank, shape, and actual strides.
 * For positive rank, shape and strides are non-NULL immutable arrays of ndim entries in
 * logical dimension order; strides are measured in elements, not bytes. Rank zero returns
 * NULL arrays and represents a scalar with one element. Positive-rank empty tensors preserve
 * their shape and actual strides, including zero extents and any zero strides.
 *
 * The descriptor is copied, but its arrays are borrowed and stable for the tensor's lifetime.
 * Copying it neither copies the arrays nor retains the tensor. Keep an owned reference to the
 * same native tensor and its instance alive while using the arrays; copy the arrays for a
 * detached snapshot. Logical element count is the shape product, starting with one.
 *
 * For another object kind, returns KU_STATUS_TYPE_MISMATCH and sets primitive_type to
 * KU_PRIMITIVE_NONE, ndim to zero, and both arrays to NULL. These are not successful metadata.
 * This query does not allocate, retain, release, synchronize, or read payload data. It does
 * not change upload readiness: the tensor must not be consumed or observed before its upload
 * completion reports success. Under the valid-object contract, only SUCCESS and TYPE_MISMATCH
 * are possible.
 */
ku_status_t ku_tensor_get_info(ku_object_t tensor, ku_tensor_info_t *out);

/*
 * Returns KU_STATUS_SUCCESS and a borrowed address at the tensor's logical first element,
 * not necessarily its allocation base. Empty tensors return NULL; nonempty tensors return
 * a non-NULL address. Another object kind returns KU_STATUS_TYPE_MISMATCH and sets *out to
 * NULL, so always inspect the status.
 *
 * The address belongs to the device returned by ku_tensor_get_device. This is not a host
 * mapping or download: host code must not dereference a CUDA device address. The mutable
 * raw pointer grants neither exclusive access nor writable ownership; obey storage permissions,
 * typed layout, aliasing, and synchronization requirements. Do not free it, infer its allocator,
 * or treat logical size as permission to access additional allocation capacity.
 *
 * Keep an owned reference to the same native tensor and its instance alive throughout every
 * operation using the address, including pending asynchronous copies. Retained handles share
 * storage; this query creates neither a copy nor a lock. It does not allocate, retain, release,
 * synchronize, select a device, or read payload data. A returned address does not establish
 * readiness: wait for successful upload completion and use the existing completion/device APIs
 * for ordering and ku_device_copy_async for host downloads. Under the valid-object contract,
 * only SUCCESS and TYPE_MISMATCH are possible.
 */
ku_status_t ku_tensor_get_data(ku_object_t tensor, void **out);

/*
 * Returns KU_STATUS_SUCCESS and the exact device handle associated with tensor.
 * The device borrows from its instance: do not release it. It remains valid after tensor
 * destruction until the instance is destroyed. Tensors must still be released before their
 * instance is destroyed. This query does not retain, allocate, or synchronize.
 * For another object kind, returns KU_STATUS_TYPE_MISMATCH and sets *out to NULL.
 */
ku_status_t ku_tensor_get_device(ku_object_t tensor, ku_device_t *out);

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

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_TENSOR_H_
