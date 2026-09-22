#![cfg(kuai_runtime_cpu)]

use std::sync::Mutex;

use kurt::{HasObjectKind, *};

static CPU: Mutex<()> = Mutex::new(());

#[test]
fn synchronous_upload_releases_host_borrow_before_return() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let mut input = vec![3_i64, 5];
    let tensor = device.tensor_from_slice(&[2], &input)?;
    input.fill(99);
    drop(input);
    assert_eq!(device.to_vec::<i64>(&tensor)?, [3, 5]);

    let empty = device.tensor_from_slice::<i64>(&[0], &[])?;
    assert!(device.to_vec::<i64>(&empty)?.is_empty());
    let scalar = device.tensor_from_slice(&[], &[7_i64])?;
    assert_eq!(scalar.len(), 1);
    assert_eq!(device.to_vec::<i64>(&scalar)?, [7]);
    Ok(())
}

#[test]
fn tensor_accessors_return_values_directly() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let tensor = device.zeros::<i64>(&[2, 3])?;
    let len: usize = tensor.len();
    let empty: bool = tensor.is_empty();
    let same_device: bool = tensor.is_on(&device);
    assert_eq!(len, 6);
    assert!(!empty);
    assert!(same_device);
    let zero = device.zeros::<i64>(&[2, 0])?;
    assert_eq!(zero.len(), 0);
    assert!(zero.is_empty());
    assert!(zero.is_on(&device));
    drop(zero);
    drop(tensor);
    instance.close()?;
    Ok(())
}

#[test]
fn native_object_trait_accepts_tensor_and_device_dependent_array() -> Result<()> {
    fn check_kind<T: NativeObject + ?Sized>(object: &T, expected: i32) {
        let mut kind = 0;
        // SAFETY: the borrowed object and its KuInstance remain live throughout
        // this synchronous, read-only query.
        assert_eq!(
            unsafe { kurt_sys::ku_object_get_kind(object.as_raw(), &mut kind) },
            kurt_sys::KU_STATUS_SUCCESS
        );
        assert_eq!(kind, expected);
    }

    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let tensor = instance
        .default_device()
        .tensor_from_slice(&[2], &[7_i64, 8])?;
    check_kind(&tensor, kurt_sys::KU_OBJECT_TENSOR);
    let raw = NativeObject::as_raw(&tensor);
    let object: KuArc<KuObject<'_>> = tensor.into();
    check_kind(&object, kurt_sys::KU_OBJECT_TENSOR);
    assert_eq!(NativeObject::as_raw(&object), raw);
    let erased: &dyn NativeObject = &object;
    check_kind(erased, kurt_sys::KU_OBJECT_TENSOR);
    let array = KuArc::<KuArray>::new(&[object])?;
    check_kind(&array, kurt_sys::KU_OBJECT_ARRAY);
    let tensor = KuArc::<KuTensor>::try_from(array.get(0)?)?;
    drop(array);
    assert_eq!(NativeObject::as_raw(&tensor), raw);
    assert_eq!(instance.default_device().to_vec::<i64>(&tensor)?, [7, 8]);
    drop(tensor);
    instance.close()?;
    Ok(())
}

#[test]
fn instance_configuration_capabilities_and_ownership() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    assert!(matches!(
        KuInstance::with_options(InstanceOptions {
            device_ids: vec![],
            ..Default::default()
        }),
        Err(Error::InvalidArgument(_))
    ));
    assert!(matches!(
        KuInstance::with_options(InstanceOptions {
            device_ids: vec![0, 0],
            ..Default::default()
        }),
        Err(Error::InvalidArgument(_))
    ));
    let instance = KuInstance::with_options(InstanceOptions {
        worker_threads: 2,
        ..Default::default()
    })?;
    let device = instance.default_device();
    assert_eq!(device.info()?, (DeviceType::Cpu, 0));
    assert_eq!(instance.capabilities()?.worker_threads, 2);
    assert_eq!(device.capabilities()?.device_id, 0);
    assert!(matches!(
        instance.device(42),
        Err(Error::Runtime(Status::OUT_OF_RANGE))
    ));
    assert!(matches!(
        KuInstance::new(Vendor::Cpu),
        Err(Error::Runtime(Status::ALREADY_INITIALIZED))
    ));
    assert_eq!(instance.clone().close(), Err(Error::ResourcesInUse));
    device.flush()?;
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()?;
    Ok(())
}

macro_rules! tensor_roundtrips {
    ($($name:ident: $ty:ty => $data:expr),* $(,)?) => {
        $(#[test] fn $name() -> Result<()> {
            let _guard = CPU.lock().unwrap();
            let instance = KuInstance::new(Vendor::Cpu)?;
            let data: Vec<$ty> = $data;
            let tensor = {
                let device = instance.default_device();
                device.tensor_from_slice(&[2, 3], &data)?
            };
            let metadata: TensorMetadata = tensor.metadata();
            assert_eq!(metadata.shape, [2, 3]);
            assert_eq!(metadata.strides, [1, 2]);
            assert_eq!(metadata.primitive_type, <$ty as Element>::TYPE);
            assert_eq!(instance.default_device().to_vec::<$ty>(&tensor)?, data);
            drop(tensor);
            instance.close()?;
            Ok(())
        })*
    };
}

tensor_roundtrips! {
    tensor_byte: i8 => vec![0, -1, 2, 3, 4, 5],
    tensor_i16: i16 => vec![0, -1, 2, 3, 4, 5],
    tensor_i32: i32 => vec![0, -1, 2, 3, 4, 5],
    tensor_i64: i64 => vec![0, -1, 2, 3, 4, 5],
    tensor_f32: f32 => vec![0.0, -1.0, 2.0, 3.0, 4.0, 5.0],
    tensor_f64: f64 => vec![0.0, -1.0, 2.0, 3.0, 4.0, 5.0],
    tensor_boolean: Boolean => vec![Boolean::NULL, Boolean::FALSE, Boolean::TRUE, Boolean::NULL, Boolean::TRUE, Boolean::FALSE],
}

#[test]
fn tensor_validation_empty_rank_zero_and_wrong_dtype() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    assert!(matches!(
        device.tensor_from_slice(&[2], &[1_i64]),
        Err(Error::InvalidArgument(_))
    ));
    assert!(matches!(
        device.zeros::<i64>(&[usize::MAX, 2]),
        Err(Error::InvalidArgument(_))
    ));
    assert!(matches!(
        device.zeros::<i64>(&[1; 9]),
        Err(Error::InvalidArgument(_))
    ));
    let empty = device.zeros::<f64>(&[2, 0, 3])?;
    assert!(empty.is_empty());
    assert!(device.to_vec::<f64>(&empty)?.is_empty());
    assert_eq!(empty.metadata().shape, [2, 0, 3]);
    assert_eq!(empty.metadata().strides, [1, 2, 0]);
    assert_eq!(empty.metadata().primitive_type, PrimitiveType::F64);
    let scalar = device.tensor_from_slice(&[], &[42_i64])?;
    assert_eq!(scalar.len(), 1);
    assert!(scalar.metadata().shape.is_empty());
    assert!(scalar.metadata().strides.is_empty());
    assert_eq!(scalar.metadata().primitive_type, PrimitiveType::I64);
    assert_eq!(device.to_vec::<i64>(&scalar)?, [42]);
    assert_eq!(device.to_vec::<f64>(&scalar), Err(Error::TypeMismatch));
    Ok(())
}

#[test]
fn array_elements_outlive_array_wrappers_within_the_instance() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let tensor = device.tensor_from_slice(&[3], &[11_i32, 22, 33])?;
    let inner = KuArc::<KuArray>::new(&[tensor.into()])?;
    let outer = KuArc::<KuArray>::new(&[inner.into()])?;
    let inner = KuArc::<KuArray>::try_from(outer.get(0)?)?;
    drop(outer);
    let tensor = KuArc::<KuTensor>::try_from(inner.get(0)?)?;
    drop(inner);
    assert_eq!(device.to_vec::<i32>(&tensor)?, [11, 22, 33]);
    drop(tensor);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()?;
    Ok(())
}

#[test]
fn checked_leaves_from_mixed_arrays_outlive_the_instance() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let spec = SliceSpec {
        start: Some(1),
        stop: Some(7),
        step: std::num::NonZeroI64::new(2),
    };
    let (scalar, bytes, slice) = {
        let instance = KuInstance::new(Vendor::Cpu)?;
        let tensor = instance.default_device().zeros::<i64>(&[2])?;
        let mixed = KuArc::<KuArray>::new(&[
            tensor.into(),
            KuArc::<KuScalar>::new(42_i64)?.into(),
            KuArc::<KuString>::new(b"Kuai\0runtime")?.into(),
            KuArc::<KuSlice>::new(spec)?.into(),
        ])?;
        let outer = KuArc::<KuArray>::new(&[mixed.into()])?;
        let mixed = KuArc::<KuArray>::try_from(outer.get(0)?)?;
        drop(outer);
        assert!(matches!(
            KuArc::<KuScalar>::try_from(mixed.get(0)?),
            Err(Error::TypeMismatch)
        ));
        assert!(matches!(
            KuArc::<KuString>::try_from(mixed.get(0)?),
            Err(Error::TypeMismatch)
        ));
        assert!(matches!(
            KuArc::<KuSlice>::try_from(mixed.get(0)?),
            Err(Error::TypeMismatch)
        ));
        let leaves = (
            KuArc::<KuScalar>::try_from(mixed.get(1)?)?,
            KuArc::<KuString>::try_from(mixed.get(2)?)?,
            KuArc::<KuSlice>::try_from(mixed.get(3)?)?,
        );
        drop(mixed);
        instance.close()?;
        leaves
    };
    assert_eq!(scalar.value(), ScalarValue::I64(42));
    assert_eq!(bytes.as_bytes(), b"Kuai\0runtime");
    assert_eq!(slice.spec(), spec);
    KuInstance::new(Vendor::Cpu)?.close()?;
    Ok(())
}

#[test]
fn synchronous_transfers_preserve_large_buffers() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let data: Vec<i64> = (0..65_536).collect();
    let tensor = device.tensor_from_slice(&[data.len()], &data)?;
    let copied = device.to_vec::<i64>(&tensor)?;
    drop(tensor);
    assert_eq!(copied, data);
    instance.close()?;
    Ok(())
}

#[test]
fn cloned_tensor_metadata_and_download_outlive_the_original() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let tensor = device.tensor_from_slice(&[2], &[7_i64, 8])?;
    let retained = tensor.clone();
    drop(tensor);
    let metadata: TensorMetadata = retained.metadata();
    assert_eq!(metadata.shape, [2]);
    assert_eq!(metadata.strides, [1]);
    assert_eq!(metadata.primitive_type, PrimitiveType::I64);
    let values = device.to_vec::<i64>(&retained)?;
    drop(retained);
    assert_eq!(values, [7, 8]);
    assert_eq!(metadata.shape, [2]);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()?;
    Ok(())
}

#[test]
fn builtin_lookup_and_checked_frame_lifetimes() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    assert!(instance.builtin_names()?.iter().any(|name| name == "add"));
    assert!(matches!(
        instance.builtin("missing_function"),
        Err(Error::Runtime(Status::NOT_FOUND))
    ));
    let add: KuCCall<'_> = instance.builtin("add")?;
    let device: KuDevice<'_> = instance.default_device();
    let mut context: KuFrameContext<'_> = device.frame_context()?;
    let left: KuArc<KuObject<'_>> = KuArc::<KuScalar>::new(20_i64)?.into();
    let right: KuArc<KuObject<'_>> = KuArc::<KuScalar>::new(22_i64)?.into();
    // SAFETY: scalar integer addition is defined for these non-overflowing
    // operands; this builtin neither mutates nor retains its arguments.
    let results = unsafe { add.call_unchecked(&mut context, &[Some(&*left), Some(&*right)], &[]) }?;
    assert_eq!(results.len(), 1);
    let result = KuArc::<KuScalar>::try_from(results.into_iter().next().unwrap().unwrap())?;
    assert_eq!(result.value(), ScalarValue::I64(42));
    drop(context);
    drop(result);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()?;
    Ok(())
}

#[test]
fn builtin_tensor_result_outlives_local_arguments_context_and_callable() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let result = {
        let mut context = instance.default_device().frame_context()?;
        let temporary = instance.clone();
        let add = temporary.builtin("add")?;
        let left: KuArc<KuObject<'_>> = temporary
            .default_device()
            .tensor_from_slice(&[3], &[1_i64, 2, 3])?
            .into();
        let right: KuArc<KuObject<'_>> = temporary
            .default_device()
            .tensor_from_slice(&[3], &[10_i64, 20, 30])?
            .into();
        // SAFETY: matching initialized integer tensors use non-overflowing
        // addition; this builtin neither mutates nor retains its arguments.
        let mut results =
            unsafe { add.call_unchecked(&mut context, &[Some(&*left), Some(&*right)], &[]) }?;
        assert_eq!(results.len(), 1);
        results.pop().unwrap().unwrap()
    };
    let tensor = KuArc::<KuTensor>::try_from(result)?;
    assert_eq!(tensor.metadata().shape, [3]);
    assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [11, 22, 33]);
    drop(tensor);
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()?;
    Ok(())
}

#[test]
fn erased_tensor_and_array_clones_preserve_native_identity() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let tensor = device.tensor_from_slice(&[2], &[7_i64, 8])?;
    let tensor_raw = tensor.as_raw();
    assert!(tensor.is_on(&device));
    let array = KuArc::<KuArray>::new(&[tensor.clone().into()])?;
    let array_raw = array.as_raw();
    let retained_array = array.clone();
    drop(array);
    drop(tensor);
    assert_eq!(retained_array.as_raw(), array_raw);
    let tensor = {
        let object = retained_array.get(0)?;
        assert_eq!(object.kind(), KuObjectKind::Tensor);
        let cloned_object = object.clone();
        drop(object);
        drop(retained_array);
        KuArc::<KuTensor>::try_from(cloned_object)?
    };
    assert_eq!(tensor.as_raw(), tensor_raw);
    assert!(tensor.is_on(&device));
    assert_eq!(device.to_vec::<i64>(&tensor)?, [7, 8]);
    drop(tensor);
    instance.close()?;
    Ok(())
}

#[test]
fn checked_leaves_from_nested_arrays_outlive_the_instance() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let (scalar, string, slice) = {
        let instance = KuInstance::new(Vendor::Cpu)?;
        let tensor = instance.default_device().zeros::<i64>(&[2])?;
        let inner = KuArc::<KuArray>::new(&[
            tensor.into(),
            KuArc::<KuScalar>::new(42_i64)?.into(),
            KuArc::<KuString>::new(b"independent\0")?.into(),
            KuArc::<KuSlice>::new(SliceSpec::default())?.into(),
        ])?;
        let outer = KuArc::<KuArray>::new(&[inner.into()])?;
        let inner = KuArc::<KuArray>::try_from(outer.get(0)?)?;
        drop(outer);
        let scalar = KuArc::<KuScalar>::try_from(inner.get(1)?)?;
        let string = KuArc::<KuString>::try_from(inner.get(2)?)?;
        let slice = KuArc::<KuSlice>::try_from(inner.get(3)?)?;
        drop(inner);
        instance.close()?;
        (scalar, string, slice)
    };
    assert_eq!(scalar.value(), ScalarValue::I64(42));
    assert_eq!(string.as_bytes(), b"independent\0");
    assert_eq!(slice.spec(), SliceSpec::default());
    Ok(())
}

#[test]
fn checked_builtin_leaf_outlives_the_context_and_instance() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let scalar = {
        let instance = KuInstance::new(Vendor::Cpu)?;
        let scalar = {
            let add = instance.builtin("add")?;
            let mut context = instance.default_device().frame_context()?;
            let left: KuArc<KuObject<'_>> = KuArc::<KuScalar>::new(20_i64)?.into();
            let right: KuArc<KuObject<'_>> = KuArc::<KuScalar>::new(22_i64)?.into();
            // SAFETY: initialized scalar operands use non-overflowing addition.
            let mut outputs =
                unsafe { add.call_unchecked(&mut context, &[Some(&*left), Some(&*right)], &[]) }?;
            let output = outputs.pop().unwrap().unwrap();
            KuArc::<KuScalar>::try_from(output)?
        };
        instance.close()?;
        scalar
    };
    assert_eq!(scalar.value(), ScalarValue::I64(42));
    Ok(())
}

#[test]
fn downloaded_values_outlive_source_instance_wrapper_and_nested_array() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let values = {
        let temporary = instance.clone();
        let tensor = temporary
            .default_device()
            .tensor_from_slice(&[3], &[11_i64, 22, 33])?;
        let inner =
            KuArc::<KuArray>::new(&[tensor.into(), KuArc::<KuScalar>::new(42_i64)?.into()])?;
        let outer = KuArc::<KuArray>::new(&[inner.into()])?;
        let inner = KuArc::<KuArray>::try_from(outer.get(0)?)?;
        let tensor = KuArc::<KuTensor>::try_from(inner.get(0)?)?;
        assert!(tensor.is_on(&device));
        // The synchronous copy returns host values independent of these owners.
        device.to_vec::<i64>(&tensor)?
    };
    instance.close()?;
    assert_eq!(values, [11, 22, 33]);
    Ok(())
}

#[test]
fn empty_downloaded_values_outlive_local_device_and_tensor_views() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let values = {
        let device = instance.default_device();
        let tensor = device.zeros::<f64>(&[2, 0])?;
        assert!(tensor.is_on(&device));
        assert!(matches!(
            device.to_vec::<i64>(&tensor),
            Err(Error::TypeMismatch)
        ));
        device.to_vec::<f64>(&tensor)?
    };
    instance.close()?;
    assert!(values.is_empty());
    Ok(())
}
