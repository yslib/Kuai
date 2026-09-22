#ifndef KURT_C_KU_ARRAY_H_
#define KURT_C_KU_ARRAY_H_

#include "ku_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Creates an immutable array from count borrowed, non-NULL object handles. The array retains
 * every element and returns one owned object reference. items itself follows the non-NULL
 * pointer contract even when count is zero.
 */
ku_status_t ku_array_create(const ku_object_t *items, ku_size_t count, ku_object_t *out);

ku_status_t ku_array_get_size(ku_object_t array, ku_size_t *out);

/* Returns one owned reference to the selected non-NULL element. */
ku_status_t ku_array_get(ku_object_t array, ku_size_t index, ku_object_t *out);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_ARRAY_H_
