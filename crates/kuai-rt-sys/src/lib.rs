#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

macro_rules! opaque_handle {
    ($raw:ident, $handle:ident) => {
        #[repr(C)]
        pub struct $raw {
            _private: [u8; 0],
        }
        pub type $handle = *mut $raw;
    };
}

macro_rules! define_ku_enum_value {
    (
        $(
            $name:ident = $value:expr
        ),* $(,)?
    ) => {
        $(
            #[allow(non_upper_case_globals)]
            pub const $name: ku_status_t = $value;
        )*
    };
}

pub type ku_status_t = i32;
define_ku_enum_value!(
    KU_STATUS_SUCCESS = 0,
    KU_STATUS_UNKNOWN = 1,
    KU_STATUS_RUNTIME_ERROR = 2,
    KU_STATUS_INVALID_ARGUMENT = 3,
    KU_STATUS_BUFFER_TOO_SMALL = 4,
    KU_STATUS_UNSUPPORTED_TYPE = 5,
    KU_STATUS_UNSUPPORTED_BUILTIN = 6,
    KU_STATUS_UNSUPPORTED_CAPABILITY = 7,
    KU_STATUS_CAPABILITY_MISMATCH = 8,
    KU_STATUS_NOT_READY = 9,
    KU_STATUS_ALREADY_INITIALIZED = 10,
);

opaque_handle!(_ku_engine, ku_engine_t);
opaque_handle!(_ku_device, ku_device_t);
opaque_handle!(_ku_completion, ku_completion_t);
opaque_handle!(_ku_stream, ku_stream_t);
opaque_handle!(_ku_event, ku_event_t);
opaque_handle!(_ku_memory_pool, ku_memory_pool_t);
opaque_handle!(_ku_tensor, ku_tensor_t);
opaque_handle!(_ku_objec_t, ku_object_t);
opaque_handle!(_ku_frame_ctx, ku_frame_ctx_t);
opaque_handle!(_ku_builtin_builder, ku_builtin_builder_t);
opaque_handle!(_ku_overload_set, ku_overload_set_t);

#[repr(C)]
pub struct ku_string_view_t {
    pub data: *const u8,
    pub size: usize,
}

pub type ku_device_type_t = i32;
define_ku_enum_value!(
    KU_DEVICE_TYPE_CPU = 0,
    KU_DEVICE_TYPE_CUDA = 1,
    KU_DEVICE_TYPE_CUDA_HOST = 3,
    KU_DEVICE_TYPE_CUDA_MANAGED = 4,
    KU_DEVICE_TYPE_CUDA_ROCM = 4,
    KU_DEVICE_TYPE_CUDA_EXT = 5,
);

pub type ku_memcpy_kind_t = i32;
define_ku_enum_value!(
    KU_MEMCPY_KIND_HOST_TO_HOST = 0,
    KU_MEMCPY_KIND_HOST_TO_DEVICE = 1,
    KU_MEMCPY_KIND_DEVICE_TO_HOST = 2,
    KU_MEMCPY_KIND_DEVICE_TO_DEVICE = 3,
    KU_MEMCPY_KIND_DEFAULT = 4,
);

pub type ku_device_id_t = i32;
pub type ku_size_t = usize;

#[repr(C)]
pub struct ku_device_info_t {
    device_type: ku_device_type_t,
    ku_device_id_t: ku_device_id_t,
}

#[repr(C)]
pub struct ku_system_api_t {
    pub ctx: *mut std::ffi::c_void,

    pub get_device_count:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, count: *mut i32) -> ku_status_t,

    pub get_device: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        device: *mut ku_device_id_t,
    ) -> ku_status_t,

    pub set_device:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, device: ku_device_id_t) -> ku_status_t,

    pub get_per_thread_stream: unsafe extern "C" fn(ctx: *mut std::ffi::c_void) -> ku_stream_t,

    pub stream_create:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, stream: *mut ku_stream_t) -> ku_status_t,

    pub stream_destroy:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, stream: ku_stream_t) -> ku_status_t,

    pub stream_query:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, stream: ku_stream_t) -> ku_status_t,

    pub stream_synchronize:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, stream: ku_stream_t) -> ku_status_t,

    pub malloc_device: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        ptr: *mut *mut std::ffi::c_void,
        bytes: usize,
    ) -> ku_status_t,

    pub free_device:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, ptr: *mut std::ffi::c_void) -> ku_status_t,

    pub memory_pool_create: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        device: ku_device_id_t,
        out: *mut ku_memory_pool_t,
    ) -> ku_status_t,

    pub memory_pool_destroy:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, pool: ku_memory_pool_t) -> ku_status_t,

    pub malloc_from_pool_async: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        ptr: *mut *mut std::ffi::c_void,
        bytes: usize,
        pool: ku_memory_pool_t,
        stream: ku_stream_t,
    ) -> ku_status_t,

    pub free_async: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        ptr: *mut std::ffi::c_void,
        stream: ku_stream_t,
    ) -> ku_status_t,

    pub malloc_host: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        ptr: *mut *mut std::ffi::c_void,
        bytes: usize,
    ) -> ku_status_t,

    pub free_host:
        unsafe extern "C" fn(ctx: *mut std::ffi::c_void, ptr: *mut std::ffi::c_void) -> ku_status_t,

    pub memcpy: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        dst: *mut std::ffi::c_void,
        src: *const std::ffi::c_void,
        bytes: usize,
        kind: ku_memcpy_kind_t,
    ) -> ku_status_t,

    pub memcpy_async: unsafe extern "C" fn(
        ctx: *mut std::ffi::c_void,
        dst: *mut std::ffi::c_void,
        src: *const std::ffi::c_void,
        bytes: usize,
        kind: ku_memcpy_kind_t,
        stream: ku_stream_t,
    ) -> ku_status_t,
}

pub type ku_system_builtin_loader_t =
    unsafe extern "C" fn(out: ku_builtin_builder_t) -> ku_status_t;

#[repr(C)]
pub struct ku_system_built_record_t {
    pub name: ku_string_view_t,
    pub loader: ku_system_builtin_loader_t,
}

#[repr(C)]
pub struct ku_system_module_t {
    device_type: ku_device_type_t,
    ku_system_api_t: ku_system_api_t,
    records_being: *const ku_system_built_record_t,
    records_end: *const ku_system_built_record_t,
}

pub type ku_task_fn_t = unsafe extern "C" fn(ctx: *mut std::ffi::c_void);

#[repr(C)]
pub struct ku_task_t {
    pub fn_: ku_task_fn_t,
    pub ctx: *mut std::ffi::c_void,
}

#[repr(C)]
pub struct ku_scheduler_t {
    pub ctx: *mut std::ffi::c_void,
    pub submit: unsafe extern "C" fn(ctx: *mut std::ffi::c_void, task: ku_task_t) -> ku_status_t,
}

unsafe extern "C" {
    pub fn ku_status_string(status: i32) -> *const std::ffi::c_char;

    pub fn ku_get_default_device(engine: ku_engine_t, device: *mut ku_device_t) -> ku_status_t;

    pub fn ku_get_device(
        engine: ku_engine_t,
        device_id: ku_device_id_t,
        device: *mut ku_device_t,
    ) -> ku_status_t;

    pub fn ku_get_device_info(device: ku_device_t, info: *mut ku_device_info_t) -> ku_status_t;

    pub fn kuSystemModule() -> ku_system_module_t;

}
