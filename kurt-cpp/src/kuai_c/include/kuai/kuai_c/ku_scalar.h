#ifndef KURT_C_KU_SCALAR_H_
#define KURT_C_KU_SCALAR_H_

#include "ku_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Copies value into a newly allocated scalar and returns one owned object reference. */
ku_status_t ku_scalar_create(const ku_union_t *value, ku_object_t *out);

/* Copies a scalar's tagged primitive value into out. */
ku_status_t ku_scalar_get_value(ku_object_t scalar, ku_union_t *out);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_SCALAR_H_
