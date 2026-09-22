#ifndef KURT_C_KU_CONTEXT_H_
#define KURT_C_KU_CONTEXT_H_

#include "ku_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Creates an owned frame context permanently bound to device. The context borrows
 * device, so it must be destroyed before the instance that owns device.
 */
ku_status_t ku_frame_ctx_create(ku_device_t device, ku_frame_ctx_t *out_ctx);

/* Returns the borrowed device handle supplied when ctx was created. */
ku_status_t ku_frame_ctx_get_device(ku_frame_ctx_t ctx, ku_device_t *out_device);

/* Destroys an owned frame context. A ku_frame_t only borrows its ctx. */
void ku_frame_ctx_destroy(ku_frame_ctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_CONTEXT_H_
