#ifndef KURT_C_KU_TYPES_H_
#define KURT_C_KU_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KU_STATUS_DEFS(X)                                \
    X(SUCCESS, 0, "success")                             \
    X(NOT_READY, 1, "not ready")                         \
    X(BUFFER_TOO_SMALL, 2, "buffer too small")           \
    X(INVALID_ARGUMENT, 100, "invalid argument")         \
    X(OUT_OF_RANGE, 101, "out of range")                 \
    X(TYPE_MISMATCH, 102, "type mismatch")               \
    X(INVALID_STATE, 103, "invalid state")               \
    X(NOT_FOUND, 104, "not found")                       \
    X(ALREADY_INITIALIZED, 105, "already initialized")   \
    X(NOT_SUPPORTED, 106, "not supported")               \
    X(OUT_OF_HOST_MEMORY, 200, "out of host memory")     \
    X(OUT_OF_DEVICE_MEMORY, 201, "out of device memory") \
    X(BACKEND_UNAVAILABLE, 300, "backend unavailable")   \
    X(DEVICE_UNAVAILABLE, 301, "device unavailable")     \
    X(DEVICE_LOST, 302, "device lost")                   \
    X(DEVICE_ERROR, 303, "device error")                 \
    X(BUILTIN_ERROR, 400, "builtin error")               \
    X(INTERNAL_ERROR, 900, "internal error")

typedef struct _ku_instance    *ku_instance_t;
typedef struct _ku_device      *ku_device_t;
typedef struct _ku_completion  *ku_completion_t;
typedef struct _ku_host        *ku_host_t;
typedef struct _ku_stream      *ku_stream_t;
typedef struct _ku_event       *ku_event_t;
typedef struct _ku_memory_pool *ku_memory_pool_t;
typedef struct _ku_object      *ku_object_t;
typedef struct _ku_frame_ctx   *ku_frame_ctx_t;

typedef size_t  ku_size_t;
typedef int32_t ku_status_t;

enum {
#define X(name, value, ...) KU_STATUS_##name = value,
    KU_STATUS_DEFS(X)
#undef X
};

typedef struct ku_string_view_t {
    const char *data;
    ku_size_t   size;
} ku_string_view_t;

const char *ku_status_string(ku_status_t status);

typedef int32_t ku_primitive_type_t;

typedef int8_t  ku_char8_t;
typedef int16_t ku_i16_t;
typedef int32_t ku_i32_t;
typedef int64_t ku_i64_t;
typedef float   ku_f32_t;
typedef double  ku_f64_t;

enum {
    KU_BOOL_NULL = INT8_MIN,
    KU_BOOL_FALSE = 0,
    KU_BOOL_TRUE = 1
};

typedef struct ku_void {
    uint8_t reserved;
} ku_void_t;

typedef struct ku_bool {
    int8_t value;
} ku_bool_t;

#define KU_PRIMITIVE_TYPE_DEFS(X)                \
    X(NONE, 0, "none", ku_void_t, none)          \
    X(BOOLEAN, 1, "boolean", ku_bool_t, boolean) \
    X(BYTE, 2, "byte", ku_char8_t, ch)           \
    X(I16, 3, "i16", ku_i16_t, i16)              \
    X(I32, 4, "i32", ku_i32_t, i32)              \
    X(I64, 5, "i64", ku_i64_t, i64)              \
    X(F32, 6, "f32", ku_f32_t, f32)              \
    X(F64, 7, "f64", ku_f64_t, f64)

enum {
#define X(name, value, ...) KU_PRIMITIVE_##name = value,
    KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
};

typedef struct ku_union {
    union {
#define X(name, value, display_name, payload_type, field) payload_type field;
        KU_PRIMITIVE_TYPE_DEFS(X)
#undef X
    };
    ku_primitive_type_t tag;
} ku_union_t;

#ifdef __cplusplus
}
#endif

#endif // KURT_C_KU_TYPES_H_
