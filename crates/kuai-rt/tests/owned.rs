#![cfg(kuai_runtime_cpu)]

use kuai_rt::{HasObjectKind, *};
use std::sync::Mutex;

static CPU: Mutex<()> = Mutex::new(());

#[test]
fn device_copies_and_tensor_share_the_runtime_borrow() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = { instance.default_device() };
    let tensor = device.tensor_from_slice(&[2], &[3_i64, 5])?;
    let reader = device;
    assert_eq!(reader.as_raw(), device.as_raw());
    assert_eq!(reader.to_vec::<i64>(&tensor)?, [3, 5]);
    drop(tensor);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn queried_device_outlives_local_tensor() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let queried = {
        let device = instance.device(0)?;
        let tensor = instance
            .default_device()
            .tensor_from_slice(&[2], &[19_i64, 23])?;
        let queried = tensor.device();
        assert_eq!(queried.as_raw(), device.as_raw());
        assert!(tensor.is_on(&queried));
        assert_eq!(queried.to_vec::<i64>(&tensor)?, [19, 23]);
        drop(tensor);
        queried
    };
    assert_eq!(queried.instance().as_raw(), instance.as_raw());
    let tensor = queried.tensor_from_slice(&[2], &[29_i64, 31])?;
    assert_eq!(queried.to_vec::<i64>(&tensor)?, [29, 31]);
    drop(tensor);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn explicit_instance_clone_prevents_close_until_final_release() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let remaining = instance.clone();
    assert_eq!(instance.close(), Err(Error::ResourcesInUse));
    let device = remaining.default_device();
    let tensor = device.tensor_from_slice(&[2], &[37_i64, 41])?;
    assert_eq!(device.to_vec::<i64>(&tensor)?, [37, 41]);
    drop(tensor);
    remaining.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn tensor_from_temporary_device_releases_before_instance_close() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let tensor = instance
        .default_device()
        .tensor_from_slice(&[2], &[43_i64, 47])?;
    assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [43, 47]);
    drop(tensor);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn native_views_and_owners_are_send_sync() {
    fn both<T: Send + Sync>() {}
    fn native<T: NativeObject>() {}
    both::<KuDevice>();
    both::<KuTensor>();
    both::<KuArray>();
    both::<KuObject>();
    both::<KuInstanceRef>();
    both::<KuArc<KuTensor>>();
    both::<KuArc<KuArray>>();
    both::<KuArc<KuObject>>();
    native::<KuTensor>();
    native::<KuArray>();
    native::<KuObject>();
    native::<KuArc<KuTensor>>();
    native::<KuArc<KuArray>>();
    native::<KuArc<KuObject>>();
}

#[test]
fn owned_leaf_arrays_and_conversions_need_no_instance() -> Result<()> {
    let scalar = KuArc::<KuScalar>::new(42_i64)?;
    let text = KuArc::<KuString>::new(b"owned\0value")?;
    let slice = KuArc::<KuSlice>::new(SliceSpec::default())?;
    let array = KuArc::<KuArray>::new(&[])?;
    assert!(array.is_empty());
    let objects = [
        scalar.clone().into(),
        text.clone().into(),
        slice.clone().into(),
        array.clone().into(),
    ];
    let kinds = [
        KuObjectKind::Scalar,
        KuObjectKind::String,
        KuObjectKind::Slice,
        KuObjectKind::Array,
    ];
    let raw = [
        scalar.as_raw(),
        text.as_raw(),
        slice.as_raw(),
        array.as_raw(),
    ];
    let leaves = KuArc::<KuArray>::new(&objects)?;
    assert_eq!(leaves.len(), 4);
    for (index, object) in objects.iter().enumerate() {
        assert_eq!(object.kind(), kinds[index]);
        assert_eq!(object.as_raw(), raw[index]);
        assert_eq!(object.clone().as_raw(), raw[index]);
        assert_eq!(leaves.get(index)?.as_raw(), raw[index]);
    }
    assert_eq!(
        KuArc::<KuScalar>::try_from(objects[0].clone())?.value(),
        ScalarValue::I64(42)
    );
    assert_eq!(
        KuArc::<KuString>::try_from(objects[1].clone())?.as_bytes(),
        b"owned\0value"
    );
    assert_eq!(
        KuArc::<KuSlice>::try_from(objects[2].clone())?.spec(),
        SliceSpec::default()
    );
    assert!(KuArc::<KuArray>::try_from(objects[3].clone())?.is_empty());
    assert!(matches!(
        KuArc::<KuScalar>::try_from(objects[1].clone()),
        Err(Error::TypeMismatch)
    ));
    assert!(matches!(
        KuArc::<KuString>::try_from(objects[0].clone()),
        Err(Error::TypeMismatch)
    ));
    assert!(matches!(
        KuArc::<KuSlice>::try_from(objects[0].clone()),
        Err(Error::TypeMismatch)
    ));
    assert!(matches!(
        KuArc::<KuArray>::try_from(objects[0].clone()),
        Err(Error::TypeMismatch)
    ));
    assert!(matches!(
        KuArc::<KuTensor>::try_from(objects[0].clone()),
        Err(Error::TypeMismatch)
    ));
    assert!(leaves.get(4).is_err());
    let clone = leaves.clone();
    assert_eq!(clone.as_raw(), leaves.as_raw());
    assert_eq!(clone.len(), 4);
    Ok(())
}

#[test]
fn nested_arrays_keep_extracted_tensor_references_alive() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let array = {
        let device = instance.default_device();
        let tensor = device.tensor_from_slice(&[2], &[7_i64, 11])?;
        let raw = tensor.as_raw();
        let object: KuArc<KuObject<'_>> = tensor.into();
        assert_eq!(object.kind(), KuObjectKind::Tensor);
        assert_eq!(object.as_raw(), raw);
        assert_eq!(KuArc::<KuTensor>::try_from(object.clone())?.as_raw(), raw);
        let inner = KuArc::<KuArray>::new(&[object])?;
        KuArc::<KuArray>::new(&[inner.into(), KuArc::<KuScalar>::new(42_i64)?.into()])?
    };
    let inner = KuArc::<KuArray>::try_from(array.get(0)?)?;
    drop(array);
    let object = inner.get(0)?;
    drop(inner);
    // The owned reference survives every parent while borrowing the KuInstance.
    assert_eq!(object.kind(), KuObjectKind::Tensor);
    let tensor = KuArc::<KuTensor>::try_from(object)?;
    assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [7, 11]);
    std::thread::scope(|scope| scope.spawn(move || drop(tensor)).join().unwrap());
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn extracted_leaves_outlive_array_runtime_borrows() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let array = {
        let device = instance.default_device();
        KuArc::<KuArray>::new(&[
            device.zeros::<i64>(&[1])?.into(),
            KuArc::<KuScalar>::new(42_i64)?.into(),
            KuArc::<KuString>::new("leaf")?.into(),
            KuArc::<KuSlice>::new(SliceSpec::default())?.into(),
        ])?
    };
    let scalar = KuArc::<KuScalar>::try_from(array.get(1)?)?;
    let text = KuArc::<KuString>::try_from(array.get(2)?)?;
    let slice = KuArc::<KuSlice>::try_from(array.get(3)?)?;
    drop(array);
    instance.close()?;
    assert_eq!(scalar.value(), ScalarValue::I64(42));
    assert_eq!(text.as_bytes(), b"leaf");
    assert_eq!(slice.spec(), SliceSpec::default());
    Ok(())
}

#[test]
fn nested_objects_preserve_identity_and_keep_exact_device() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let owned = {
        let device = instance.default_device();
        let tensor = device.tensor_from_slice(&[2], &[13_i64, 17])?;
        let object: KuArc<KuObject<'_>> = tensor.into();
        let cloned = object.clone();
        assert_eq!(cloned.as_raw(), object.as_raw());
        let tensor = KuArc::<KuTensor>::try_from(cloned)?;
        assert!(tensor.is_on(&device));
        assert_eq!(tensor.device().as_raw(), device.as_raw());
        let inner = KuArc::<KuArray>::new(&[object])?;
        let nested = KuArc::<KuArray>::new(&[KuArc::<KuScalar>::new(1_i64)?.into(), inner.into()])?;
        let object: KuArc<KuObject<'_>> = nested.clone().into();
        assert_eq!(object.as_raw(), nested.as_raw());
        nested
    };
    // The array retains native references and borrows the still-live runtime.
    let inner = KuArc::<KuArray>::try_from(owned.get(1)?)?;
    let tensor = KuArc::<KuTensor>::try_from(inner.get(0)?)?;
    assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [13, 17]);
    drop(tensor);
    drop(inner);
    drop(owned);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn leaf_and_empty_arrays_do_not_retain_instances() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let empty = KuArc::<KuArray>::new(&[])?;
    assert!(empty.is_empty());
    let instance = KuInstance::new(Vendor::Cpu)?;
    let (owned, scalar, text, slice) = {
        let scalar: KuArc<KuObject<'static>> = KuArc::<KuScalar>::new(42_i64)?.into();
        let text: KuArc<KuObject<'static>> = KuArc::<KuString>::new("leaf")?.into();
        let slice: KuArc<KuObject<'static>> = KuArc::<KuSlice>::new(SliceSpec::default())?.into();
        let leaves = KuArc::<KuArray>::new(&[
            scalar.clone(),
            text.clone(),
            slice.clone(),
            KuArc::<KuArray>::new(&[])?.into(),
        ])?;
        (leaves, scalar, text, slice)
    };
    instance.close()?;
    assert_eq!(owned.len(), 4);
    assert_eq!(
        KuArc::<KuScalar>::try_from(scalar)?.value(),
        ScalarValue::I64(42)
    );
    assert_eq!(KuArc::<KuString>::try_from(text)?.as_bytes(), b"leaf");
    assert_eq!(
        KuArc::<KuSlice>::try_from(slice)?.spec(),
        SliceSpec::default()
    );
    Ok(())
}

#[test]
fn owned_array_clones_extract_and_download_on_concurrent_threads() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let array = {
        let tensor = instance
            .default_device()
            .tensor_from_slice(&[3], &[2_i64, 3, 5])?;
        KuArc::<KuArray>::new(&[tensor.into()])?
    };
    std::thread::scope(|scope| {
        let workers: Vec<_> = (0..8)
            .map(|_| {
                let array = array.clone();
                scope.spawn(move || {
                    for _ in 0..16 {
                        let clone = array.clone();
                        let tensor = KuArc::<KuTensor>::try_from(clone.get(0).unwrap()).unwrap();
                        assert_eq!(tensor.device().to_vec::<i64>(&tensor).unwrap(), [2, 3, 5]);
                    }
                })
            })
            .collect();
        drop(array);
        for worker in workers {
            worker.join().unwrap();
        }
    });
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn tensor_releases_on_another_thread_before_instance_close() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = { instance.default_device() };
    let tensor = device.tensor_from_slice(&[2], &[3_i64, 5])?;
    std::thread::scope(|scope| {
        scope
            .spawn(move || {
                assert_eq!(tensor.device().to_vec::<i64>(&tensor).unwrap(), [3, 5]);
            })
            .join()
            .unwrap()
    });
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn tensor_clones_share_native_identity_within_runtime_borrow() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let original = device.zeros::<i64>(&[2])?;
    let tensor = original.clone();
    assert_eq!(tensor.as_raw(), original.as_raw());
    drop(original);
    let clone = tensor.clone();
    drop(tensor);
    let retained = clone.clone();
    assert_eq!(retained.as_raw(), clone.as_raw());
    assert_eq!(clone.metadata().shape, [2]);
    assert_eq!(clone.len(), 2);
    assert!(!clone.is_empty());
    assert!(clone.is_on(&device));
    assert_eq!(device.to_vec::<i64>(&clone)?, [0, 0]);
    drop(retained);
    drop(clone);
    assert_eq!(device.info()?.1, 0);
    assert_eq!(device.capabilities()?.device_id, 0);
    device.flush()?;
    let empty = device.zeros::<i64>(&[0])?;
    assert!(empty.is_empty());
    drop(empty);
    std::thread::scope(|scope| {
        scope
            .spawn(move || assert_eq!(device.info().unwrap().1, 0))
            .join()
            .unwrap()
    });
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}
