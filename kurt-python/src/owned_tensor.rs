use kurt::{KuArc, KuInstance, KuTensor, ScalarValue};

/// A value materialized while borrowing one facade-owned runtime instance.
pub(crate) enum BuiltValue<'i> {
    Tensor(KuArc<KuTensor<'i>>),
    Scalar(ScalarValue),
    String(String),
}

/// A facade value after its construction-time runtime owner has been handled.
pub(crate) enum OwnedValue {
    Tensor(OwnedTensor),
    Scalar(ScalarValue),
    String(String),
}

/// Keeps a runtime-dependent tensor and the boxed runtime passed to its builder.
///
/// The boxed `KuInstance` has a stable address before the builder runs. The
/// higher-ranked builder can return a tensor borrowing this lender, or one
/// with genuinely independent `'static` native backing. It cannot capture a
/// tensor borrowing an unrelated short-lived runtime. This type keeps the box
/// unchanged beside the tensor and exposes neither owner extraction nor mutable
/// access. Field order is part of the invariant: `tensor` drops before `owner`,
/// releasing the native tensor while its runtime wrapper still exists. The only
/// lifetime erasure below changes the view's marker after successful
/// construction; it does not move or clone the owner or adopt a raw object.
/// `KuInstance` clones share a runtime, but cannot rebind a view borrowed from
/// this particular wrapper.
pub(crate) struct OwnedTensor {
    tensor: KuArc<KuTensor<'static>>,
    // Retains the lender's runtime through tensor drop; field order matters.
    #[allow(dead_code)]
    owner: Box<KuInstance>,
}

impl OwnedTensor {
    /// Borrows the tensor for no longer than this owning adapter is borrowed.
    pub(crate) fn tensor<'a>(&'a self) -> &'a KuTensor<'a> {
        &self.tensor
    }
}

/// Builds a facade value while keeping a tensor's lending instance when needed.
pub(crate) fn build<E>(
    instance: KuInstance,
    builder: impl for<'i> FnOnce(&'i KuInstance) -> Result<BuiltValue<'i>, E>,
) -> Result<OwnedValue, E> {
    // Boxing precedes the borrow, so the lender's Rust address cannot change.
    let owner = Box::new(instance);
    // Consume the borrowed result inside this scope. For a tensor, the only
    // lifetime erasure occurs before the scope ends; scalar and string results
    // have no runtime borrow. The original borrowed enum is then gone before
    // the unchanged box moves into the adapter below.
    let value: BuiltValue<'static> = match builder(&owner)? {
        BuiltValue::Tensor(tensor) => {
            // SAFETY: The higher-ranked builder returns a tensor borrowing
            // `owner` or genuinely independent `'static` native backing, never
            // an unrelated short-lived runtime. The adapter retains the stable
            // box without mutation or escape, and drops tensor before owner.
            BuiltValue::Tensor(unsafe { erase_runtime_marker(tensor) })
        }
        BuiltValue::Scalar(value) => BuiltValue::Scalar(value),
        BuiltValue::String(value) => BuiltValue::String(value),
    };

    Ok(match value {
        BuiltValue::Tensor(tensor) => OwnedValue::Tensor(OwnedTensor { tensor, owner }),
        BuiltValue::Scalar(value) => OwnedValue::Scalar(value),
        BuiltValue::String(value) => OwnedValue::String(value),
    })
}

/// Changes only KuTensor's covariant runtime marker before `OwnedTensor`
/// couples the tensor to the boxed runtime passed to its builder.
unsafe fn erase_runtime_marker<'i>(tensor: KuArc<KuTensor<'i>>) -> KuArc<KuTensor<'static>> {
    // SAFETY: KuArc is an inline tensor view plus its native intrusive owner;
    // KuTensor's lifetime is solely PhantomData. The caller establishes that
    // the boxed lender remains in the same OwnedTensor until tensor drop;
    // independent backing must genuinely remain valid for `'static`.
    unsafe { std::mem::transmute::<KuArc<KuTensor<'i>>, KuArc<KuTensor<'static>>>(tensor) }
}

#[cfg(test)]
mod tests {
    use std::{convert::Infallible, panic, sync::Mutex};

    use kurt::{KuInstance, ScalarValue, Vendor};

    use super::{BuiltValue, OwnedValue, build};

    static CPU: Mutex<()> = Mutex::new(());

    #[test]
    fn tensor_keeps_cpu_runtime_alive_after_original_instance_drops() {
        let _guard = CPU.lock().unwrap();
        let original = KuInstance::new(Vendor::Cpu).unwrap();
        let value = build(original.clone(), |instance| {
            let tensor = instance
                .default_device()
                .tensor_from_slice(&[2], &[3_i64, 5])
                .unwrap();
            Ok::<_, Infallible>(BuiltValue::Tensor(tensor))
        })
        .unwrap();
        drop(original);

        let OwnedValue::Tensor(tensor) = value else {
            panic!("expected tensor");
        };
        let device = tensor.tensor().device();
        assert_eq!(device.to_vec::<i64>(tensor.tensor()).unwrap(), [3, 5]);

        drop(tensor);
        KuInstance::new(Vendor::Cpu).unwrap().close().unwrap();
    }

    #[test]
    fn scalar_drops_the_unused_cpu_owner() {
        let _guard = CPU.lock().unwrap();
        let value = build(KuInstance::new(Vendor::Cpu).unwrap(), |_| {
            Ok::<_, Infallible>(BuiltValue::Scalar(ScalarValue::I64(42)))
        })
        .unwrap();

        let OwnedValue::Scalar(value) = value else {
            panic!("expected scalar");
        };
        assert_eq!(value, ScalarValue::I64(42));
        KuInstance::new(Vendor::Cpu).unwrap().close().unwrap();
    }

    #[test]
    fn string_drops_the_unused_cpu_owner() {
        let _guard = CPU.lock().unwrap();
        let value = build(KuInstance::new(Vendor::Cpu).unwrap(), |_| {
            Ok::<_, Infallible>(BuiltValue::String("Kuai".to_owned()))
        })
        .unwrap();

        let OwnedValue::String(value) = value else {
            panic!("expected string");
        };
        assert_eq!(value, "Kuai");
        KuInstance::new(Vendor::Cpu).unwrap().close().unwrap();
    }

    #[test]
    fn builder_error_releases_its_last_cpu_owner() {
        let _guard = CPU.lock().unwrap();
        let result = build(KuInstance::new(Vendor::Cpu).unwrap(), |_| {
            Err::<BuiltValue<'_>, _>("expected builder failure")
        });

        assert!(matches!(result, Err("expected builder failure")));
        KuInstance::new(Vendor::Cpu).unwrap().close().unwrap();
    }

    #[test]
    fn builder_unwind_releases_its_last_cpu_owner() {
        let _guard = CPU.lock().unwrap();
        let result = panic::catch_unwind(|| {
            let _: Result<OwnedValue, ()> = build(KuInstance::new(Vendor::Cpu).unwrap(), |_| {
                panic!("expected builder panic")
            });
        });

        assert!(result.is_err());
        KuInstance::new(Vendor::Cpu).unwrap().close().unwrap();
    }
}
