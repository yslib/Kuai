use std::mem::MaybeUninit;
use std::ptr::{self, NonNull};

use crate::{
    Error, HasObjectKind, KuArc, KuObjectKind, NativeObject, NativeType, Result, error::check,
    ku_arc::OwnedRaw, string_view, sys,
};

/// A borrowed immutable byte-string view, independent of any runtime device.
#[derive(Debug)]
pub struct KuString {
    raw: NonNull<sys::_ku_object>,
}

impl crate::native::private::Sealed for KuString {}
impl crate::native::private::View for KuString {}
impl NativeType for KuString {}

// SAFETY: byte storage is immutable, native references are atomic, and no
// runtime dependency is needed. Every safe view is borrowed from a live owner.
unsafe impl Send for KuString {}
unsafe impl Sync for KuString {}

impl NativeObject for KuString {
    fn as_raw(&self) -> sys::ku_object_t {
        self.raw.as_ptr()
    }
}

impl HasObjectKind for KuString {
    fn kind(&self) -> KuObjectKind {
        KuObjectKind::String
    }
}

impl KuString {
    /// # Safety
    /// raw must identify a live immutable string guarded by its owner. Nonempty
    /// storage must be one allocation with non-null data and size <= isize::MAX.
    pub(crate) unsafe fn from_raw(raw: sys::ku_object_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("valid string is non-null"),
        }
    }

    pub fn as_bytes(&self) -> &[u8] {
        let mut out = MaybeUninit::uninit();
        // SAFETY: this view borrows a live string and the output slot is writable.
        let status = unsafe { sys::ku_string_get_value(self.as_raw(), out.as_mut_ptr()) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        // SAFETY: the live string kind guarantees successful initialization.
        let out = unsafe { out.assume_init() };
        if out.size == 0 {
            return &[];
        }
        // SAFETY: the string invariant guarantees valid immutable byte storage;
        // the returned slice borrows self and cannot outlive its native owner.
        unsafe { std::slice::from_raw_parts(out.data.cast(), out.size) }
    }

    pub fn to_str(&self) -> Result<&str> {
        std::str::from_utf8(self.as_bytes()).map_err(Error::InvalidUtf8)
    }
}

impl KuArc<KuString> {
    pub fn new(bytes: impl AsRef<[u8]>) -> Result<Self> {
        let mut raw = ptr::null_mut();
        // SAFETY: even empty Rust slices have non-null data; the runtime copies it.
        check(unsafe { sys::ku_string_create(string_view(bytes.as_ref()), &mut raw) })?;
        // SAFETY: success supplies one independent string with valid byte storage.
        unsafe {
            let owned: OwnedRaw<'static> = OwnedRaw::new(raw);
            Ok(owned.into_view(KuString::from_raw(raw)))
        }
    }
}
