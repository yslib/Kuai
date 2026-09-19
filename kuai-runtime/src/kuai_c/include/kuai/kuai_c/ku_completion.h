#ifndef KURT_C_KU_COMPLETION_H_
#define KURT_C_KU_COMPLETION_H_

#include "ku_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Completion callbacks may run on an internal worker thread. They must return
 * quickly and must not throw across the C ABI. A callback registered after the
 * operation has completed is invoked before ku_completion_on_completion() returns. The
 * user_data object must remain valid until the callback returns. Waiting and
 * callback dispatch are independent, so ku_completion_wait() may return while
 * a registered callback is still running.
 */
typedef void (*ku_completion_callback_t)(void *user_data, ku_status_t status);

/* Completion handles are owning, one-shot asynchronous operation results. */
ku_status_t ku_completion_retain(ku_completion_t completion);

ku_status_t ku_completion_release(ku_completion_t completion);

/* Waits for completion and returns the asynchronous operation's final status. */
ku_status_t ku_completion_wait(ku_completion_t completion);

/* Registers the completion's single callback. */
ku_status_t ku_completion_on_completion(ku_completion_t          completion,
                                        ku_completion_callback_t callback,
                                        void                    *user_data);

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_COMPLETION_H_
