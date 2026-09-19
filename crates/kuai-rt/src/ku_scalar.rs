use std::mem::MaybeUninit;
use std::ptr::{self, NonNull};

use crate::{
    HasObjectKind, KuArc, KuObjectKind, NativeObject, NativeType, Result, error::check,
    ku_arc::OwnedRaw, sys,
};

/// A borrowed scalar view, independent of any runtime device.
#[derive(Debug)]
pub struct KuScalar {
    raw: NonNull<sys::_ku_object>,
}

impl crate::native::private::Sealed for KuScalar {}
impl crate::native::private::View for KuScalar {}
impl NativeType for KuScalar {}

// SAFETY: scalar payloads are immutable, native references are atomic, and
// there are no runtime dependencies. KuArc keeps the native object live.
unsafe impl Send for KuScalar {}
unsafe impl Sync for KuScalar {}

impl NativeObject for KuScalar {
    fn as_raw(&self) -> sys::ku_object_t {
        self.raw.as_ptr()
    }
}

impl HasObjectKind for KuScalar {
    fn kind(&self) -> KuObjectKind {
        KuObjectKind::Scalar
    }
}

impl KuScalar {
    /// # Safety
    /// raw must identify a live immutable scalar with a supported primitive
    /// tag and initialized matching payload. Its owner must guard this view.
    pub(crate) unsafe fn from_raw(raw: sys::ku_object_t) -> Self {
        Self {
            raw: NonNull::new(raw).expect("valid scalar is non-null"),
        }
    }

    pub fn value(&self) -> ScalarValue {
        let mut out = MaybeUninit::uninit();
        // SAFETY: this view borrows a live scalar and the output slot is writable.
        let status = unsafe { sys::ku_scalar_get_value(self.as_raw(), out.as_mut_ptr()) };
        debug_assert_eq!(status, sys::KU_STATUS_SUCCESS);
        // SAFETY: the live scalar kind guarantees successful initialization;
        // its invariant provides a supported tag and initialized matching member.
        unsafe { ScalarValue::from_raw(out.assume_init()) }
    }
}

impl KuArc<KuScalar> {
    pub fn new(value: impl Into<ScalarValue>) -> Result<Self> {
        let input = value.into().raw();
        let mut raw = ptr::null_mut();
        // SAFETY: the payload matches its tag; input and output are valid.
        check(unsafe { sys::ku_scalar_create(&input, &mut raw) })?;
        // SAFETY: success supplies one independent scalar with valid payload.
        unsafe {
            let owned: OwnedRaw<'static> = OwnedRaw::new(raw);
            Ok(owned.into_view(KuScalar::from_raw(raw)))
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(i32)]
pub enum PrimitiveType {
    None = sys::KU_PRIMITIVE_NONE,
    Boolean = sys::KU_PRIMITIVE_BOOLEAN,
    Byte = sys::KU_PRIMITIVE_BYTE,
    I16 = sys::KU_PRIMITIVE_I16,
    I32 = sys::KU_PRIMITIVE_I32,
    I64 = sys::KU_PRIMITIVE_I64,
    F32 = sys::KU_PRIMITIVE_F32,
    F64 = sys::KU_PRIMITIVE_F64,
}

/// A runtime boolean, including the native null value. Its representation is
/// an integer byte, so every bit pattern written by the runtime is valid Rust.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct Boolean(i8);

impl Boolean {
    pub const NULL: Self = Self(sys::KU_BOOL_NULL);
    pub const FALSE: Self = Self(sys::KU_BOOL_FALSE);
    pub const TRUE: Self = Self(sys::KU_BOOL_TRUE);

    pub fn value(self) -> Option<bool> {
        if self.0 == sys::KU_BOOL_NULL {
            None
        } else {
            Some(self.0 != 0)
        }
    }
}

impl From<bool> for Boolean {
    fn from(value: bool) -> Self {
        if value { Self::TRUE } else { Self::FALSE }
    }
}

impl From<Option<bool>> for Boolean {
    fn from(value: Option<bool>) -> Self {
        value.map(Self::from).unwrap_or(Self::NULL)
    }
}

mod private {
    pub trait Sealed {}
}

/// Initialized, byte-copyable tensor elements with a fixed native dtype.
/// Sealed to prevent incorrect layouts or invalid Rust bit patterns.
pub trait Element: private::Sealed + Copy + Default + Unpin + Send + Sync + 'static {
    const TYPE: PrimitiveType;
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ScalarValue {
    None,
    Boolean(Boolean),
    Byte(i8),
    I16(i16),
    I32(i32),
    I64(i64),
    F32(f32),
    F64(f64),
}

impl private::Sealed for i8 {}
impl Element for i8 {
    const TYPE: PrimitiveType = PrimitiveType::Byte;
}
impl From<i8> for ScalarValue {
    fn from(value: i8) -> Self {
        Self::Byte(value)
    }
}

impl private::Sealed for i16 {}
impl Element for i16 {
    const TYPE: PrimitiveType = PrimitiveType::I16;
}
impl From<i16> for ScalarValue {
    fn from(value: i16) -> Self {
        Self::I16(value)
    }
}

impl private::Sealed for i32 {}
impl Element for i32 {
    const TYPE: PrimitiveType = PrimitiveType::I32;
}
impl From<i32> for ScalarValue {
    fn from(value: i32) -> Self {
        Self::I32(value)
    }
}

impl private::Sealed for i64 {}
impl Element for i64 {
    const TYPE: PrimitiveType = PrimitiveType::I64;
}
impl From<i64> for ScalarValue {
    fn from(value: i64) -> Self {
        Self::I64(value)
    }
}

impl private::Sealed for f32 {}
impl Element for f32 {
    const TYPE: PrimitiveType = PrimitiveType::F32;
}
impl From<f32> for ScalarValue {
    fn from(value: f32) -> Self {
        Self::F32(value)
    }
}

impl private::Sealed for f64 {}
impl Element for f64 {
    const TYPE: PrimitiveType = PrimitiveType::F64;
}
impl From<f64> for ScalarValue {
    fn from(value: f64) -> Self {
        Self::F64(value)
    }
}

impl ScalarValue {
    pub fn primitive_type(self) -> PrimitiveType {
        match self {
            Self::None => PrimitiveType::None,
            Self::Boolean(_) => PrimitiveType::Boolean,
            Self::Byte(_) => PrimitiveType::Byte,
            Self::I16(_) => PrimitiveType::I16,
            Self::I32(_) => PrimitiveType::I32,
            Self::I64(_) => PrimitiveType::I64,
            Self::F32(_) => PrimitiveType::F32,
            Self::F64(_) => PrimitiveType::F64,
        }
    }

    pub(crate) fn raw(self) -> sys::ku_union_t {
        let value = match self {
            Self::None => sys::ku_union_value_t {
                none: sys::ku_void_t { reserved: 0 },
            },
            Self::Boolean(value) => sys::ku_union_value_t {
                boolean: sys::ku_bool_t { value: value.0 },
            },
            Self::Byte(value) => sys::ku_union_value_t { ch: value },
            Self::I16(value) => sys::ku_union_value_t { i16: value },
            Self::I32(value) => sys::ku_union_value_t { i32: value },
            Self::I64(value) => sys::ku_union_value_t { i64: value },
            Self::F32(value) => sys::ku_union_value_t { f32: value },
            Self::F64(value) => sys::ku_union_value_t { f64: value },
        };
        sys::ku_union_t {
            value,
            tag: self.primitive_type() as i32,
        }
    }

    /// # Safety
    /// `raw` must have a supported primitive tag and the union member selected
    /// by that tag must be initialized.
    pub(crate) unsafe fn from_raw(raw: sys::ku_union_t) -> Self {
        // SAFETY: the caller guarantees that the matched member is initialized.
        unsafe {
            match raw.tag {
                sys::KU_PRIMITIVE_NONE => Self::None,
                sys::KU_PRIMITIVE_BOOLEAN => Self::Boolean(Boolean(raw.value.boolean.value)),
                sys::KU_PRIMITIVE_BYTE => Self::Byte(raw.value.ch),
                sys::KU_PRIMITIVE_I16 => Self::I16(raw.value.i16),
                sys::KU_PRIMITIVE_I32 => Self::I32(raw.value.i32),
                sys::KU_PRIMITIVE_I64 => Self::I64(raw.value.i64),
                sys::KU_PRIMITIVE_F32 => Self::F32(raw.value.f32),
                sys::KU_PRIMITIVE_F64 => Self::F64(raw.value.f64),
                _ => unreachable!("native scalar has an unsupported primitive tag"),
            }
        }
    }
}

impl private::Sealed for Boolean {}
impl Element for Boolean {
    const TYPE: PrimitiveType = PrimitiveType::Boolean;
}
impl From<Boolean> for ScalarValue {
    fn from(value: Boolean) -> Self {
        Self::Boolean(value)
    }
}
impl From<bool> for ScalarValue {
    fn from(value: bool) -> Self {
        Self::Boolean(value.into())
    }
}
impl From<Option<bool>> for ScalarValue {
    fn from(value: Option<bool>) -> Self {
        Self::Boolean(value.into())
    }
}
impl From<()> for ScalarValue {
    fn from(_: ()) -> Self {
        Self::None
    }
}
