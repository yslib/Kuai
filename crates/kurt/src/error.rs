use std::ffi::CStr;
use std::fmt;

use crate::sys;

/// A native status code, including codes introduced by newer runtime versions.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Status(pub i32);

impl Status {
    pub const SUCCESS: Self = Self(sys::KU_STATUS_SUCCESS);
    pub const NOT_READY: Self = Self(sys::KU_STATUS_NOT_READY);
    pub const BUFFER_TOO_SMALL: Self = Self(sys::KU_STATUS_BUFFER_TOO_SMALL);
    pub const INVALID_ARGUMENT: Self = Self(sys::KU_STATUS_INVALID_ARGUMENT);
    pub const OUT_OF_RANGE: Self = Self(sys::KU_STATUS_OUT_OF_RANGE);
    pub const TYPE_MISMATCH: Self = Self(sys::KU_STATUS_TYPE_MISMATCH);
    pub const INVALID_STATE: Self = Self(sys::KU_STATUS_INVALID_STATE);
    pub const NOT_FOUND: Self = Self(sys::KU_STATUS_NOT_FOUND);
    pub const ALREADY_INITIALIZED: Self = Self(sys::KU_STATUS_ALREADY_INITIALIZED);
    pub const NOT_SUPPORTED: Self = Self(sys::KU_STATUS_NOT_SUPPORTED);
    pub const OUT_OF_HOST_MEMORY: Self = Self(sys::KU_STATUS_OUT_OF_HOST_MEMORY);
    pub const OUT_OF_DEVICE_MEMORY: Self = Self(sys::KU_STATUS_OUT_OF_DEVICE_MEMORY);
    pub const BACKEND_UNAVAILABLE: Self = Self(sys::KU_STATUS_BACKEND_UNAVAILABLE);
    pub const DEVICE_UNAVAILABLE: Self = Self(sys::KU_STATUS_DEVICE_UNAVAILABLE);
    pub const DEVICE_LOST: Self = Self(sys::KU_STATUS_DEVICE_LOST);
    pub const DEVICE_ERROR: Self = Self(sys::KU_STATUS_DEVICE_ERROR);
    pub const BUILTIN_ERROR: Self = Self(sys::KU_STATUS_BUILTIN_ERROR);
    pub const INTERNAL_ERROR: Self = Self(sys::KU_STATUS_INTERNAL_ERROR);
}

impl fmt::Display for Status {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // SAFETY: every status maps to a static nul-terminated description.
        let text = unsafe { CStr::from_ptr(sys::ku_status_string(self.0)) };
        write!(f, "{} ({})", text.to_string_lossy(), self.0)
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum Error {
    Runtime(Status),
    InvalidArgument(&'static str),
    DeviceMismatch,
    TypeMismatch,
    InvalidUtf8(std::str::Utf8Error),
    ResourcesInUse,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Runtime(status) => status.fmt(f),
            Self::InvalidArgument(message) => f.write_str(message),
            Self::DeviceMismatch => f.write_str("objects belong to a different device or instance"),
            Self::TypeMismatch => f.write_str("runtime value has a different type"),
            Self::InvalidUtf8(error) => error.fmt(f),
            Self::ResourcesInUse => f.write_str("the instance still has live dependent resources"),
        }
    }
}

impl std::error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::InvalidUtf8(error) => Some(error),
            _ => None,
        }
    }
}

pub type Result<T> = std::result::Result<T, Error>;

pub(crate) fn check(status: sys::ku_status_t) -> Result<()> {
    if status == sys::KU_STATUS_SUCCESS {
        Ok(())
    } else {
        Err(Error::Runtime(Status(status)))
    }
}
