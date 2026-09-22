use std::mem::MaybeUninit;
use std::num::NonZeroI64;
use std::ptr::{self, NonNull};

use crate::{
    HasObjectKind, KuArc, KuObjectKind, NativeObject, NativeType, Result, error::check,
    ku_arc::OwnedRaw, sys,
};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SliceSpec {
    pub start: Option<i64>,
    pub stop: Option<i64>,
    pub step: Option<NonZeroI64>,
}

/// A borrowed slice specification, independent of any runtime device.
#[derive(Debug)]
pub struct KuSlice {
    raw: NonNull<sys::_ku_object>,
}

impl crate::native::private::Sealed for KuSlice {}
impl crate::native::private::View for KuSlice {}
impl NativeType for KuSlice {}

// SAFETY: immutable descriptors have atomic native references and no runtime
// dependencies. Safe views remain borrowed from a live native owner.
unsafe impl Send for KuSlice {}
unsafe impl Sync for KuSlice {}

impl NativeObject for KuSlice {
    fn as_raw(&self) -> sys::ku_object_t {
        self.raw.as_ptr()
    }
}

impl HasObjectKind for KuSlice {
    fn kind(&self) -> KuObjectKind {
        KuObjectKind::Slice
    }
}

impl KuSlice {
    /// # Safety
    /// raw must identify a live immutable slice guarded by its owner, with
    /// valid descriptor fields and a nonzero explicit step when present.
    pub(crate) unsafe fn from_raw(raw: sys::ku_object_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("valid slice is non-null"),
        }
    }
}

impl KuArc<KuSlice> {
    pub fn new(spec: SliceSpec) -> Result<Self> {
        let input = sys::ku_slice_desc_t {
            start: spec.start.unwrap_or(0),
            stop: spec.stop.unwrap_or(0),
            step: spec.step.map(NonZeroI64::get).unwrap_or(0),
            flags: if spec.start.is_some() {
                sys::KU_SLICE_HAS_START
            } else {
                0
            } | if spec.stop.is_some() {
                sys::KU_SLICE_HAS_STOP
            } else {
                0
            } | if spec.step.is_some() {
                sys::KU_SLICE_HAS_STEP
            } else {
                0
            },
        };
        let mut raw = ptr::null_mut();
        // SAFETY: the descriptor is valid and the runtime copies its values.
        check(unsafe { sys::ku_slice_create(&input, &mut raw) })?;
        // SAFETY: success supplies one owned slice with the valid input
        // descriptor, whose explicit step is nonzero by its Rust type.
        unsafe {
            let owned: OwnedRaw<'static> = OwnedRaw::new(raw);
            Ok(owned.into_view(KuSlice::from_raw(raw)))
        }
    }
}

impl KuSlice {
    pub fn spec(&self) -> SliceSpec {
        let mut out = MaybeUninit::uninit();
        // SAFETY: this view borrows a live slice and the output slot is writable.
        let status = unsafe { sys::ku_slice_get_value(self.as_raw(), out.as_mut_ptr()) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        // SAFETY: the live slice kind guarantees successful initialization.
        let out = unsafe { out.assume_init() };
        SliceSpec {
            start: (out.flags & sys::KU_SLICE_HAS_START != 0).then_some(out.start),
            stop: (out.flags & sys::KU_SLICE_HAS_STOP != 0).then_some(out.stop),
            step: if out.flags & sys::KU_SLICE_HAS_STEP != 0 {
                debug_assert_ne!(out.step, 0, "native slice has a zero explicit step");
                NonZeroI64::new(out.step)
            } else {
                None
            },
        }
    }
}
