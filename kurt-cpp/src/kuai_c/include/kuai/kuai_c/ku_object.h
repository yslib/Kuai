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

/* Every non-NULL ku_object_t published by the runtime owns one intrusive reference. */
ku_status_t ku_object_retain(ku_object_t object);

ku_status_t ku_object_release(ku_object_t object);

/* Publishes a stable materializable value kind without exposing internal C++ RTTI values. */
ku_status_t ku_object_get_kind(ku_object_t object, ku_object_kind_t *out);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_OBJECT_H_
