use std::ffi::c_char;

opaque_handles! {
    _ku_instance => ku_instance_t,
    _ku_device => ku_device_t,
    _ku_completion => ku_completion_t,
    _ku_host => ku_host_t,
    _ku_stream => ku_stream_t,
    _ku_event => ku_event_t,
    _ku_memory_pool => ku_memory_pool_t,
    _ku_object => ku_object_t,
    _ku_frame_ctx => ku_frame_ctx_t,
}

pub type ku_size_t = usize;

c_enum! {
    ku_status_t: i32 {
        KU_STATUS_SUCCESS = 0,
        KU_STATUS_NOT_READY = 1,
        KU_STATUS_BUFFER_TOO_SMALL = 2,
        KU_STATUS_INVALID_ARGUMENT = 100,
        KU_STATUS_OUT_OF_RANGE = 101,
        KU_STATUS_TYPE_MISMATCH = 102,
        KU_STATUS_INVALID_STATE = 103,
        KU_STATUS_NOT_FOUND = 104,
        KU_STATUS_ALREADY_INITIALIZED = 105,
        KU_STATUS_NOT_SUPPORTED = 106,
        KU_STATUS_OUT_OF_HOST_MEMORY = 200,
        KU_STATUS_OUT_OF_DEVICE_MEMORY = 201,
        KU_STATUS_BACKEND_UNAVAILABLE = 300,
        KU_STATUS_DEVICE_UNAVAILABLE = 301,
        KU_STATUS_DEVICE_LOST = 302,
        KU_STATUS_DEVICE_ERROR = 303,
        KU_STATUS_BUILTIN_ERROR = 400,
        KU_STATUS_INTERNAL_ERROR = 900,
    }
}

/// Borrowed bytes; the data need not be UTF-8 or nul-terminated.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_string_view_t {
    pub data: *const c_char,
    pub size: ku_size_t,
}

pub type ku_char8_t = i8;
pub type ku_i16_t = i16;
pub type ku_i32_t = i32;
pub type ku_i64_t = i64;
pub type ku_f32_t = f32;
pub type ku_f64_t = f64;

pub const KU_BOOL_NULL: i8 = i8::MIN;
pub const KU_BOOL_FALSE: i8 = 0;
pub const KU_BOOL_TRUE: i8 = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ku_void_t {
    pub reserved: u8,
}

/// Three-state runtime boolean, distinct from Rust's `bool`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ku_bool_t {
    pub value: i8,
}

primitive_types! {
    KU_PRIMITIVE_NONE = 0 => none: ku_void_t,
    KU_PRIMITIVE_BOOLEAN = 1 => boolean: ku_bool_t,
    KU_PRIMITIVE_BYTE = 2 => ch: ku_char8_t,
    KU_PRIMITIVE_I16 = 3 => i16: ku_i16_t,
    KU_PRIMITIVE_I32 = 4 => i32: ku_i32_t,
    KU_PRIMITIVE_I64 = 5 => i64: ku_i64_t,
    KU_PRIMITIVE_F32 = 6 => f32: ku_f32_t,
    KU_PRIMITIVE_F64 = 7 => f64: ku_f64_t,
}

/// Tagged primitive. Access only the payload field selected by `tag`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ku_union_t {
    pub value: ku_union_value_t,
    pub tag: ku_primitive_type_t,
}

unsafe extern "C" {
    /// Returns a borrowed, static, nul-terminated status description.
    pub fn ku_status_string(status: ku_status_t) -> *const c_char;
}
