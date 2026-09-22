use std::{marker::PhantomData, ptr::NonNull};

use crate::{
    Error, KuArc, KuArray, KuScalar, KuSlice, KuString, KuTensor, NativeObject, NativeType, Result,
    error::check, ku_arc::OwnedRaw, ku_instance::InstanceInner, sys,
};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum KuObjectKind {
    Scalar,
    Tensor,
    Array,
    String,
    Slice,
}

/// Classifies a value independently of native object access.
///
/// This safe, unsealed trait may be implemented by ordinary Rust values. Its
/// classification does not establish a native handle or justify unchecked casts.
pub trait HasObjectKind {
    /// Returns this value's classification.
    fn kind(&self) -> KuObjectKind;
}

impl KuObjectKind {
    fn from_raw(raw: i32) -> Result<Self> {
        match raw {
            sys::KU_OBJECT_SCALAR => Ok(Self::Scalar),
            sys::KU_OBJECT_TENSOR => Ok(Self::Tensor),
            sys::KU_OBJECT_ARRAY => Ok(Self::Array),
            sys::KU_OBJECT_STRING => Ok(Self::String),
            sys::KU_OBJECT_SLICE => Ok(Self::Slice),
            _ => Err(Error::TypeMismatch),
        }
    }
}

/// An erased native view borrowing all transitive runtime dependencies.
/// Ownership belongs to KuArc; the native object supplies its value kind.
#[derive(Debug)]
pub struct KuObject<'r> {
    raw: NonNull<sys::_ku_object>,
    _runtime: PhantomData<&'r InstanceInner>,
}

impl crate::native::private::Sealed for KuObject<'_> {}
impl crate::native::private::View for KuObject<'_> {}
impl NativeType for KuObject<'_> {}

// SAFETY: supported native objects have immutable initialized payloads and
// atomic references. Every transitive runtime dependency remains borrowed for
// 'r and supports cross-thread access and destruction under its native contract.
unsafe impl Send for KuObject<'_> {}
unsafe impl Sync for KuObject<'_> {}

impl NativeObject for KuObject<'_> {
    fn as_raw(&self) -> sys::ku_object_t {
        self.raw.as_ptr()
    }
}

impl HasObjectKind for KuObject<'_> {
    fn kind(&self) -> KuObjectKind {
        let mut kind = 0;
        // SAFETY: this view borrows a live immutable object of a supported kind.
        let status = unsafe { sys::ku_object_get_kind(self.as_raw(), &mut kind) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        KuObjectKind::from_raw(kind).expect("valid object has a supported native kind")
    }
}

impl<'r> KuObject<'r> {
    /// # Safety
    /// raw must identify a live immutable object guarded by its owner, with a
    /// known supported kind and initialized materializable payload. All nested
    /// payloads must also be valid, with runtime dependencies live throughout 'r.
    unsafe fn from_raw(raw: sys::ku_object_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("valid object is non-null"),
            _runtime: PhantomData,
        }
    }

    /// Classifies a guarded native reference, releasing it on errors.
    ///
    /// # Safety
    /// The owned object and every nested element must have initialized immutable
    /// materializable payloads: supported scalar tags, valid string storage,
    /// nonzero explicit slice steps, and valid tensor metadata/data. Every
    /// transitive runtime dependency must remain alive throughout 'r.
    pub(crate) unsafe fn from_native(native: OwnedRaw<'r>) -> Result<KuArc<Self>> {
        let raw = native.as_raw();
        let mut kind = 0;
        // SAFETY: native owns a live reference and guards all error cleanup.
        check(unsafe { sys::ku_object_get_kind(raw, &mut kind) })?;
        KuObjectKind::from_raw(kind)?;
        // SAFETY: classification proves a supported kind; the caller supplies
        // payload and runtime validity. The guard still owns the reference.
        let view = unsafe { Self::from_raw(raw) };
        // SAFETY: view describes this exact initialized native reference.
        Ok(unsafe { native.into_view(view) })
    }
}

impl<'r> From<KuArc<KuScalar>> for KuArc<KuObject<'r>> {
    fn from(value: KuArc<KuScalar>) -> Self {
        // SAFETY: the live owner establishes an independent immutable scalar.
        let target = unsafe { KuObject::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: immediately transfers the same reference without retain/release.
        unsafe { Self::from_view(target) }
    }
}

impl<'r> From<KuArc<KuString>> for KuArc<KuObject<'r>> {
    fn from(value: KuArc<KuString>) -> Self {
        // SAFETY: the live owner establishes an independent immutable string.
        let target = unsafe { KuObject::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: immediately transfers the same reference without retain/release.
        unsafe { Self::from_view(target) }
    }
}

impl<'r> From<KuArc<KuSlice>> for KuArc<KuObject<'r>> {
    fn from(value: KuArc<KuSlice>) -> Self {
        // SAFETY: the live owner establishes an independent immutable slice.
        let target = unsafe { KuObject::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: immediately transfers the same reference without retain/release.
        unsafe { Self::from_view(target) }
    }
}

impl<'r> From<KuArc<KuTensor<'r>>> for KuArc<KuObject<'r>> {
    fn from(value: KuArc<KuTensor<'r>>) -> Self {
        // SAFETY: the live owner establishes a valid tensor with dependencies in 'r.
        let target: KuObject<'r> = unsafe { KuObject::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: immediately transfers the same reference and runtime lifetime.
        unsafe { Self::from_view(target) }
    }
}

impl<'r> From<KuArc<KuArray<'r>>> for KuArc<KuObject<'r>> {
    fn from(value: KuArc<KuArray<'r>>) -> Self {
        // SAFETY: the live owner establishes a valid array with dependencies in 'r.
        let target: KuObject<'r> = unsafe { KuObject::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: immediately transfers the same reference and transitive lifetime.
        unsafe { Self::from_view(target) }
    }
}

impl TryFrom<KuArc<KuObject<'_>>> for KuArc<KuScalar> {
    type Error = Error;
    fn try_from(value: KuArc<KuObject<'_>>) -> Result<Self> {
        if value.kind() != KuObjectKind::Scalar {
            return Err(Error::TypeMismatch);
        }
        // SAFETY: the live owner and checked kind establish a valid scalar.
        let target = unsafe { KuScalar::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: scalar payloads have no runtime dependency; transfer ownership.
        Ok(unsafe { Self::from_view(target) })
    }
}

impl TryFrom<KuArc<KuObject<'_>>> for KuArc<KuString> {
    type Error = Error;
    fn try_from(value: KuArc<KuObject<'_>>) -> Result<Self> {
        if value.kind() != KuObjectKind::String {
            return Err(Error::TypeMismatch);
        }
        // SAFETY: the live owner and checked kind establish valid string storage.
        let target = unsafe { KuString::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: string storage has no runtime dependency; transfer ownership.
        Ok(unsafe { Self::from_view(target) })
    }
}

impl TryFrom<KuArc<KuObject<'_>>> for KuArc<KuSlice> {
    type Error = Error;
    fn try_from(value: KuArc<KuObject<'_>>) -> Result<Self> {
        if value.kind() != KuObjectKind::Slice {
            return Err(Error::TypeMismatch);
        }
        // SAFETY: the live owner and checked kind establish a valid descriptor.
        let target = unsafe { KuSlice::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: slice descriptors have no runtime dependency; transfer ownership.
        Ok(unsafe { Self::from_view(target) })
    }
}

impl<'r> TryFrom<KuArc<KuObject<'r>>> for KuArc<KuTensor<'r>> {
    type Error = Error;
    fn try_from(value: KuArc<KuObject<'r>>) -> Result<Self> {
        if value.kind() != KuObjectKind::Tensor {
            return Err(Error::TypeMismatch);
        }
        // SAFETY: the live owner and checked kind establish a valid tensor in 'r.
        let target: KuTensor<'r> = unsafe { KuTensor::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: transfers the same reference, kind, and runtime lifetime.
        Ok(unsafe { Self::from_view(target) })
    }
}

impl<'r> TryFrom<KuArc<KuObject<'r>>> for KuArc<KuArray<'r>> {
    type Error = Error;
    fn try_from(value: KuArc<KuObject<'r>>) -> Result<Self> {
        if value.kind() != KuObjectKind::Array {
            return Err(Error::TypeMismatch);
        }
        // SAFETY: the live owner and checked kind establish a valid array in 'r.
        let target: KuArray<'r> = unsafe { KuArray::from_raw(value.as_raw()) };
        let _source = value.into_view();
        // SAFETY: transfers the same reference, kind, and transitive lifetime.
        Ok(unsafe { Self::from_view(target) })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ScalarValue;

    #[test]
    fn classified_scalar_preserves_identity_and_wrong_downcast_preserves_other_owner() {
        let original = KuArc::<KuScalar>::new(42_i64).unwrap();
        let raw = original.as_raw();
        // SAFETY: original keeps this real scalar alive; the additional reference
        // is immediately guarded, classified, and consumed by the failed downcast.
        let object = unsafe {
            assert_eq!(sys::ku_object_retain(raw), sys::KU_STATUS_SUCCESS);
            KuObject::from_native(OwnedRaw::<'static>::new(raw)).unwrap()
        };
        assert_eq!(object.kind(), KuObjectKind::Scalar);
        assert_eq!(object.as_raw(), raw);
        assert!(matches!(
            KuArc::<KuString>::try_from(object),
            Err(Error::TypeMismatch)
        ));
        assert_eq!(original.value(), ScalarValue::I64(42));
    }
}
