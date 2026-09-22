use crate::{HasObjectKind, sys};

pub(crate) mod private {
    pub trait Sealed {}
    pub trait View {}
}

/// A value exposing the runtime's native object ABI.
///
/// This trait is sealed and implemented only by the crate's typed object
/// views (KuScalar, KuTensor, KuArray, KuString, KuSlice), [`crate::KuObject`], and
/// their [`crate::KuArc`] owners.
/// It describes native ABI capability, not the
/// implementation language of the underlying value.
pub trait NativeObject: private::Sealed {
    /// Borrows the same live, non-null native object handle without retaining,
    /// releasing, mutating, allocating, or transferring ownership.
    ///
    /// The raw pointer does not carry Rust's borrow or extend any lifetime.
    /// Keep the object and all of its runtime dependencies alive while using
    /// the handle, and follow each unsafe FFI operation's contract. Do not
    /// release or mutate this borrowed reference. Retaining a separate owned
    /// reference requires following the sys ownership rules and independently
    /// ensuring its runtime dependencies remain valid; retain alone does not
    /// extend their lifetimes. This capability does not establish any
    /// tensor-specific preconditions.
    fn as_raw(&self) -> sys::ku_object_t;
}

/// A sealed lightweight view that may be owned by [`crate::KuArc`].
/// Every native view provides value classification through [`HasObjectKind`].
/// Only the crate's native object views implement this trait; owners do not.
pub trait NativeType: NativeObject + HasObjectKind + private::View {}
