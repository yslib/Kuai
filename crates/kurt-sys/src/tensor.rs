use std::ffi::c_void;

use crate::types::*;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_tensor_create_desc_t {
    pub device: ku_device_t,
    pub primitive_type: ku_primitive_type_t,
    /// Must be in 0..=8.
    pub ndim: i32,
    /// May be null only for rank zero.
    pub shape: *const i64,
    /// Null for canonical layout, otherwise canonical column-major element strides.
    pub strides: *const i64,
}

/// A borrowed view of a tensor's native type and actual layout.
///
/// For positive rank, `shape` and `strides` point to immutable arrays of `ndim`
/// entries in logical dimension order. Strides are in elements, not bytes.
/// Rank zero has null arrays and one element. Positive-rank empty tensors keep
/// their actual shape and strides, including zero extents and any zero strides.
/// The logical element count is the shape product, starting with one.
///
/// Copying this descriptor neither copies its arrays nor retains the tensor.
/// Keep an owned reference to the same native tensor and its instance alive
/// while using the arrays, which are stable for the tensor's lifetime. Copy
/// the arrays for a detached snapshot. Handle rank zero without passing its
/// null pointers to `slice::from_raw_parts`, even with a zero length.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ku_tensor_info_t {
    pub primitive_type: ku_primitive_type_t,
    pub ndim: i32,
    pub shape: *const ku_size_t,
    pub strides: *const ku_size_t,
}

unsafe extern "C" {
    /// Creates a contiguous column-major tensor with one owned object reference.
    pub fn ku_tensor_create(
        desc: *const ku_tensor_create_desc_t,
        out: *mut ku_object_t,
    ) -> ku_status_t;
    /// Returns `KU_STATUS_SUCCESS` and the tensor's native type and actual layout.
    /// A non-tensor returns `KU_STATUS_TYPE_MISMATCH` and resets `primitive_type`
    /// to `KU_PRIMITIVE_NONE`, `ndim` to zero, and both pointers to null. Those
    /// reset fields are not successful metadata. Only these two statuses are
    /// possible for a valid object.
    ///
    /// Direct arguments must be valid and non-null; `out` must be writable.
    /// See [`ku_tensor_info_t`] for borrowed-array lifetime requirements.
    /// Does not allocate, retain, release, synchronize, or read payload data.
    /// Upload readiness is unchanged: do not consume or observe the tensor
    /// until its upload completion reports success.
    pub fn ku_tensor_get_info(tensor: ku_object_t, out: *mut ku_tensor_info_t) -> ku_status_t;
    /// Returns `KU_STATUS_SUCCESS` and the borrowed logical first-element address,
    /// which need not be the allocation base. Empty tensors return null;
    /// nonempty tensors return a non-null address. A non-tensor returns
    /// `KU_STATUS_TYPE_MISMATCH` and clears `*out` to null, so check the status.
    /// Only these two statuses are possible for a valid object.
    ///
    /// The address belongs to the device returned by [`ku_tensor_get_device`].
    /// It is not a host mapping or download; host code must not dereference a
    /// CUDA device address. The mutable raw pointer grants neither exclusive
    /// access nor writable ownership. Obey storage permissions, typed layout,
    /// aliasing, and synchronization requirements. Do not free it, infer its
    /// allocator, or assume logical size exposes extra allocation capacity.
    ///
    /// Keep an owned reference to the same native tensor and its instance alive
    /// throughout every operation using the address, including pending async
    /// copies. Retained handles share storage; this creates no copy or lock.
    /// Does not allocate, retain, release, synchronize, select a device, or read
    /// payload data. The address does not establish readiness: wait for successful
    /// upload completion and use existing completion/device APIs for ordering and
    /// `ku_device_copy_async` for host downloads.
    /// Direct arguments must be valid and non-null; `out` must be writable.
    pub fn ku_tensor_get_data(tensor: ku_object_t, out: *mut *mut c_void) -> ku_status_t;
    /// Returns the tensor's exact device handle, borrowed from its instance.
    /// Do not release the device. It remains valid after tensor destruction until
    /// instance destruction; tensors must still be released before their instance.
    /// Does not retain, allocate, or synchronize. A non-tensor returns
    /// `KU_STATUS_TYPE_MISMATCH` and clears `*out` to null.
    /// Direct arguments must be valid and non-null; `out` must be writable.
    pub fn ku_tensor_get_device(tensor: ku_object_t, out: *mut ku_device_t) -> ku_status_t;
    /// Copies exactly the tensor's logical byte size from borrowed host storage.
    /// On success, both outputs own one reference; on failure, both are null.
    /// Keep `src` valid and unchanged until completion; do not use the tensor
    /// until the completion reports success. Direct pointers must be non-null,
    /// even for an empty tensor.
    pub fn ku_tensor_create_from_host_async(
        desc: *const ku_tensor_create_desc_t,
        src: *const c_void,
        bytes: ku_size_t,
        out_tensor: *mut ku_object_t,
        out_completion: *mut ku_completion_t,
    ) -> ku_status_t;
}
