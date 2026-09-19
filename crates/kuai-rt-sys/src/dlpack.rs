//! DLPack types matching the header bundled with `kuai-runtime` (version 1.3).
//!
//! These are non-owning ABI representations. Copying a managed descriptor does
//! not duplicate ownership. Only invoke its deleter once, on the original pointer.
#![allow(non_upper_case_globals)]

use std::ffi::{c_char, c_int, c_void};

pub const DLPACK_MAJOR_VERSION: u32 = 1;
pub const DLPACK_MINOR_VERSION: u32 = 3;
pub const DLPACK_FLAG_BITMASK_READ_ONLY: u64 = 1 << 0;
pub const DLPACK_FLAG_BITMASK_IS_COPIED: u64 = 1 << 1;
pub const DLPACK_FLAG_BITMASK_IS_SUBBYTE_TYPE_PADDED: u64 = 1 << 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DLPackVersion {
    pub major: u32,
    pub minor: u32,
}

c_enum! {
    DLDeviceType: i32 {
        kDLCPU = 1,
        kDLCUDA = 2,
        kDLCUDAHost = 3,
        kDLOpenCL = 4,
        kDLVulkan = 7,
        kDLMetal = 8,
        kDLVPI = 9,
        kDLROCM = 10,
        kDLROCMHost = 11,
        kDLExtDev = 12,
        kDLCUDAManaged = 13,
        kDLOneAPI = 14,
        kDLWebGPU = 15,
        kDLHexagon = 16,
        kDLMAIA = 17,
        kDLTrn = 18,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DLDevice {
    pub device_type: DLDeviceType,
    pub device_id: i32,
}

c_enum! {
    DLDataTypeCode: std::ffi::c_uint {
        kDLInt = 0,
        kDLUInt = 1,
        kDLFloat = 2,
        kDLOpaqueHandle = 3,
        kDLBfloat = 4,
        kDLComplex = 5,
        kDLBool = 6,
        kDLFloat8_e3m4 = 7,
        kDLFloat8_e4m3 = 8,
        kDLFloat8_e4m3b11fnuz = 9,
        kDLFloat8_e4m3fn = 10,
        kDLFloat8_e4m3fnuz = 11,
        kDLFloat8_e5m2 = 12,
        kDLFloat8_e5m2fnuz = 13,
        kDLFloat8_e8m0fnu = 14,
        kDLFloat6_e2m3fn = 15,
        kDLFloat6_e3m2fn = 16,
        kDLFloat4_e2m1fn = 17,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DLDataType {
    pub code: u8,
    pub bits: u8,
    pub lanes: u16,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct DLTensor {
    pub data: *mut c_void,
    pub device: DLDevice,
    pub ndim: i32,
    pub dtype: DLDataType,
    pub shape: *mut i64,
    pub strides: *mut i64,
    pub byte_offset: u64,
}

pub type DLManagedTensorDeleter = unsafe extern "C" fn(tensor: *mut DLManagedTensor);
pub type DLManagedTensorVersionedDeleter =
    unsafe extern "C" fn(tensor: *mut DLManagedTensorVersioned);

/// Legacy DLPack descriptor; Kuai's tensor API uses `DLManagedTensorVersioned`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct DLManagedTensor {
    pub dl_tensor: DLTensor,
    pub manager_ctx: *mut c_void,
    /// Nullable code pointer with signature `DLManagedTensorDeleter`.
    pub deleter: *const c_void,
}

/// Check `version.major` before accessing fields other than the stable
/// version/context/deleter prefix. On incompatibility, invoke the deleter.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct DLManagedTensorVersioned {
    pub version: DLPackVersion,
    pub manager_ctx: *mut c_void,
    /// Nullable code pointer with signature `DLManagedTensorVersionedDeleter`.
    pub deleter: *const c_void,
    pub flags: u64,
    pub dl_tensor: DLTensor,
}

pub type DLPackManagedTensorAllocator = unsafe extern "C" fn(
    prototype: *mut DLTensor,
    out: *mut *mut DLManagedTensorVersioned,
    error_ctx: *mut c_void,
    set_error: unsafe extern "C" fn(
        error_ctx: *mut c_void,
        kind: *const c_char,
        message: *const c_char,
    ),
) -> c_int;

pub type DLPackManagedTensorFromPyObjectNoSync =
    unsafe extern "C" fn(py_object: *mut c_void, out: *mut *mut DLManagedTensorVersioned) -> c_int;
/// Nullable code pointer for the optional borrowed-tensor export operation.
pub type DLPackDLTensorFromPyObjectNoSync = *const c_void;
/// Signature of a non-null `DLPackDLTensorFromPyObjectNoSync` callback.
pub type DLPackDLTensorFromPyObjectNoSyncFn =
    unsafe extern "C" fn(py_object: *mut c_void, out: *mut DLTensor) -> c_int;
pub type DLPackCurrentWorkStream = unsafe extern "C" fn(
    device_type: DLDeviceType,
    device_id: i32,
    out_current_stream: *mut *mut c_void,
) -> c_int;
pub type DLPackManagedTensorToPyObjectNoSync = unsafe extern "C" fn(
    tensor: *mut DLManagedTensorVersioned,
    out_py_object: *mut *mut c_void,
) -> c_int;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct DLPackExchangeAPIHeader {
    pub version: DLPackVersion,
    pub prev_api: *mut DLPackExchangeAPIHeader,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct DLPackExchangeAPI {
    pub header: DLPackExchangeAPIHeader,
    pub managed_tensor_allocator: DLPackManagedTensorAllocator,
    pub managed_tensor_from_py_object_no_sync: DLPackManagedTensorFromPyObjectNoSync,
    pub managed_tensor_to_py_object_no_sync: DLPackManagedTensorToPyObjectNoSync,
    pub dltensor_from_py_object_no_sync: DLPackDLTensorFromPyObjectNoSync,
    pub current_work_stream: DLPackCurrentWorkStream,
}
