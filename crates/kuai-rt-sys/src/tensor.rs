use std::ffi::c_void;

use crate::dlpack::DLManagedTensorVersioned;
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

unsafe extern "C" {
    /// Creates a contiguous column-major tensor with one owned object reference.
    pub fn ku_tensor_create(
        desc: *const ku_tensor_create_desc_t,
        out: *mut ku_object_t,
    ) -> ku_status_t;
    pub fn ku_tensor_get_size(tensor: ku_object_t, out: *mut ku_size_t) -> ku_status_t;
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
    /// Creates an owning export. Invoke the exported deleter exactly once.
    pub fn ku_tensor_to_dlpack(
        tensor: ku_object_t,
        out: *mut *mut DLManagedTensorVersioned,
    ) -> ku_status_t;
    /// Consumes `dlpack` only on success, returning one owned tensor reference.
    /// The descriptor must identify `device`. Ownership remains with the caller on failure.
    pub fn ku_tensor_from_dlpack(
        device: ku_device_t,
        dlpack: *mut DLManagedTensorVersioned,
        out: *mut ku_object_t,
    ) -> ku_status_t;
}
