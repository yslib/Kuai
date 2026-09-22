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

unsafe extern "C" {
    /// Acquires an additional intrusive reference to a live object.
    pub fn ku_object_retain(object: ku_object_t) -> ku_status_t;
    /// Releases one owned reference; the handle may become invalid.
    pub fn ku_object_release(object: ku_object_t) -> ku_status_t;
    /// Writes the stable materializable value kind.
    pub fn ku_object_get_kind(object: ku_object_t, out: *mut ku_object_kind_t) -> ku_status_t;
    /// Copies a tagged primitive into a new scalar with one owned reference.
    pub fn ku_scalar_create(value: *const ku_union_t, out: *mut ku_object_t) -> ku_status_t;
    /// Copies the scalar payload and its tag into `out`.
    pub fn ku_scalar_get_value(scalar: ku_object_t, out: *mut ku_union_t) -> ku_status_t;
    /// Copies the bytes into a new immutable string with one owned reference.
    pub fn ku_string_create(value: ku_string_view_t, out: *mut ku_object_t) -> ku_status_t;
    /// Returns a borrowed view, valid only while the string remains alive.
    pub fn ku_string_get_value(string: ku_object_t, out: *mut ku_string_view_t) -> ku_status_t;
    /// Retains each borrowed element and returns one owned array reference.
    /// `items` must be non-null even when `count` is zero; elements must be non-null.
    pub fn ku_array_create(
        items: *const ku_object_t,
        count: ku_size_t,
        out: *mut ku_object_t,
    ) -> ku_status_t;
    pub fn ku_array_get_size(array: ku_object_t, out: *mut ku_size_t) -> ku_status_t;
    /// Returns one owned reference to the element at `index`.
    pub fn ku_array_get(array: ku_object_t, index: ku_size_t, out: *mut ku_object_t)
    -> ku_status_t;
    /// Copies the descriptor into a new immutable slice with one owned reference.
    pub fn ku_slice_create(desc: *const ku_slice_desc_t, out: *mut ku_object_t) -> ku_status_t;
    pub fn ku_slice_get_value(slice: ku_object_t, out: *mut ku_slice_desc_t) -> ku_status_t;
}
