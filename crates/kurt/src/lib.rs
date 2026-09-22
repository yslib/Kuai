#![doc = include_str!("../README.md")]
#![deny(unsafe_op_in_unsafe_fn)]

mod error;
mod ku_arc;
mod ku_array;
mod ku_c_call;
mod ku_instance;
mod ku_object;
mod ku_scalar;
mod ku_slice;
mod ku_string;
mod ku_tensor;
mod native;

pub use error::{Error, Result, Status};
pub use ku_arc::KuArc;
pub use ku_array::KuArray;
pub use ku_c_call::{KuCCall, KuFrameContext};
pub use ku_instance::{
    DeviceCapabilities, DeviceType, InstanceCapabilities, InstanceOptions, KuDevice, KuInstance,
    KuInstanceRef, Vendor,
};
pub use ku_object::{HasObjectKind, KuObject, KuObjectKind};
pub use ku_scalar::{Boolean, Element, KuScalar, PrimitiveType, ScalarValue};
pub use ku_slice::{KuSlice, SliceSpec};
pub use ku_string::KuString;
pub use ku_tensor::{KuTensor, TensorMetadata};
pub use native::{NativeObject, NativeType};

use kurt_sys as sys;

fn string_view(bytes: &[u8]) -> sys::ku_string_view_t {
    sys::ku_string_view_t {
        data: bytes.as_ptr().cast(),
        size: bytes.len(),
    }
}
