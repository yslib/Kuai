//! Opt-in CUDA hardware coverage. This file compiles on CPU-only presets too.

use kurt::*;
use kurt_sys as sys;
use std::{ptr, sync::Mutex};

static CUDA: Mutex<()> = Mutex::new(());

fn vendor<'a>(device: &'a KuDevice<'_>) -> &'a sys::ku_vendor_api_t {
    let mut api = ptr::null();
    // SAFETY: the device borrows the live runtime. The returned table inherits
    // that borrow, including its context and vendor module function pointers.
    unsafe {
        assert_eq!(
            sys::ku_device_get_vendor_api(device.as_raw(), &mut api),
            sys::KU_STATUS_SUCCESS
        );
        &*api
    }
}

fn current_device(api: &sys::ku_vendor_api_t) -> i32 {
    let mut id = -1;
    // SAFETY: callers keep the owner of this table alive; output is writable.
    assert_eq!(
        unsafe { (api.get_device)(api.ctx, &mut id) },
        sys::KU_STATUS_SUCCESS
    );
    id
}

fn device_count(api: &sys::ku_vendor_api_t) -> i32 {
    let mut count = 0;
    // SAFETY: callers keep the owner of this table alive; output is writable.
    assert_eq!(
        unsafe { (api.get_device_count)(api.ctx, &mut count) },
        sys::KU_STATUS_SUCCESS
    );
    count
}

fn select_device(api: &sys::ku_vendor_api_t, id: i32) {
    // SAFETY: the live table's callers pass a device ID below its device count.
    assert_eq!(
        unsafe { (api.set_device)(api.ctx, id) },
        sys::KU_STATUS_SUCCESS
    );
}

fn stream_identity(device: &KuDevice) -> usize {
    let mut stream = ptr::null_mut();
    // SAFETY: both borrowed streams remain owned by the live runtime/vendor.
    // Integer addresses are used only for identity, never reconstructed.
    unsafe {
        assert_eq!(
            sys::ku_device_get_default_stream(device.as_raw(), &mut stream),
            sys::KU_STATUS_SUCCESS
        );
        assert!(!stream.is_null());
        let api = vendor(device);
        assert_ne!(stream, (api.get_per_thread_stream)(api.ctx));
    }
    stream as usize
}

#[test]
#[ignore = "requires CUDA-enabled preset and hardware"]
fn owned_default_stream_has_the_same_identity_across_host_threads() -> Result<()> {
    let _guard = CUDA.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cuda)?;
    let device = instance.default_device();
    let expected = stream_identity(&device);
    std::thread::scope(|scope| {
        let workers: Vec<_> = (0..4)
            .map(|_| scope.spawn(move || assert_eq!(stream_identity(&device), expected)))
            .collect();
        for worker in workers {
            worker.join().unwrap();
        }
    });
    instance.close()
}

#[test]
#[ignore = "requires CUDA-enabled preset and hardware"]
fn allocation_upload_download_and_free_restore_host_device_selection() -> Result<()> {
    let _guard = CUDA.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cuda)?;
    let device = instance.default_device();
    let tensor = std::thread::scope(|scope| {
        scope
            .spawn(move || -> Result<KuArc<KuTensor<'_>>> {
                let api = vendor(&device);
                let selected = if device_count(api) >= 2 { 1 } else { 0 };
                select_device(api, selected);
                let zeros = device.zeros::<i64>(&[4])?;
                assert_eq!(current_device(api), selected);
                drop(zeros);
                assert_eq!(current_device(api), selected);
                let tensor = device.tensor_from_slice(&[4], &[19_i64, 23, 29, 31])?;
                assert_eq!(current_device(api), selected);
                Ok(tensor)
            })
            .join()
            .unwrap()
    })?;
    std::thread::scope(|scope| {
        scope
            .spawn(move || -> Result<()> {
                let device = tensor.device();
                let api = vendor(&device);
                let selected = if device_count(api) >= 2 { 1 } else { 0 };
                select_device(api, selected);
                assert_eq!(device.to_vec::<i64>(&tensor)?, [19, 23, 29, 31]);
                assert_eq!(current_device(api), selected);
                drop(tensor);
                device.flush()?;
                assert_eq!(current_device(api), selected);
                Ok(())
            })
            .join()
            .unwrap()
    })?;
    instance.close()
}

#[test]
#[ignore = "requires CUDA-enabled preset and hardware"]
fn final_tensor_and_instance_release_restore_selection_on_another_thread() -> Result<()> {
    let _guard = CUDA.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cuda)?;
    let tensor = instance
        .default_device()
        .tensor_from_slice(&[2], &[37_i64, 41])?;
    std::thread::scope(|scope| {
        scope
            .spawn(move || {
                let device = tensor.device();
                let api = vendor(&device);
                let selected = if device_count(api) >= 2 { 1 } else { 0 };
                select_device(api, selected);
                drop(tensor);
                assert_eq!(current_device(api), selected);
            })
            .join()
            .unwrap()
    });
    // Dependent borrows have ended; the owning KuInstance can now move to an
    // unscoped thread and be destroyed there without losing selection coverage.
    std::thread::spawn(move || -> Result<()> {
        let selected = {
            let device = instance.default_device();
            let api = vendor(&device);
            let selected = if device_count(api) >= 2 { 1 } else { 0 };
            select_device(api, selected);
            selected
        };
        // This drops the final runtime owner. No
        // pointer or table borrowed from that runtime is used afterward.
        drop(instance);
        let next = KuInstance::new(Vendor::Cuda)?;
        let next_device = next.default_device();
        let next_api = vendor(&next_device);
        assert_eq!(current_device(next_api), selected);
        assert_eq!(
            next_device.to_vec::<i64>(&*next_device.zeros::<i64>(&[2])?)?,
            [0, 0]
        );
        assert_eq!(current_device(next_api), selected);
        next.close()
    })
    .join()
    .unwrap()
}

#[test]
#[ignore = "requires CUDA-enabled preset and hardware"]
fn mixed_device_nested_arrays_preserve_exact_devices() -> Result<()> {
    let _guard = CUDA.lock().unwrap();
    let probe = KuInstance::new(Vendor::Cuda)?;
    let count = device_count(vendor(&probe.default_device()));
    probe.close()?;
    if count < 2 {
        // The other CUDA tests still exercise all single-device requirements.
        return Ok(());
    }
    let instance = KuInstance::with_options(InstanceOptions {
        vendor: Vendor::Cuda,
        device_ids: vec![0, 1],
        ..InstanceOptions::default()
    })?;
    let array = {
        let first = instance.device(0)?;
        let second = instance.device(1)?;
        let tensor = second.tensor_from_slice(&[1], &[43_i64])?;
        assert!(matches!(
            first.to_vec::<i64>(&tensor),
            Err(Error::DeviceMismatch)
        ));
        let nested = KuArc::<KuArray>::new(&[tensor.into()])?;
        let array = KuArc::<KuArray>::new(&[
            first.tensor_from_slice(&[1], &[47_i64])?.into(),
            nested.into(),
        ])?;
        let clone = array.clone();
        assert_eq!(clone.as_raw(), array.as_raw());
        clone
    };
    let first = KuArc::<KuTensor>::try_from(array.get(0)?)?;
    let nested = KuArc::<KuArray>::try_from(array.get(1)?)?;
    drop(array);
    let second = KuArc::<KuTensor>::try_from(nested.get(0)?)?;
    drop(nested);
    assert_eq!(first.device().info()?, (DeviceType::Cuda, 0));
    assert_eq!(second.device().info()?, (DeviceType::Cuda, 1));
    assert_eq!(first.device().to_vec::<i64>(&first)?, [47]);
    assert_eq!(second.device().to_vec::<i64>(&second)?, [43]);
    assert!(matches!(
        first.device().to_vec::<i64>(&second),
        Err(Error::DeviceMismatch)
    ));
    drop(first);
    drop(second);
    instance.close()?;
    KuInstance::new(Vendor::Cuda)?.close()
}

#[test]
#[cfg(kuai_runtime_cpu)]
#[ignore = "requires release-all preset, CPU backend, and CUDA hardware"]
fn mixed_cpu_cuda_borrows_with_equal_numeric_ids_survive_parent_release() -> Result<()> {
    let _guard = CUDA.lock().unwrap();
    let cpu_instance = KuInstance::new(Vendor::Cpu)?;
    let cuda_instance = KuInstance::new(Vendor::Cuda)?;
    let array = {
        let cpu_device = cpu_instance.default_device();
        let cuda_device = cuda_instance.default_device();
        assert_eq!(cpu_device.info()?, (DeviceType::Cpu, 0));
        assert_eq!(cuda_device.info()?, (DeviceType::Cuda, 0));
        assert_ne!(cpu_device.as_raw(), cuda_device.as_raw());
        let nested = KuArc::<KuArray>::new(&[
            cpu_device.tensor_from_slice(&[2], &[53_i64, 59])?.into(),
            cuda_device.tensor_from_slice(&[2], &[61_i64, 67])?.into(),
        ])?;
        KuArc::<KuArray>::new(&[nested.into()])?
    };
    let nested = KuArc::<KuArray>::try_from(array.get(0)?)?;
    drop(array);
    let cpu = KuArc::<KuTensor>::try_from(nested.get(0)?)?;
    let cuda = KuArc::<KuTensor>::try_from(nested.get(1)?)?;
    drop(nested);
    assert_eq!(cpu.device().info()?, (DeviceType::Cpu, 0));
    assert_eq!(cuda.device().info()?, (DeviceType::Cuda, 0));
    assert_eq!(cpu.device().to_vec::<i64>(&cpu)?, [53, 59]);
    assert_eq!(cuda.device().to_vec::<i64>(&cuda)?, [61, 67]);
    assert!(matches!(
        cpu.device().to_vec::<i64>(&cuda),
        Err(Error::DeviceMismatch)
    ));
    assert!(matches!(
        cuda.device().to_vec::<i64>(&cpu),
        Err(Error::DeviceMismatch)
    ));
    drop(cpu);
    drop(cuda);
    cpu_instance.close()?;
    cuda_instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()?;
    KuInstance::new(Vendor::Cuda)?.close()
}
