use std::ffi::CStr;

use kuai_sys::*;

pub fn success(status: ku_status_t) {
    // SAFETY: the status function returns a static nul-terminated string.
    let message = unsafe { CStr::from_ptr(ku_status_string(status)) };
    assert_eq!(status, KU_STATUS_SUCCESS, "{message:?}");
}

pub fn view(bytes: &[u8]) -> ku_string_view_t {
    ku_string_view_t {
        data: bytes.as_ptr().cast(),
        size: bytes.len(),
    }
}

// Test-only ownership guard. Construct only from a successful owned output.
pub struct Object(pub ku_object_t);

impl Drop for Object {
    fn drop(&mut self) {
        // SAFETY: each guard owns one reference from the runtime.
        unsafe { success(ku_object_release(self.0)) }
    }
}
