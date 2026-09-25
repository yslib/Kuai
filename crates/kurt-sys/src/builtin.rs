use std::ffi::c_void;

use crate::types::*;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_builtin_info_t {
    pub name: ku_string_view_t,
}

/// Borrowed arguments and writable result storage for one validated call.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_frame_t {
    /// May be null only when the callable does not require a context.
    pub ctx: ku_frame_ctx_t,
    /// Non-null array of `pargn + kargn` nullable objects, positional first.
    pub argv: *const ku_object_t,
    pub pargn: ku_size_t,
    /// May be null only when `kargn == 0`.
    pub knames: *const ku_string_view_t,
    pub kargn: ku_size_t,
    /// Non-null writable array; non-null returned objects carry owned references.
    pub results: *mut ku_object_t,
    /// Must be greater than zero.
    pub result_capacity: ku_size_t,
    /// Actual count on success, required count if too small, otherwise zero.
    pub result_count: ku_size_t,
}

pub type ku_ffi_t = unsafe extern "C" fn(frame: *mut ku_frame_t) -> ku_status_t;
pub type ku_closure_ffi_t =
    unsafe extern "C" fn(capture: *mut c_void, frame: *mut ku_frame_t) -> ku_status_t;

/// Non-owning callable; its producer must keep the capture alive during use.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_closure_t {
    pub capture: *mut c_void,
    pub ffi: ku_closure_ffi_t,
}

c_enum! {
    ku_call_kind_t: i32 {
        KU_CALL_FFI = 1,
        KU_CALL_CALLABLE = 2,
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub union ku_call_value_t {
    pub ffi: ku_ffi_t,
    pub closure: ku_closure_t,
}

/// Only access the union member selected by `kind`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct ku_call_t {
    pub kind: ku_call_kind_t,
    pub value: ku_call_value_t,
}

unsafe extern "C" {
    /// Enumerates borrowed builtin names. `out` may be null only at zero capacity.
    /// On `KU_STATUS_BUFFER_TOO_SMALL`, writes the required count and leaves `out` untouched.
    pub fn ku_instance_get_builtin_info(
        instance: ku_instance_t,
        out: *mut ku_builtin_info_t,
        capacity: ku_size_t,
        out_count: *mut ku_size_t,
    ) -> ku_status_t;
    /// On success writes an invocable target borrowed from the instance.
    pub fn ku_instance_get_proc_address(
        instance: ku_instance_t,
        name: ku_string_view_t,
        out: *mut ku_call_t,
    ) -> ku_status_t;
}
