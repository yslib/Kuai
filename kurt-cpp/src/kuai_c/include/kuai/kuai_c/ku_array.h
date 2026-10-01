#ifndef KURT_C_KU_ARRAY_H_
#define KURT_C_KU_ARRAY_H_

#include "ku_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ku_object_create(KU_OBJECT_ARRAY, ...) retains each of count borrowed elements.
 * items must be non-NULL and readable for count handles, even when count is zero.
 * Non-NULL elements must be live; a NULL element returns KU_STATUS_INVALID_ARGUMENT.
 */
typedef struct ku_array_create_desc_t {
    const ku_object_t *items;
    ku_size_t          count;
} ku_array_create_desc_t;

ku_status_t ku_array_get_size(ku_object_t array, ku_size_t *out);

/* Returns one owned reference to the selected non-NULL element. */
ku_status_t ku_array_get(ku_object_t array, ku_size_t index, ku_object_t *out);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_ARRAY_H_
