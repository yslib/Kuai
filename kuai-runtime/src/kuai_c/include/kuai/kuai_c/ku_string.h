#ifndef KURT_C_KU_STRING_H_
#define KURT_C_KU_STRING_H_

#include "ku_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Copies value into a newly allocated immutable string and returns one owned reference. */
ku_status_t ku_string_create(ku_string_view_t value, ku_object_t *out);

/* Returns a borrowed view that remains valid while string is alive. */
ku_status_t ku_string_get_value(ku_object_t string, ku_string_view_t *out);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_STRING_H_
