#ifndef KURT_C_KU_BUILTIN_H_
#define KURT_C_KU_BUILTIN_H_

#include "ku_object.h"
#include "ku_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ku_builtin_info_t {
    ku_string_view_t name;
} ku_builtin_info_t;

/*
 * Two-phase borrowed builtin enumeration. out may be NULL only when capacity is zero.
 * On KU_STATUS_BUFFER_TOO_SMALL, out_count receives the required count and out remains untouched.
 */
ku_status_t ku_instance_get_builtin_info(ku_instance_t      instance,
                                         ku_builtin_info_t *out,
                                         ku_size_t          capacity,
                                         ku_size_t         *out_count);

/* Borrowed frame for one validated call. Invalid field combinations are caller errors. */
typedef struct ku_frame_t {
    /* Borrowed; may be NULL unless the callable requires a frame context. */
    ku_frame_ctx_t ctx;
    /* Borrowed, non-NULL array of pargn + kargn nullable objects; positional values first. */
    const ku_object_t *argv;
    /* Number of positional values in argv. */
    ku_size_t pargn;
    /* Borrowed keyword names; may be NULL only when kargn is zero. */
    const ku_string_view_t *knames;
    /* Number of keyword values; knames[i] names argv[pargn + i]. */
    ku_size_t kargn;
    /* Non-NULL output array; every non-NULL result carries one owned reference. */
    ku_object_t *results;
    /* Writable entry count in results; always greater than zero. */
    ku_size_t result_capacity;
    /* Actual count on success, required count if too small, otherwise zero. */
    ku_size_t result_count;
} ku_frame_t;

typedef ku_status_t (*ku_ffi_t)(ku_frame_t *);
typedef ku_status_t (*ku_closure_ffi_t)(void *, ku_frame_t *);

/* Non-owning callable view. Its producer-defined owner keeps capture alive while the target is
 * used. */
typedef struct ku_closure_t {
    void            *capture;
    ku_closure_ffi_t ffi;
} ku_closure_t;

/* A call target is either a direct FFI or a captured callable; every other kind is invalid. */
typedef int32_t ku_call_target_kind_t;

enum {
    KU_CALL_TARGET_FFI = 1,
    KU_CALL_TARGET_CALLABLE = 2,
};

typedef union ku_call_target_value_t {
    ku_ffi_t     ffi;
    ku_closure_t closure;
} ku_call_target_value_t;

typedef struct ku_call_target_t {
    ku_call_target_kind_t  kind;
    ku_call_target_value_t value;
} ku_call_target_t;

/*
 * Writes a borrowed invocable target only on success. A resolved target invokes the registered
 * matcher before entering a typed handler, so registered argument-shape validation applies even
 * when a name has only one registration.
 */
ku_status_t
ku_instance_get_proc_address(ku_instance_t instance, ku_string_view_t name, ku_call_target_t *out);

typedef int32_t ku_builtin_match_rank_t;

enum {
    KU_BUILTIN_NO_MATCH = -1,
};

typedef ku_builtin_match_rank_t (*ku_builtin_match_t)(void *state, const ku_frame_t *frame);
typedef void (*ku_builtin_destroy_t)(void *state);

/*
 * A successful builder add transfers state ownership to the receiver. On an
 * error, state remains owned by the producer. A non-NULL state must have a
 * non-NULL destroy callback.
 */
typedef struct ku_builtin_registration_t {
    ku_string_view_t     name;
    const void          *overload_key;
    ku_call_target_t     target;
    void                *state;
    ku_builtin_match_t   match;
    ku_builtin_destroy_t destroy;
} ku_builtin_registration_t;

typedef ku_status_t (*ku_builtin_add_t)(void                            *context,
                                        const ku_builtin_registration_t *registration);

/* Borrowed registration callback view, valid only during one vendor builtin loader call. */
struct ku_builtin_builder_t {
    void            *context;
    ku_builtin_add_t add;
};

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_BUILTIN_H_
