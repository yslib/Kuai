#ifndef KURT_C_KU_SLICE_H_
#define KURT_C_KU_SLICE_H_

#include "ku_object.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ku_slice_flags_t;

enum {
    KU_SLICE_HAS_START = 1u << 0,
    KU_SLICE_HAS_STOP = 1u << 1,
    KU_SLICE_HAS_STEP = 1u << 2
};

/* Copied by ku_object_create(KU_OBJECT_SLICE, ...); an explicit step must be nonzero. */
typedef struct ku_slice_desc_t {
    int64_t          start;
    int64_t          stop;
    int64_t          step;
    ku_slice_flags_t flags;
} ku_slice_desc_t;

/* Copies the immutable slice descriptor into out. */
ku_status_t ku_slice_get_value(ku_object_t slice, ku_slice_desc_t *out);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_SLICE_H_
