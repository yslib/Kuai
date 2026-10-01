use std::ffi::c_void;

use crate::types::*;

c_enum! {
    ku_object_kind_t: i32 {
        KU_OBJECT_SCALAR = 1,
        KU_OBJECT_TENSOR = 2,
        KU_OBJECT_ARRAY = 3,
        KU_OBJECT_STRING = 4,
        KU_OBJECT_SLICE = 5,
    }
}

c_enum! {
    ku_slice_flags_t: u32 {
        KU_SLICE_HAS_START = 1 << 0,
        KU_SLICE_HAS_STOP = 1 << 1,
        KU_SLICE_HAS_STEP = 1 << 2,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ku_slice_desc_t {
    pub start: i64,
    pub stop: i64,
    pub step: i64,
    pub flags: ku_slice_flags_t,
}

/// Creation retains each borrowed element. `items` must be non-null, aligned,
/// and readable for `count` handles, even when empty. Non-null elements must be
/// live; null elements return `KU_STATUS_INVALID_ARGUMENT`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_array_create_desc_t {
    pub items: *const ku_object_t,
    pub count: ku_size_t,
}

unsafe extern "C" {
    /// Creates a fully constructed value with one owned reference.
    ///
    /// Descriptor types by kind: scalar = `ku_union_t`, string = `ku_string_view_t`,
    /// slice = `ku_slice_desc_t`, array = `ku_array_create_desc_t`, and tensor =
    /// `ku_tensor_create_desc_t`. Descriptors are borrowed for the call; each
    /// type copies or retains the data it needs. No partial object is published.
    /// Unsupported kinds return `KU_STATUS_NOT_SUPPORTED` without reading the
    /// descriptor. Every returned failure clears `*out` to null.
    ///
    /// # Safety
    /// `desc` and `out` must be valid and non-null; `out` must be writable.
    /// For a supported kind, `desc` must point to a live, aligned descriptor of
    /// the matching type. Scalar payloads must match their tag; string data must
    /// be non-null and readable for its size, including when empty. Other fields
    /// follow the contracts documented on their descriptor types.
    /// Descriptor types are trusted, not checked at runtime.
    pub fn ku_object_create(
        kind: ku_object_kind_t,
        desc: *const c_void,
        out: *mut ku_object_t,
    ) -> ku_status_t;
    /// Acquires an additional intrusive reference to a live object.
    pub fn ku_object_retain(object: ku_object_t) -> ku_status_t;
    /// Releases one owned reference; the handle may become invalid.
    pub fn ku_object_release(object: ku_object_t) -> ku_status_t;
    /// Writes the stable materializable value kind.
    pub fn ku_object_get_kind(object: ku_object_t, out: *mut ku_object_kind_t) -> ku_status_t;
    /// Copies the scalar payload and its tag into `out`.
    pub fn ku_scalar_get_value(scalar: ku_object_t, out: *mut ku_union_t) -> ku_status_t;
    /// Returns a borrowed view, valid only while the string remains alive.
    pub fn ku_string_get_value(string: ku_object_t, out: *mut ku_string_view_t) -> ku_status_t;
    pub fn ku_array_get_size(array: ku_object_t, out: *mut ku_size_t) -> ku_status_t;
    /// Returns one owned reference to the element at `index`.
    pub fn ku_array_get(array: ku_object_t, index: ku_size_t, out: *mut ku_object_t)
    -> ku_status_t;
    pub fn ku_slice_get_value(slice: ku_object_t, out: *mut ku_slice_desc_t) -> ku_status_t;
}
