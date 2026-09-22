use kurt::{HasObjectKind, *};

#[test]
fn typed_value_accessors_return_values_directly() -> Result<()> {
    let scalar = KuArc::<KuScalar>::new(42_i64)?;
    let value: ScalarValue = scalar.value();
    assert_eq!(value, ScalarValue::I64(42));

    let text = KuArc::<KuString>::new(b"native\0bytes")?;
    let bytes: &[u8] = text.as_bytes();
    assert_eq!(bytes, b"native\0bytes");
    assert!(KuArc::<KuString>::new([])?.as_bytes().is_empty());

    let spec = SliceSpec {
        start: Some(-3),
        stop: Some(5),
        step: std::num::NonZeroI64::new(-2),
    };
    let actual: SliceSpec = KuArc::<KuSlice>::new(spec)?.spec();
    assert_eq!(actual, spec);

    let array = KuArc::<KuArray>::new(&[scalar.into(), text.into()])?;
    let len: usize = array.len();
    let empty: bool = array.is_empty();
    assert_eq!(len, 2);
    assert!(!empty);
    let empty = KuArc::<KuArray>::new(&[])?;
    assert_eq!(empty.len(), 0);
    assert!(empty.is_empty());
    Ok(())
}

#[test]
fn native_object_trait_passes_typed_and_erased_handles_to_ffi() -> Result<()> {
    fn check_kind<T: NativeObject + ?Sized>(value: &T, expected: i32) {
        let raw = value.as_raw();
        assert!(!raw.is_null());
        assert_eq!(raw, value.as_raw());
        let mut kind = 0;
        // SAFETY: the sealed capability supplies a live borrowed object; this
        // synchronous query neither mutates nor retains it.
        assert_eq!(
            unsafe { kurt_sys::ku_object_get_kind(raw, &mut kind) },
            kurt_sys::KU_STATUS_SUCCESS
        );
        assert_eq!(kind, expected);
    }

    let scalar = KuArc::<KuScalar>::new(42_i64)?;
    let string = KuArc::<KuString>::new(b"native\0object")?;
    let slice = KuArc::<KuSlice>::new(SliceSpec::default())?;
    let array = KuArc::<KuArray>::new(&[scalar.clone().into()])?;
    check_kind(&scalar, kurt_sys::KU_OBJECT_SCALAR);
    check_kind(&string, kurt_sys::KU_OBJECT_STRING);
    check_kind(&slice, kurt_sys::KU_OBJECT_SLICE);
    check_kind(&array, kurt_sys::KU_OBJECT_ARRAY);

    let objects: [KuArc<KuObject<'_>>; 4] =
        [scalar.into(), string.into(), slice.into(), array.into()];
    let expected = [
        kurt_sys::KU_OBJECT_SCALAR,
        kurt_sys::KU_OBJECT_STRING,
        kurt_sys::KU_OBJECT_SLICE,
        kurt_sys::KU_OBJECT_ARRAY,
    ];
    for (object, kind) in objects.iter().zip(expected) {
        check_kind(object, kind);
        let erased: &dyn NativeObject = object;
        check_kind(erased, kind);
        assert_eq!(erased.as_raw(), object.as_raw());
    }
    let retained = objects.clone();
    drop(objects);
    for (object, kind) in retained.iter().zip(expected) {
        check_kind(object, kind);
    }
    Ok(())
}

macro_rules! scalar_tests {
    ($($name:ident: $input:expr => $expected:expr),* $(,)?) => {
        $(#[test] fn $name() -> Result<()> {
            let scalar = KuArc::<KuScalar>::new($input)?;
            assert_eq!(scalar.value(), $expected);
            let object: KuArc<KuObject<'_>> = scalar.clone().into();
            drop(scalar);
            assert_eq!(object.kind(), KuObjectKind::Scalar);
            assert_eq!(KuArc::<KuScalar>::try_from(object)?.value(), $expected);
            Ok(())
        })*
    };
}

scalar_tests! {
    none: () => ScalarValue::None,
    boolean: true => ScalarValue::Boolean(Boolean::TRUE),
    null_boolean: None::<bool> => ScalarValue::Boolean(Boolean::NULL),
    byte: -7_i8 => ScalarValue::Byte(-7),
    i16: -123_i16 => ScalarValue::I16(-123),
    i32: -123_i32 => ScalarValue::I32(-123),
    i64: i64::MAX => ScalarValue::I64(i64::MAX),
    f32: 1.25_f32 => ScalarValue::F32(1.25),
    f64: 1.25_f64 => ScalarValue::F64(1.25),
}

#[test]
fn strings_preserve_bytes_and_validate_utf8() -> Result<()> {
    let input = vec![b'K', 0, 255];
    let value = KuArc::<KuString>::new(&input)?;
    drop(input);
    assert_eq!(value.as_bytes(), [b'K', 0, 255]);
    assert!(matches!(value.to_str(), Err(Error::InvalidUtf8(_))));
    assert_eq!(KuArc::<KuString>::new("快")?.to_str()?, "快");
    assert_eq!(KuArc::<KuString>::new([])?.as_bytes(), []);
    Ok(())
}

#[test]
fn arrays_retain_values_and_check_bounds_and_types() -> Result<()> {
    let element: KuArc<KuObject<'_>> = KuArc::<KuScalar>::new(42_i64)?.into();
    let inner: KuArc<KuObject<'_>> = KuArc::<KuArray>::new(&[element])?.into();
    let outer = KuArc::<KuArray>::new(&[inner])?;
    let inner = KuArc::<KuArray>::try_from(outer.get(0)?)?;
    drop(outer);
    let element = inner.get(0)?;
    drop(inner);
    assert_eq!(
        KuArc::<KuScalar>::try_from(element.clone())?.value(),
        ScalarValue::I64(42)
    );
    assert!(matches!(
        KuArc::<KuString>::try_from(element),
        Err(Error::TypeMismatch)
    ));
    let empty = KuArc::<KuArray>::new(&[])?;
    assert!(empty.is_empty());
    assert!(matches!(
        empty.get(0),
        Err(Error::Runtime(Status::OUT_OF_RANGE))
    ));
    Ok(())
}

#[test]
fn slices_distinguish_missing_bounds_and_nonzero_steps() -> Result<()> {
    for spec in [
        SliceSpec::default(),
        SliceSpec {
            start: Some(0),
            stop: Some(-7),
            step: std::num::NonZeroI64::new(-2),
        },
    ] {
        assert_eq!(KuArc::<KuSlice>::new(spec)?.spec(), spec);
    }
    assert_eq!(Boolean::NULL.value(), None);
    assert_eq!(Boolean::FALSE.value(), Some(false));
    assert_eq!(Boolean::TRUE.value(), Some(true));
    Ok(())
}

#[test]
fn errors_preserve_native_status_and_have_descriptions() {
    let error = Error::Runtime(Status::NOT_FOUND);
    assert!(error.to_string().contains("not found"));
    assert!(!Status(-12345).to_string().is_empty());
    fn is_error<T: std::error::Error>() {}
    is_error::<Error>();
}

#[test]
fn erased_kinds_preserve_native_identity_and_allow_checked_payload_access() -> Result<()> {
    let number = KuArc::<KuScalar>::new(42_i64)?;
    let number_raw = number.as_raw();
    let text = KuArc::<KuString>::new(b"Kuai\0")?;
    let text_raw = text.as_raw();
    let slice = KuArc::<KuSlice>::new(SliceSpec::default())?;
    let slice_raw = slice.as_raw();
    let array = KuArc::<KuArray>::new(&[number.clone().into()])?;
    let array_raw = array.as_raw();
    let values: [KuArc<KuObject<'_>>; 4] = [number.into(), text.into(), slice.into(), array.into()];
    let retained = values.clone();
    drop(values);
    for (value, expected_raw) in retained
        .iter()
        .zip([number_raw, text_raw, slice_raw, array_raw])
    {
        assert_eq!(value.as_raw(), expected_raw);
    }
    for value in retained {
        match value.kind() {
            KuObjectKind::Scalar => {
                assert_eq!(
                    KuArc::<KuScalar>::try_from(value)?.value(),
                    ScalarValue::I64(42)
                );
            }
            KuObjectKind::String => {
                assert_eq!(KuArc::<KuString>::try_from(value)?.as_bytes(), b"Kuai\0");
            }
            KuObjectKind::Slice => {
                assert_eq!(
                    KuArc::<KuSlice>::try_from(value)?.spec(),
                    SliceSpec::default()
                );
            }
            KuObjectKind::Array => {
                let value = KuArc::<KuArray>::try_from(value)?;
                assert_eq!(
                    KuArc::<KuScalar>::try_from(value.get(0)?)?.value(),
                    ScalarValue::I64(42)
                );
            }
            KuObjectKind::Tensor => panic!("no tensor was constructed"),
        }
    }
    Ok(())
}

#[test]
fn native_owner_outlives_rust_wrappers() -> Result<()> {
    let value = KuArc::<KuScalar>::new(42_i64)?;
    let raw = value.as_raw();
    // SAFETY: acquire our own native reference while the Rust owner is live.
    assert_eq!(
        unsafe { kurt_sys::ku_object_retain(raw) },
        kurt_sys::KU_STATUS_SUCCESS
    );
    let cloned = value.clone();
    drop(value);
    drop(cloned);
    let mut kind = 0;
    // SAFETY: our separately retained reference survives both Rust wrappers.
    let status = unsafe { kurt_sys::ku_object_get_kind(raw, &mut kind) };
    // SAFETY: release exactly the reference explicitly retained above.
    let released = unsafe { kurt_sys::ku_object_release(raw) };
    assert_eq!(status, kurt_sys::KU_STATUS_SUCCESS);
    assert_eq!(released, kurt_sys::KU_STATUS_SUCCESS);
    assert_eq!(kind, kurt_sys::KU_OBJECT_SCALAR);
    Ok(())
}

#[test]
fn rust_clone_outlives_other_native_and_rust_owners() -> Result<()> {
    let value = KuArc::<KuString>::new(b"shared\0value")?;
    let raw = value.as_raw();
    // SAFETY: independently retain this live string, then release that same reference.
    assert_eq!(
        unsafe { kurt_sys::ku_object_retain(raw) },
        kurt_sys::KU_STATUS_SUCCESS
    );
    let cloned = value.clone();
    drop(value);
    assert_eq!(
        unsafe { kurt_sys::ku_object_release(raw) },
        kurt_sys::KU_STATUS_SUCCESS
    );
    assert_eq!(cloned.as_raw(), raw);
    assert_eq!(cloned.as_bytes(), b"shared\0value");
    Ok(())
}

#[test]
fn primitive_type_mappings_are_preserved() {
    for (value, kind) in [
        (ScalarValue::None, PrimitiveType::None),
        (ScalarValue::Boolean(Boolean::NULL), PrimitiveType::Boolean),
        (ScalarValue::Byte(-1), PrimitiveType::Byte),
        (ScalarValue::I16(-2), PrimitiveType::I16),
        (ScalarValue::I32(-3), PrimitiveType::I32),
        (ScalarValue::I64(-4), PrimitiveType::I64),
        (ScalarValue::F32(1.25), PrimitiveType::F32),
        (ScalarValue::F64(2.5), PrimitiveType::F64),
    ] {
        assert_eq!(value.primitive_type(), kind);
    }
    assert_eq!(<i8 as Element>::TYPE, PrimitiveType::Byte);
    assert_eq!(<i16 as Element>::TYPE, PrimitiveType::I16);
    assert_eq!(<i32 as Element>::TYPE, PrimitiveType::I32);
    assert_eq!(<i64 as Element>::TYPE, PrimitiveType::I64);
    assert_eq!(<f32 as Element>::TYPE, PrimitiveType::F32);
    assert_eq!(<f64 as Element>::TYPE, PrimitiveType::F64);
    assert_eq!(<Boolean as Element>::TYPE, PrimitiveType::Boolean);
}

#[test]
fn status_constants_match_the_native_abi() {
    for (status, raw) in [
        (Status::SUCCESS, kurt_sys::KU_STATUS_SUCCESS),
        (Status::NOT_READY, kurt_sys::KU_STATUS_NOT_READY),
        (
            Status::BUFFER_TOO_SMALL,
            kurt_sys::KU_STATUS_BUFFER_TOO_SMALL,
        ),
        (
            Status::INVALID_ARGUMENT,
            kurt_sys::KU_STATUS_INVALID_ARGUMENT,
        ),
        (Status::OUT_OF_RANGE, kurt_sys::KU_STATUS_OUT_OF_RANGE),
        (Status::TYPE_MISMATCH, kurt_sys::KU_STATUS_TYPE_MISMATCH),
        (Status::INVALID_STATE, kurt_sys::KU_STATUS_INVALID_STATE),
        (Status::NOT_FOUND, kurt_sys::KU_STATUS_NOT_FOUND),
        (
            Status::ALREADY_INITIALIZED,
            kurt_sys::KU_STATUS_ALREADY_INITIALIZED,
        ),
        (Status::NOT_SUPPORTED, kurt_sys::KU_STATUS_NOT_SUPPORTED),
        (
            Status::OUT_OF_HOST_MEMORY,
            kurt_sys::KU_STATUS_OUT_OF_HOST_MEMORY,
        ),
        (
            Status::OUT_OF_DEVICE_MEMORY,
            kurt_sys::KU_STATUS_OUT_OF_DEVICE_MEMORY,
        ),
        (
            Status::BACKEND_UNAVAILABLE,
            kurt_sys::KU_STATUS_BACKEND_UNAVAILABLE,
        ),
        (
            Status::DEVICE_UNAVAILABLE,
            kurt_sys::KU_STATUS_DEVICE_UNAVAILABLE,
        ),
        (Status::DEVICE_LOST, kurt_sys::KU_STATUS_DEVICE_LOST),
        (Status::DEVICE_ERROR, kurt_sys::KU_STATUS_DEVICE_ERROR),
        (Status::BUILTIN_ERROR, kurt_sys::KU_STATUS_BUILTIN_ERROR),
        (Status::INTERNAL_ERROR, kurt_sys::KU_STATUS_INTERNAL_ERROR),
    ] {
        assert_eq!(status.0, raw);
    }
}
