mod support;

use std::ffi::CStr;
use std::mem::MaybeUninit;
use std::ptr;

use kurt_sys::*;
use support::{Object, success, view};

#[test]
fn status_descriptions_include_known_and_unknown_codes() {
    let cases = [
        (KU_STATUS_SUCCESS, "success"),
        (KU_STATUS_NOT_READY, "not ready"),
        (KU_STATUS_BUFFER_TOO_SMALL, "buffer too small"),
        (KU_STATUS_INVALID_ARGUMENT, "invalid argument"),
        (KU_STATUS_OUT_OF_RANGE, "out of range"),
        (KU_STATUS_TYPE_MISMATCH, "type mismatch"),
        (KU_STATUS_INVALID_STATE, "invalid state"),
        (KU_STATUS_NOT_FOUND, "not found"),
        (KU_STATUS_ALREADY_INITIALIZED, "already initialized"),
        (KU_STATUS_NOT_SUPPORTED, "not supported"),
        (KU_STATUS_OUT_OF_HOST_MEMORY, "out of host memory"),
        (KU_STATUS_OUT_OF_DEVICE_MEMORY, "out of device memory"),
        (KU_STATUS_BACKEND_UNAVAILABLE, "backend unavailable"),
        (KU_STATUS_DEVICE_UNAVAILABLE, "device unavailable"),
        (KU_STATUS_DEVICE_LOST, "device lost"),
        (KU_STATUS_DEVICE_ERROR, "device error"),
        (KU_STATUS_BUILTIN_ERROR, "builtin error"),
        (KU_STATUS_INTERNAL_ERROR, "internal error"),
    ];
    for (code, expected) in cases {
        // SAFETY: the runtime returns static nul-terminated strings for all codes.
        let text = unsafe { CStr::from_ptr(ku_status_string(code)) };
        assert_eq!(text.to_bytes(), expected.as_bytes());
    }
    let unknown = unsafe { CStr::from_ptr(ku_status_string(-12345)) };
    assert!(!unknown.to_bytes().is_empty());
}

macro_rules! scalar_roundtrips {
    ($($test:ident: $tag:ident, $field:ident, [$($value:expr),+ $(,)?]);* $(;)?) => {
        $(
            #[test]
            fn $test() {
                for expected in [$($value),+] {
                    let input = ku_union_t {
                        value: ku_union_value_t { $field: expected },
                        tag: $tag,
                    };
                    // SAFETY: descriptors and outputs are live stack storage;
                    // the initialized payload member matches the tag.
                    unsafe {
                        let mut raw = ptr::null_mut();
                        success(ku_scalar_create(&input, &mut raw));
                        let scalar = Object(raw);
                        let mut kind = 0;
                        success(ku_object_get_kind(scalar.0, &mut kind));
                        assert_eq!(kind, KU_OBJECT_SCALAR);
                        let mut output = MaybeUninit::uninit();
                        success(ku_scalar_get_value(scalar.0, output.as_mut_ptr()));
                        let output = output.assume_init();
                        assert_eq!(output.tag, $tag);
                        assert_eq!(output.value.$field, expected);
                    }
                }
            }
        )*
    };
}

scalar_roundtrips! {
    scalar_none: KU_PRIMITIVE_NONE, none, [ku_void_t { reserved: 0 }];
    scalar_boolean: KU_PRIMITIVE_BOOLEAN, boolean, [
        ku_bool_t { value: KU_BOOL_NULL },
        ku_bool_t { value: KU_BOOL_FALSE },
        ku_bool_t { value: KU_BOOL_TRUE },
    ];
    scalar_byte: KU_PRIMITIVE_BYTE, ch, [i8::MIN, 0, i8::MAX];
    scalar_i16: KU_PRIMITIVE_I16, i16, [i16::MIN, 0, i16::MAX];
    scalar_i32: KU_PRIMITIVE_I32, i32, [i32::MIN, 0, i32::MAX];
    scalar_i64: KU_PRIMITIVE_I64, i64, [i64::MIN, 0, i64::MAX];
    scalar_f32: KU_PRIMITIVE_F32, f32, [-1.25, 0.0, f32::INFINITY];
    scalar_f64: KU_PRIMITIVE_F64, f64, [-1.25, 0.0, f64::INFINITY];
}

#[test]
fn string_copies_arbitrary_bytes_and_retain_keeps_them_alive() {
    for bytes in [b"".as_slice(), b"Kuai\0\xff".as_slice()] {
        // SAFETY: even empty slices provide a non-null input pointer. The
        // borrowed output is read only while an owned string reference lives.
        unsafe {
            let mut input = bytes.to_vec();
            let mut raw = ptr::null_mut();
            success(ku_string_create(view(&input), &mut raw));
            let original = Object(raw);
            success(ku_object_retain(raw));
            let retained = Object(raw);
            drop(original);
            input.fill(0);
            let mut output = MaybeUninit::uninit();
            success(ku_string_get_value(retained.0, output.as_mut_ptr()));
            let output = output.assume_init();
            let actual = std::slice::from_raw_parts(output.data.cast::<u8>(), output.size);
            assert_eq!(actual, bytes);
            let mut kind = 0;
            success(ku_object_get_kind(retained.0, &mut kind));
            assert_eq!(kind, KU_OBJECT_STRING);
        }
    }
}

#[test]
fn arrays_retain_elements_and_get_returns_an_owned_reference() {
    // SAFETY: all handles are live and every successful owned output has a guard.
    unsafe {
        let mut raw = ptr::null_mut();
        success(ku_string_create(view(b"element"), &mut raw));
        let item = Object(raw);
        let items = [item.0, item.0];
        success(ku_array_create(items.as_ptr(), items.len(), &mut raw));
        let array = Object(raw);
        drop(item);
        let mut count = 0;
        success(ku_array_get_size(array.0, &mut count));
        assert_eq!(count, 2);
        let mut kind = 0;
        success(ku_object_get_kind(array.0, &mut kind));
        assert_eq!(kind, KU_OBJECT_ARRAY);
        assert_eq!(ku_array_get(array.0, 2, &mut raw), KU_STATUS_OUT_OF_RANGE);
        assert!(raw.is_null());
        success(ku_array_get(array.0, 1, &mut raw));
        let selected = Object(raw);
        drop(array);
        let mut output = MaybeUninit::uninit();
        success(ku_string_get_value(selected.0, output.as_mut_ptr()));
        let output = output.assume_init();
        assert_eq!(
            std::slice::from_raw_parts(output.data.cast::<u8>(), output.size),
            b"element"
        );

        let empty: [ku_object_t; 0] = [];
        success(ku_array_create(empty.as_ptr(), 0, &mut raw));
        let empty = Object(raw);
        success(ku_array_get_size(empty.0, &mut count));
        assert_eq!(count, 0);
    }
}

#[test]
fn slices_preserve_optional_bounds_and_negative_steps() {
    for desc in [
        ku_slice_desc_t {
            start: 0,
            stop: 0,
            step: 0,
            flags: 0,
        },
        ku_slice_desc_t {
            start: 10,
            stop: -4,
            step: -2,
            flags: KU_SLICE_HAS_START | KU_SLICE_HAS_STOP | KU_SLICE_HAS_STEP,
        },
    ] {
        // SAFETY: valid descriptors and writable outputs; read after success.
        unsafe {
            let mut raw = ptr::null_mut();
            success(ku_slice_create(&desc, &mut raw));
            let slice = Object(raw);
            let mut kind = 0;
            success(ku_object_get_kind(slice.0, &mut kind));
            assert_eq!(kind, KU_OBJECT_SLICE);
            let mut output = MaybeUninit::uninit();
            success(ku_slice_get_value(slice.0, output.as_mut_ptr()));
            assert_eq!(output.assume_init(), desc);
        }
    }
}

#[test]
fn tensor_device_query_rejects_string_and_clears_output() {
    // SAFETY: the string is live and the output slot is writable. Its initial
    // sentinel is never dereferenced and must be overwritten by the query.
    unsafe {
        let mut raw = ptr::null_mut();
        success(ku_string_create(view(b"not a tensor"), &mut raw));
        let string = Object(raw);
        let mut device = ptr::dangling_mut();
        assert_eq!(
            ku_tensor_get_device(string.0, &mut device),
            KU_STATUS_TYPE_MISMATCH
        );
        assert!(device.is_null());
    }
}

#[test]
fn invalid_values_and_wrong_object_kinds_return_statuses() {
    // SAFETY: only semantic values are invalid; direct pointer contracts hold.
    unsafe {
        let mut raw = ptr::null_mut();
        let invalid = ku_union_t {
            value: ku_union_value_t { i64: 0 },
            tag: -1,
        };
        assert_eq!(
            ku_scalar_create(&invalid, &mut raw),
            KU_STATUS_INVALID_ARGUMENT
        );
        assert!(raw.is_null());
        let invalid = ku_slice_desc_t {
            start: 0,
            stop: 1,
            step: 0,
            flags: KU_SLICE_HAS_STEP,
        };
        assert_eq!(
            ku_slice_create(&invalid, &mut raw),
            KU_STATUS_INVALID_ARGUMENT
        );
        assert!(raw.is_null());

        success(ku_string_create(view(b"not a scalar"), &mut raw));
        let string = Object(raw);
        let mut scalar = MaybeUninit::uninit();
        assert_eq!(
            ku_scalar_get_value(string.0, scalar.as_mut_ptr()),
            KU_STATUS_TYPE_MISMATCH
        );
        let mut info = ku_tensor_info_t {
            primitive_type: KU_PRIMITIVE_F64,
            ndim: 2,
            shape: ptr::dangling(),
            strides: ptr::dangling(),
        };
        assert_eq!(
            ku_tensor_get_info(string.0, &mut info),
            KU_STATUS_TYPE_MISMATCH
        );
        assert_eq!(info.primitive_type, KU_PRIMITIVE_NONE);
        assert_eq!(info.ndim, 0);
        assert!(info.shape.is_null());
        assert!(info.strides.is_null());
        let mut data = ptr::dangling_mut();
        assert_eq!(
            ku_tensor_get_data(string.0, &mut data),
            KU_STATUS_TYPE_MISMATCH
        );
        assert!(data.is_null());
        let mut count = 0;
        assert_eq!(
            ku_array_get_size(string.0, &mut count),
            KU_STATUS_TYPE_MISMATCH
        );
    }
}
