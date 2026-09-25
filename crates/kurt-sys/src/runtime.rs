use std::ffi::{c_int, c_void};

use crate::builtin::{ku_builtin_info_t, ku_call_t};
use crate::types::*;

pub type ku_device_id_t = i32;

c_enum! {
    ku_device_type_t: i32 {
        KU_DEVICE_CPU = 1,
        KU_DEVICE_CUDA = 2,
        KU_DEVICE_CUDA_HOST = 3,
        KU_DEVICE_CUDA_MANAGED = 4,
        KU_DEVICE_EXT = 12,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ku_device_info_t {
    pub device_type: ku_device_type_t,
    pub device_id: ku_device_id_t,
}

c_enum! {
    ku_memcpy_kind_t: i32 {
        KU_MEMCPY_HOST_TO_DEVICE = 1,
        KU_MEMCPY_DEVICE_TO_HOST = 2,
        KU_MEMCPY_DEVICE_TO_DEVICE = 3,
    }
}

/// Borrowed backend table. Installed tables require every function to be non-null.
/// The caller must select the intended device before invoking device-bound operations.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_vendor_api_t {
    pub ctx: *mut c_void,
    pub get_device_count: unsafe extern "C" fn(ctx: *mut c_void, count: *mut c_int) -> ku_status_t,
    pub get_device:
        unsafe extern "C" fn(ctx: *mut c_void, device: *mut ku_device_id_t) -> ku_status_t,
    pub set_device: unsafe extern "C" fn(ctx: *mut c_void, device: ku_device_id_t) -> ku_status_t,
    pub get_per_thread_stream: unsafe extern "C" fn(ctx: *mut c_void) -> ku_stream_t,
    pub stream_create:
        unsafe extern "C" fn(ctx: *mut c_void, stream: *mut ku_stream_t) -> ku_status_t,
    pub stream_destroy: unsafe extern "C" fn(ctx: *mut c_void, stream: ku_stream_t) -> ku_status_t,
    pub stream_query: unsafe extern "C" fn(ctx: *mut c_void, stream: ku_stream_t) -> ku_status_t,
    pub stream_synchronize:
        unsafe extern "C" fn(ctx: *mut c_void, stream: ku_stream_t) -> ku_status_t,
    pub malloc_device: unsafe extern "C" fn(
        ctx: *mut c_void,
        ptr: *mut *mut c_void,
        bytes: ku_size_t,
    ) -> ku_status_t,
    pub free_device: unsafe extern "C" fn(ctx: *mut c_void, ptr: *mut c_void) -> ku_status_t,
    pub memory_pool_create: unsafe extern "C" fn(
        ctx: *mut c_void,
        device: ku_device_id_t,
        out: *mut ku_memory_pool_t,
    ) -> ku_status_t,
    pub memory_pool_destroy:
        unsafe extern "C" fn(ctx: *mut c_void, pool: ku_memory_pool_t) -> ku_status_t,
    pub malloc_from_pool_async: unsafe extern "C" fn(
        ctx: *mut c_void,
        ptr: *mut *mut c_void,
        bytes: ku_size_t,
        pool: ku_memory_pool_t,
        stream: ku_stream_t,
    ) -> ku_status_t,
    pub free_async: unsafe extern "C" fn(
        ctx: *mut c_void,
        ptr: *mut c_void,
        stream: ku_stream_t,
    ) -> ku_status_t,
    pub malloc_host: unsafe extern "C" fn(
        ctx: *mut c_void,
        ptr: *mut *mut c_void,
        bytes: ku_size_t,
    ) -> ku_status_t,
    pub free_host: unsafe extern "C" fn(ctx: *mut c_void, ptr: *mut c_void) -> ku_status_t,
    pub memcpy: unsafe extern "C" fn(
        ctx: *mut c_void,
        dst: *mut c_void,
        src: *const c_void,
        bytes: ku_size_t,
        kind: ku_memcpy_kind_t,
    ) -> ku_status_t,
    pub memcpy_async: unsafe extern "C" fn(
        ctx: *mut c_void,
        dst: *mut c_void,
        src: *const c_void,
        bytes: ku_size_t,
        kind: ku_memcpy_kind_t,
        stream: ku_stream_t,
    ) -> ku_status_t,
}

/// Borrowed module descriptor; every reachable pointer has module lifetime.
/// Builtin captures persist across host instance recreation.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_vendor_module_t {
    pub device_type: ku_device_type_t,
    pub vendor_api: ku_vendor_api_t,
    pub get_builtin_info: unsafe extern "C" fn(
        out: *mut ku_builtin_info_t,
        capacity: ku_size_t,
        out_count: *mut ku_size_t,
    ) -> ku_status_t,
    pub get_proc_address:
        unsafe extern "C" fn(name: ku_string_view_t, out: *mut ku_call_t) -> ku_status_t,
}

pub type ku_task_fn_t = unsafe extern "C" fn(task_ctx: *mut c_void);

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_task_t {
    pub run: ku_task_fn_t,
    pub ctx: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_scheduler_t {
    pub ctx: *mut c_void,
    /// Null selects the default scheduler; otherwise points to `ku_scheduler_submit_t` code.
    pub submit: *const c_void,
}

/// Signature of a non-null `ku_scheduler_t.submit` callback.
pub type ku_scheduler_submit_t =
    unsafe extern "C" fn(ctx: *mut c_void, task: ku_task_t) -> ku_status_t;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ku_device_stream_capabilities_t {
    pub max_compute_streams: u32,
    pub max_copy_streams: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ku_device_capabilities_t {
    pub device_id: ku_device_id_t,
    pub streams: ku_device_stream_capabilities_t,
    pub max_memory_bytes: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_instance_capabilities_t {
    pub scheduler: ku_scheduler_t,
    pub max_worker_concurrency: u32,
    pub max_host_memory_bytes: u64,
    pub devices: *const ku_device_capabilities_t,
    pub device_count: ku_size_t,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_instance_init_info_t {
    pub vendor: ku_string_view_t,
    pub default_device_id: ku_device_id_t,
    pub capabilities: ku_instance_capabilities_t,
}

/// May run on a worker thread, or synchronously when registered after completion.
/// The callback must return quickly and may still be running after a wait returns.
pub type ku_completion_callback_t =
    unsafe extern "C" fn(user_data: *mut c_void, status: ku_status_t);

unsafe extern "C" {
    /// Creates a uniquely owned instance. Only one may exist per lower-case vendor tag.
    /// Any custom scheduler and its context must remain valid until destruction.
    pub fn ku_instance_init(
        info: *const ku_instance_init_info_t,
        out: *mut ku_instance_t,
    ) -> ku_status_t;
    pub fn ku_instance_flush(instance: ku_instance_t) -> ku_status_t;
    /// Always invalidates the instance and derived handles, even on an error status.
    pub fn ku_instance_destroy(instance: ku_instance_t) -> ku_status_t;
    /// Returns a device borrowed from `instance`.
    pub fn ku_instance_get_default_device(
        instance: ku_instance_t,
        out: *mut ku_device_t,
    ) -> ku_status_t;
    /// Returns a device borrowed from `instance`, or `KU_STATUS_OUT_OF_RANGE`.
    pub fn ku_instance_get_device(
        instance: ku_instance_t,
        device_id: ku_device_id_t,
        out: *mut ku_device_t,
    ) -> ku_status_t;
    /// The returned descriptor's pointers borrow from the instance.
    pub fn ku_instance_get_capabilities(
        instance: ku_instance_t,
        out: *mut ku_instance_capabilities_t,
    ) -> ku_status_t;
    pub fn ku_device_get_info(device: ku_device_t, out: *mut ku_device_info_t) -> ku_status_t;
    /// Returns the device's exact owning instance as a borrowed, stable handle.
    /// The query does not retain, allocate, or synchronize.
    ///
    /// # Safety
    /// `device` must be valid and non-null; `out` must be valid, writable, and
    /// non-null. The returned handle remains valid until its instance is destroyed.
    /// Do not destroy the borrowed handle or use it after instance destruction.
    pub fn ku_device_get_instance(device: ku_device_t, out: *mut ku_instance_t) -> ku_status_t;
    pub fn ku_device_get_capabilities(
        device: ku_device_t,
        out: *mut ku_device_capabilities_t,
    ) -> ku_status_t;
    /// Returns a backend table borrowed from the device's instance.
    pub fn ku_device_get_vendor_api(
        device: ku_device_t,
        out: *mut *const ku_vendor_api_t,
    ) -> ku_status_t;
    pub fn ku_device_get_default_stream(device: ku_device_t, out: *mut ku_stream_t) -> ku_status_t;
    pub fn ku_device_synchronize(device: ku_device_t, stream: ku_stream_t) -> ku_status_t;
    pub fn ku_device_flush(device: ku_device_t) -> ku_status_t;
    /// Returns an owned completion on success and null on failure.
    /// The buffers must remain valid and free of conflicting accesses until completion.
    pub fn ku_device_copy_async(
        device: ku_device_t,
        dst: *mut c_void,
        src: *const c_void,
        bytes: ku_size_t,
        kind: ku_memcpy_kind_t,
        dependency_stream: ku_stream_t,
        out: *mut ku_completion_t,
    ) -> ku_status_t;

    /// Vendor module entry point, exported by `libkurt_<vendor>`, not `libkurt`.
    /// `host` must be a borrowed, non-null host handle with process lifetime.
    #[allow(non_snake_case)]
    pub fn kuVendorModule(host: ku_host_t) -> *const ku_vendor_module_t;

    /// Creates an owned context borrowing `device`; destroy it before the instance.
    pub fn ku_frame_ctx_create(device: ku_device_t, out_ctx: *mut ku_frame_ctx_t) -> ku_status_t;
    pub fn ku_frame_ctx_get_device(
        ctx: ku_frame_ctx_t,
        out_device: *mut ku_device_t,
    ) -> ku_status_t;
    pub fn ku_frame_ctx_destroy(ctx: ku_frame_ctx_t);
    pub fn ku_completion_retain(completion: ku_completion_t) -> ku_status_t;
    pub fn ku_completion_release(completion: ku_completion_t) -> ku_status_t;
    /// Waits for the operation's final status, not for registered callbacks to return.
    pub fn ku_completion_wait(completion: ku_completion_t) -> ku_status_t;
    /// Registers the single non-null callback. `user_data` must remain valid until it returns.
    pub fn ku_completion_on_completion(
        completion: ku_completion_t,
        callback: ku_completion_callback_t,
        user_data: *mut c_void,
    ) -> ku_status_t;
}
