#![cfg(kuai_runtime_cpu)]

use kuai_rt::*;
use std::sync::Mutex;

static CPU: Mutex<()> = Mutex::new(());

#[test]
fn safe_handles_are_send_and_sync() {
    fn both<T: Send + Sync>() {}
    both::<KuInstance>();
    both::<KuDevice>();
    both::<KuScalar>();
    both::<KuString>();
    both::<KuSlice>();
    both::<KuTensor>();
    both::<KuArray>();
    both::<KuObject>();
    both::<KuInstanceRef>();
    both::<KuArc<KuTensor>>();
    both::<KuArc<KuArray>>();
    both::<KuArc<KuObject>>();
}

#[test]
fn shared_tensor_downloads_and_concurrent_retain_release() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let device = instance.default_device();
    let tensor = device.tensor_from_slice(&[2], &[3_i64, 5])?;
    std::thread::scope(|scope| {
        for _ in 0..2 {
            let tensor = &tensor;
            let device = &device;
            scope.spawn(move || {
                for _ in 0..32 {
                    let clone = tensor.clone();
                    assert_eq!(device.to_vec::<i64>(&clone).unwrap(), [3, 5]);
                }
            });
        }
    });
    drop(tensor);
    instance.close()
}

#[test]
fn tensor_and_host_values_move_between_threads_within_runtime_borrow() -> Result<()> {
    let _guard = CPU.lock().unwrap();
    let instance = KuInstance::new(Vendor::Cpu)?;
    let tensor = {
        let device = instance.default_device();
        device.tensor_from_slice(&[2], &[7_i64, 11])?
    };
    let values = std::thread::scope(|scope| {
        scope
            .spawn(move || -> Result<Vec<i64>> {
                let device = tensor.device();
                let values = device.to_vec::<i64>(&tensor)?;
                drop(tensor);
                Ok(values)
            })
            .join()
            .unwrap()
    })?;
    assert_eq!(std::thread::spawn(move || values).join().unwrap(), [7, 11]);
    let tensor = {
        instance
            .default_device()
            .tensor_from_slice(&[2], &[13_i64, 17])?
    };
    // Native references may be released on another thread within the runtime borrow.
    std::thread::scope(|scope| scope.spawn(move || drop(tensor)).join().unwrap());
    instance.close()?;
    KuInstance::new(Vendor::Cpu)?.close()
}

#[test]
fn independent_leaf_owners_move_to_unscoped_threads() -> Result<()> {
    fn independent<T: Send + Sync + 'static>() {}
    independent::<KuArc<KuScalar>>();
    independent::<KuArc<KuString>>();
    independent::<KuArc<KuSlice>>();
    let scalar = KuArc::<KuScalar>::new(42_i64)?;
    let text = KuArc::<KuString>::new(b"independent\0")?;
    let slice = KuArc::<KuSlice>::new(SliceSpec::default())?;
    std::thread::spawn(move || {
        assert_eq!(scalar.value(), ScalarValue::I64(42));
        assert_eq!(text.as_bytes(), b"independent\0");
        assert_eq!(slice.spec(), SliceSpec::default());
    })
    .join()
    .unwrap();
    Ok(())
}
