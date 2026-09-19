use std::{
    marker::PhantomData,
    mem::ManuallyDrop,
    ops::Deref,
    ptr::{self, NonNull},
};

use crate::{
    HasObjectKind, KuObjectKind, NativeObject, NativeType, ku_instance::InstanceInner, sys,
};

/// Owns one intrusive native reference and exposes a lightweight Rust view.
/// Runtime dependencies are borrowed through the view's lifetime, not retained.
#[derive(Debug)]
pub struct KuArc<T: NativeType> {
    view: T,
}

impl<T: NativeType> KuArc<T> {
    /// Retains the native object once, preserving the view's runtime lifetime.
    pub fn retain(view: &T) -> Self {
        // SAFETY: the borrowed view identifies a live native object. All sealed
        // NativeType implementations contain only a pointer and optional lifetime
        // marker: no owned fields or Drop implementations. Copying this Rust view
        // after retaining represents the new native reference.
        let status = unsafe { sys::ku_object_retain(view.as_raw()) };
        assert_eq!(
            status,
            sys::KU_STATUS_SUCCESS,
            "retaining a live object failed"
        );
        Self {
            view: unsafe { ptr::read(view) },
        }
    }

    /// # Safety
    /// `view` must identify one owned native reference not owned elsewhere, with
    /// an initialized immutable payload of its declared kind. Every transitive
    /// runtime dependency must remain valid for the lifetime encoded in T.
    pub(crate) unsafe fn from_view(view: T) -> Self {
        Self { view }
    }

    // Transfers the one native reference into a replacement owner. Callers
    // must immediately adopt this view; bare views do not release references.
    pub(crate) fn into_view(self) -> T {
        let this = ManuallyDrop::new(self);
        // SAFETY: self will not drop; moving the lightweight view transfers the
        // existing reference without retaining, releasing, or copying C memory.
        unsafe { ptr::read(&this.view) }
    }
}

impl<T: NativeType> Clone for KuArc<T> {
    fn clone(&self) -> Self {
        Self::retain(&self.view)
    }
}

impl<T: NativeType> Deref for KuArc<T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.view
    }
}

impl<T: NativeType> crate::native::private::Sealed for KuArc<T> {}
impl<T: NativeType> HasObjectKind for KuArc<T> {
    fn kind(&self) -> KuObjectKind {
        <T as HasObjectKind>::kind(&self.view)
    }
}

impl<T: NativeType> NativeObject for KuArc<T> {
    fn as_raw(&self) -> sys::ku_object_t {
        self.view.as_raw()
    }
}

impl<T: NativeType> Drop for KuArc<T> {
    fn drop(&mut self) {
        // SAFETY: owns exactly one reference; T's lifetime keeps dependencies
        // live. Native release diagnostics cannot panic during destruction.
        unsafe {
            let _ = sys::ku_object_release(self.as_raw());
        }
    }
}

// An unclassified owned output, never a publicly usable view. Its lifetime
// covers all runtime dependencies through classification and error cleanup.
pub(crate) struct OwnedRaw<'r> {
    raw: NonNull<sys::_ku_object>,
    _runtime: PhantomData<&'r InstanceInner>,
}

impl<'r> OwnedRaw<'r> {
    /// # Safety
    /// raw owns one live native reference. Its immutable materializable payload
    /// and all transitive dependencies must be valid for 'r, including cleanup.
    /// An unpublished tensor may still be initializing only if an enclosing
    /// synchronous wait guard finishes before this guard can drop or be decoded.
    pub(crate) unsafe fn new(raw: sys::ku_object_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("successful object output is non-null"),
            _runtime: PhantomData,
        }
    }

    pub(crate) fn as_raw(&self) -> sys::ku_object_t {
        self.raw.as_ptr()
    }

    /// # Safety
    /// view must identify this exact object with initialized payload and the
    /// correct kind. Its lifetime cannot outlive any runtime dependency. Only
    /// independent leaf kinds may discard 'r entirely.
    pub(crate) unsafe fn into_view<T: NativeType>(self, view: T) -> KuArc<T> {
        let _this = ManuallyDrop::new(self);
        // SAFETY: the caller transfers this reference into the matching view.
        unsafe { KuArc::from_view(view) }
    }
}

impl Drop for OwnedRaw<'_> {
    fn drop(&mut self) {
        // SAFETY: owns one reference and its runtime remains borrowed.
        unsafe {
            let _ = sys::ku_object_release(self.as_raw());
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::{KuArc, KuObject, KuScalar, NativeObject, ScalarValue};

    #[test]
    fn cloned_converted_reference_preserves_identity_after_other_owners_drop() {
        let original = KuArc::<KuScalar>::new(42_i64).unwrap();
        let raw = original.as_raw();
        let erased: KuArc<KuObject<'static>> = original.clone().into();
        drop(original);
        let scalar = KuArc::<KuScalar>::try_from(erased).unwrap();
        assert_eq!(scalar.as_raw(), raw);
        assert_eq!(scalar.value(), ScalarValue::I64(42));
    }
}
