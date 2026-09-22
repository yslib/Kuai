use kurt::{HasObjectKind, *};

#[test]
fn erased_retention_preserves_native_identity() {
    let scalar = KuArc::<KuScalar>::new(42_i64).unwrap();
    let raw = scalar.as_raw();
    let object: KuArc<KuObject<'static>> = scalar.into();
    let retained = KuArc::retain(&*object);
    drop(object);
    let retained = KuArc::<KuScalar>::try_from(retained).unwrap();
    assert_eq!(retained.as_raw(), raw);
    assert_eq!(retained.value(), ScalarValue::I64(42));
}

#[test]
fn clone_preserves_payload_after_other_rust_and_native_owners_drop() -> Result<()> {
    let original = KuArc::<KuString>::new(b"native\0shared")?;
    let raw = original.as_raw();
    // SAFETY: original keeps this real object live while we acquire one C reference.
    assert_eq!(
        unsafe { kurt_sys::ku_object_retain(raw) },
        kurt_sys::KU_STATUS_SUCCESS
    );
    let clone = original.clone();
    drop(original);
    // SAFETY: release exactly the separate C reference, leaving the clone alive.
    assert_eq!(
        unsafe { kurt_sys::ku_object_release(raw) },
        kurt_sys::KU_STATUS_SUCCESS
    );
    assert_eq!(clone.as_raw(), raw);
    assert_eq!(clone.as_bytes(), b"native\0shared");
    Ok(())
}

#[test]
fn independent_kinds_upcast_and_downcast_without_changing_identity() -> Result<()> {
    let scalar = KuArc::<KuScalar>::new(42_i64)?;
    let raw = scalar.as_raw();
    let erased: KuArc<KuObject<'static>> = scalar.into();
    assert_eq!(erased.kind(), KuObjectKind::Scalar);
    let scalar = KuArc::<KuScalar>::try_from(erased)?;
    assert_eq!(scalar.as_raw(), raw);
    assert_eq!(scalar.value(), ScalarValue::I64(42));

    let text = KuArc::<KuString>::new(b"Kuai\0")?;
    let raw = text.as_raw();
    let erased: KuArc<KuObject<'static>> = text.into();
    assert_eq!(erased.kind(), KuObjectKind::String);
    let text = KuArc::<KuString>::try_from(erased)?;
    assert_eq!(text.as_raw(), raw);
    assert_eq!(text.as_bytes(), b"Kuai\0");

    let spec = SliceSpec {
        start: Some(-2),
        stop: Some(7),
        step: std::num::NonZeroI64::new(3),
    };
    let slice = KuArc::<KuSlice>::new(spec)?;
    let raw = slice.as_raw();
    let erased: KuArc<KuObject<'static>> = slice.into();
    assert_eq!(erased.kind(), KuObjectKind::Slice);
    let slice = KuArc::<KuSlice>::try_from(erased)?;
    assert_eq!(slice.as_raw(), raw);
    assert_eq!(slice.spec(), spec);

    let array = KuArc::<KuArray<'static>>::new(&[scalar.into(), text.into(), slice.into()])?;
    let raw = array.as_raw();
    let erased: KuArc<KuObject<'static>> = array.into();
    assert_eq!(erased.kind(), KuObjectKind::Array);
    let array = KuArc::<KuArray>::try_from(erased)?;
    assert_eq!(array.as_raw(), raw);
    assert_eq!(array.len(), 3);
    Ok(())
}

#[test]
fn wrong_kind_downcast_leaves_another_owner_usable() -> Result<()> {
    let scalar = KuArc::<KuScalar>::new(42_i64)?;
    let object: KuArc<KuObject<'static>> = scalar.clone().into();
    assert_eq!(object.as_raw(), scalar.as_raw());
    assert!(matches!(
        KuArc::<KuString>::try_from(object),
        Err(Error::TypeMismatch)
    ));
    assert_eq!(scalar.value(), ScalarValue::I64(42));
    Ok(())
}

#[cfg(kuai_runtime_cpu)]
mod cpu {
    use super::*;
    use std::sync::Mutex;

    static CPU: Mutex<()> = Mutex::new(());

    #[test]
    fn tensor_upcast_and_downcast_preserve_device_and_payload() -> Result<()> {
        let _guard = CPU.lock().unwrap();
        let instance = KuInstance::new(Vendor::Cpu)?;
        let tensor = instance
            .default_device()
            .tensor_from_slice(&[2], &[3_i64, 5])?;
        let raw = tensor.as_raw();
        let erased: KuArc<KuObject<'_>> = tensor.into();
        assert_eq!(erased.kind(), KuObjectKind::Tensor);
        let tensor = KuArc::<KuTensor>::try_from(erased)?;
        assert_eq!(tensor.as_raw(), raw);
        assert_eq!(tensor.device().instance().as_raw(), instance.as_raw());
        assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [3, 5]);
        Ok(())
    }

    #[test]
    fn instance_ref_outlives_temporary_device() -> Result<()> {
        let _guard = CPU.lock().unwrap();
        let instance = KuInstance::new(Vendor::Cpu)?;
        let borrowed = {
            let device = instance.device(0)?;
            device.instance()
        };
        let copied = borrowed;
        assert_eq!(borrowed.as_raw(), instance.as_raw());
        assert_eq!(
            copied.as_raw(),
            instance.default_device().instance().as_raw()
        );
        instance.close()
    }

    #[test]
    fn nested_array_elements_survive_original_tensor_and_parent_arrays() -> Result<()> {
        let _guard = CPU.lock().unwrap();
        let instance = KuInstance::new(Vendor::Cpu)?;
        let original = instance
            .default_device()
            .tensor_from_slice(&[2], &[7_i64, 11])?;
        let raw = original.as_raw();
        let inner = KuArc::<KuArray>::new(&[
            original.clone().into(),
            KuArc::<KuScalar>::new(42_i64)?.into(),
        ])?;
        let outer = KuArc::<KuArray>::new(&[inner.clone().into()])?;
        let retained_inner = KuArc::<KuArray>::try_from(outer.get(0)?)?;
        let tensor = KuArc::<KuTensor>::try_from(retained_inner.get(0)?)?;
        let scalar = KuArc::<KuScalar>::try_from(retained_inner.get(1)?)?;
        drop(retained_inner);
        drop(outer);
        drop(inner);
        drop(original);
        assert_eq!(tensor.as_raw(), raw);
        assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [7, 11]);
        assert_eq!(scalar.value(), ScalarValue::I64(42));
        drop(tensor);
        instance.close()
    }

    #[test]
    fn checked_leaf_conversion_outlives_instance() -> Result<()> {
        let _guard = CPU.lock().unwrap();
        let scalar = {
            let instance = KuInstance::new(Vendor::Cpu)?;
            let array = KuArc::<KuArray>::new(&[
                instance.default_device().zeros::<i64>(&[2])?.into(),
                KuArc::<KuScalar>::new(42_i64)?.into(),
            ])?;
            let scalar = KuArc::<KuScalar>::try_from(array.get(1)?)?;
            drop(array);
            instance.close()?;
            scalar
        };
        assert_eq!(scalar.value(), ScalarValue::I64(42));
        KuInstance::new(Vendor::Cpu)?.close()
    }
}
