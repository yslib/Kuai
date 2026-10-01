#ifndef KURT_C_KU_OBJECT_H_
#define KURT_C_KU_OBJECT_H_

#include "ku_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stable C-facing classification of materializable runtime values. */
typedef int32_t ku_object_kind_t;

enum {
    KU_OBJECT_SCALAR = 1,
    KU_OBJECT_TENSOR = 2,
    KU_OBJECT_ARRAY = 3,
    KU_OBJECT_STRING = 4,
    KU_OBJECT_SLICE = 5
};

/*
 * Creates a fully constructed value and returns one owned object reference.
 * kind selects the exact descriptor type:
 *   KU_OBJECT_SCALAR: ku_union_t
 *   KU_OBJECT_TENSOR: ku_tensor_create_desc_t (ku_tensor.h)
 *   KU_OBJECT_ARRAY:  ku_array_create_desc_t  (ku_array.h)
 *   KU_OBJECT_STRING: ku_string_view_t
 *   KU_OBJECT_SLICE:  ku_slice_desc_t         (ku_slice.h)
 *
 * desc and out must be non-NULL and valid for this call; out must be writable.
 * For a supported kind, desc must point to a live, correctly aligned object of
 * the matching descriptor type. This is a caller contract, not a runtime type
 * check. Scalar payloads must match their tag. String data must be non-NULL and
 * readable for size bytes, even when size is zero. Other descriptor fields follow
 * the contracts documented on their types.
 * Descriptors are borrowed during the call; each type copies or retains the
 * data it needs. The existing tensor upload completion contract still applies.
 * Unsupported kinds return KU_STATUS_NOT_SUPPORTED without interpreting desc.
 * On any returned failure, *out is NULL. No partially constructed object escapes.
 */
ku_status_t ku_object_create(ku_object_kind_t kind, const void *desc, ku_object_t *out);

/* Every non-NULL ku_object_t published by the runtime owns one intrusive reference. */
ku_status_t ku_object_retain(ku_object_t object);

ku_status_t ku_object_release(ku_object_t object);

/* Publishes a stable materializable value kind without exposing internal C++ RTTI values. */
ku_status_t ku_object_get_kind(ku_object_t object, ku_object_kind_t *out);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_OBJECT_H_
