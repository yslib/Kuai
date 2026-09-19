#![allow(non_camel_case_types)]

use std::ffi::c_char;

pub type ku_status_t = i32;

pub const KU_STATUS_SUCCESS: ku_status_t = 0;

unsafe extern "C" {
    pub fn ku_status_string(status: ku_status_t) -> *const c_char;
}

#[cfg(test)]
mod tests {
    use std::ffi::CStr;

    use super::{KU_STATUS_SUCCESS, ku_status_string};

    #[test]
    fn status_string_is_provided_by_the_cpp_runtime() {
        let value = unsafe { CStr::from_ptr(ku_status_string(KU_STATUS_SUCCESS)) };
        assert_eq!(value.to_bytes(), b"success");
    }
}
