use std::{
    marker::PhantomData,
    ptr::{self, NonNull},
};

use crate::{
    HasObjectKind, KuArc, KuObject, KuObjectKind, NativeObject, NativeType, Result, error::check,
    ku_arc::OwnedRaw, ku_instance::InstanceInner, sys,
};

/// A view of a native array, borrowing all transitive runtime dependencies.
#[derive(Debug)]
pub struct KuArray<'r> {
    raw: NonNull<sys::_ku_object>,
    _runtime: PhantomData<&'r InstanceInner>,
}

impl crate::native::private::Sealed for KuArray<'_> {}
impl crate::native::private::View for KuArray<'_> {}
impl NativeType for KuArray<'_> {}

// SAFETY: the immutable native array retains its elements using atomic native
// references. All transitive runtime dependencies remain borrowed for 'r and
// support cross-thread access and destruction under their native contracts.
unsafe impl Send for KuArray<'_> {}
unsafe impl Sync for KuArray<'_> {}

impl NativeObject for KuArray<'_> {
    fn as_raw(&self) -> sys::ku_object_t {
        self.raw.as_ptr()
    }
}

impl HasObjectKind for KuArray<'_> {
    fn kind(&self) -> KuObjectKind {
        KuObjectKind::Array
    }
}

impl<'r> KuArray<'r> {
    /// # Safety
    /// raw must identify a live immutable array guarded by its owner. Every
    /// nested payload must be valid, with all runtime dependencies live for 'r.
    pub(crate) unsafe fn from_raw(raw: sys::ku_object_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("valid array is non-null"),
            _runtime: PhantomData,
        }
    }

    pub fn len(&self) -> usize {
        let mut len = 0;
        // SAFETY: this view borrows a live array with valid immutable metadata.
        let status = unsafe { sys::ku_array_get_size(self.as_raw(), &mut len) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        len
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Retains an element whose runtime borrow is independent of this array borrow.
    pub fn get(&self, index: usize) -> Result<KuArc<KuObject<'r>>> {
        let mut raw = ptr::null_mut();
        // SAFETY: the runtime checks the index and returns one owned reference.
        check(unsafe { sys::ku_array_get(self.as_raw(), index, &mut raw) })?;
        // SAFETY: the native array establishes its elements' valid payloads;
        // every transitive runtime dependency remains borrowed throughout 'r.
        unsafe {
            let owned = OwnedRaw::<'r>::new(raw);
            KuObject::from_native(owned)
        }
    }
}

impl<'r> KuArc<KuArray<'r>> {
    pub fn new(items: &[KuArc<KuObject<'r>>]) -> Result<Self> {
        let raw_items: Vec<_> = items.iter().map(NativeObject::as_raw).collect();
        let mut raw = ptr::null_mut();
        // SAFETY: every input owns a live reference with dependencies in 'r.
        // The native array retains elements and copies this temporary pointer list.
        check(unsafe { sys::ku_array_create(raw_items.as_ptr(), raw_items.len(), &mut raw) })?;
        // SAFETY: success returns one immutable array with the same dependencies.
        unsafe {
            let owned = OwnedRaw::<'r>::new(raw);
            Ok(owned.into_view(KuArray::from_raw(raw)))
        }
    }
}
