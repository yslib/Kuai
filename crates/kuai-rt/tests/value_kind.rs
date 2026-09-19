use kuai_rt::{
    HasObjectKind, KuArc, KuArray, KuObject, KuObjectKind, KuScalar, KuSlice, KuString,
    NativeObject, NativeType, Result, SliceSpec,
};

fn classified<T: HasObjectKind + ?Sized>(value: &T) -> KuObjectKind {
    value.kind()
}

fn native_classified<T: NativeType>(value: &T) -> KuObjectKind {
    value.kind()
}

fn assert_classification<T: NativeType>(owner: &KuArc<T>, expected: KuObjectKind) {
    assert_eq!(classified(owner), expected);
    assert_eq!(classified(&**owner), expected);
    assert_eq!(native_classified(&**owner), expected);
}

#[test]
fn ordinary_rust_values_can_implement_classification() {
    struct RustValue;

    impl HasObjectKind for RustValue {
        fn kind(&self) -> KuObjectKind {
            KuObjectKind::Scalar
        }
    }

    assert_eq!(classified(&RustValue), KuObjectKind::Scalar);
}

#[test]
fn independent_kinds_and_erased_retention_preserve_native_identity() -> Result<()> {
    let scalar = KuArc::<KuScalar>::new(42_i64)?;
    let string = KuArc::<KuString>::new(b"Kuai")?;
    let slice = KuArc::<KuSlice>::new(SliceSpec::default())?;
    let array = KuArc::<KuArray>::new(&[])?;
    assert_classification(&scalar, KuObjectKind::Scalar);
    assert_classification(&string, KuObjectKind::String);
    assert_classification(&slice, KuObjectKind::Slice);
    assert_classification(&array, KuObjectKind::Array);

    let objects: [KuArc<KuObject<'static>>; 4] =
        [scalar.into(), string.into(), slice.into(), array.into()];
    for (object, expected) in objects.into_iter().zip([
        KuObjectKind::Scalar,
        KuObjectKind::String,
        KuObjectKind::Slice,
        KuObjectKind::Array,
    ]) {
        assert_classification(&object, expected);
        let raw = object.as_raw();
        let retained = KuArc::retain(&*object);
        drop(object);
        assert_eq!(retained.as_raw(), raw);
        assert_eq!(retained.kind(), expected);
    }
    Ok(())
}

#[cfg(kuai_runtime_cpu)]
#[test]
fn tensor_kind_and_erased_roundtrip_preserve_native_identity() -> Result<()> {
    use kuai_rt::{KuInstance, KuTensor, Vendor};

    let instance = KuInstance::new(Vendor::Cpu)?;
    let tensor = instance
        .default_device()
        .tensor_from_slice(&[2], &[3_i64, 5])?;
    assert_classification(&tensor, KuObjectKind::Tensor);
    let raw = tensor.as_raw();
    let object: KuArc<KuObject<'_>> = tensor.into();
    assert_classification(&object, KuObjectKind::Tensor);
    let tensor = KuArc::<KuTensor>::try_from(object)?;
    assert_eq!(tensor.as_raw(), raw);
    assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [3, 5]);
    Ok(())
}
