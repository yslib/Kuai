# API doctests

## Classify owners and views

```rust
use kurt::{HasObjectKind, KuArc, KuObjectKind, KuScalar};

fn kind_of<T: HasObjectKind + ?Sized>(value: &T) -> KuObjectKind { value.kind() }

let scalar = KuArc::<KuScalar>::new(42_i64)?;
assert_eq!(kind_of(&scalar), KuObjectKind::Scalar);
assert_eq!(kind_of(&*scalar), KuObjectKind::Scalar);
# Ok::<(), kurt::Error>(())
```

## Retain an erased object

```rust
use kurt::{HasObjectKind, KuArc, KuObject, KuObjectKind, KuScalar, NativeObject, ScalarValue};

let original = KuArc::<KuScalar>::new(42_i64)?;
let raw = original.as_raw();
let object: KuArc<KuObject<'static>> = original.into();
let retained = KuArc::retain(&*object);
drop(object);
let retained = match retained.kind() {
    KuObjectKind::Scalar => KuArc::<KuScalar>::try_from(retained)?,
    _ => unreachable!(),
};
assert_eq!(retained.as_raw(), raw);
assert_eq!(retained.value(), ScalarValue::I64(42));
# Ok::<(), kurt::Error>(())
```

## Extract a tensor from a temporary array

```no_run
use kurt::{KuArray, KuInstance, KuArc, KuTensor, Vendor};

let instance = KuInstance::new(Vendor::Cpu)?;
let tensor = {
    let original = instance.default_device().tensor_from_slice(&[2], &[3_i64, 5])?;
    let array = KuArc::<KuArray>::new(&[original.into()])?;
    KuArc::<KuTensor>::try_from(array.get(0)?)?
};
let device = tensor.device();
let owner = device.instance();
assert_eq!(owner.as_raw(), instance.as_raw());
assert_eq!(device.to_vec::<i64>(&tensor)?, [3, 5]);
drop(tensor);
let another = device.zeros::<i64>(&[1])?;
assert_eq!(device.to_vec::<i64>(&another)?, [0]);
drop(another);
instance.close()?;
# Ok::<(), kurt::Error>(())
```

## Use multiple vendors in an array

```no_run
use kurt::{KuArray, KuInstance, KuArc, Vendor};

let cpu = KuInstance::new(Vendor::Cpu)?;
let cuda = KuInstance::new(Vendor::Cuda)?;
let array = KuArc::<KuArray>::new(&[
    cpu.default_device().zeros::<i64>(&[1])?.into(),
    cuda.default_device().zeros::<i64>(&[1])?.into(),
])?;
assert_eq!(array.len(), 2);
drop(array);
cpu.close()?;
cuda.close()?;
# Ok::<(), kurt::Error>(())
```

## Extract an independent scalar

```no_run
use kurt::{KuArray, KuInstance, KuArc, KuScalar, ScalarValue, Vendor};

let scalar = {
    let instance = KuInstance::new(Vendor::Cpu)?;
    let array = KuArc::<KuArray>::new(&[
        instance.default_device().zeros::<i64>(&[1])?.into(),
        KuArc::<KuScalar>::new(42_i64)?.into(),
    ])?;
    let scalar = KuArc::<KuScalar>::try_from(array.get(1)?)?;
    drop(array);
    instance.close()?;
    scalar
};
assert_eq!(scalar.value(), ScalarValue::I64(42));
# Ok::<(), kurt::Error>(())
```

## Use an instance clone

```no_run
use kurt::{Error, KuInstance, Vendor};

let instance = KuInstance::new(Vendor::Cpu)?;
let remaining = instance.clone();
assert_eq!(instance.close(), Err(Error::ResourcesInUse));
let tensor = remaining.default_device().tensor_from_slice(&[2], &[7_i64, 11])?;
assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [7, 11]);
drop(tensor);
remaining.close()?;
# Ok::<(), kurt::Error>(())
```

## Share tensors in scoped threads

```no_run
use kurt::{KuInstance, Vendor};

let instance = KuInstance::new(Vendor::Cpu)?;
let tensor = instance.default_device().tensor_from_slice(&[2], &[3_i64, 5])?;
std::thread::scope(|scope| {
    scope.spawn(|| {
        let retained = tensor.clone();
        assert_eq!(retained.device().to_vec::<i64>(&retained).unwrap(), [3, 5]);
    });
});
drop(tensor);
instance.close()?;
# Ok::<(), kurt::Error>(())
```

## Move independent values between threads

```rust
use kurt::{KuArc, KuScalar, ScalarValue};
let scalar = KuArc::<KuScalar>::new(42_i64)?;
std::thread::spawn(move || assert_eq!(scalar.value(), ScalarValue::I64(42)))
    .join().unwrap();
# Ok::<(), kurt::Error>(())
```

## Borrow native handles

```rust
use kurt::{KuArc, KuScalar, NativeObject};
fn native_kind<T: NativeObject + ?Sized>(value: &T) -> i32 {
    let mut kind = 0;
    // SAFETY: the value and its dependencies stay borrowed through the query.
    let status = unsafe { kurt_sys::ku_object_get_kind(value.as_raw(), &mut kind) };
    assert_eq!(status, kurt_sys::KU_STATUS_SUCCESS);
    kind
}
let scalar = KuArc::<KuScalar>::new(42_i64)?;
assert_eq!(native_kind(&scalar), kurt_sys::KU_OBJECT_SCALAR);
assert_eq!(native_kind(&*scalar), kurt_sys::KU_OBJECT_SCALAR);
# Ok::<(), kurt::Error>(())
```

## Compile-time boundaries

A KuTensor owner cannot escape its borrowed KuInstance:

```compile_fail,E0597
use kurt::{KuInstance, Vendor};
let tensor = {
    let instance = KuInstance::new(Vendor::Cpu).unwrap();
    instance.default_device().zeros::<i64>(&[1]).unwrap()
};
println!("{}", tensor.len());
```

A KuArray containing that tensor preserves the same dependency:

```compile_fail,E0597
use kurt::{KuArray, KuInstance, KuArc, Vendor};
let array = {
    let instance = KuInstance::new(Vendor::Cpu).unwrap();
    let tensor = instance.default_device().zeros::<i64>(&[1]).unwrap();
    KuArc::<KuArray>::new(&[tensor.into()]).unwrap()
};
println!("{}", array.len());
```

Queried KuDevice and KuInstanceRef views cannot escape either:

```compile_fail,E0597
use kurt::{KuInstance, Vendor};
let device = {
    let instance = KuInstance::new(Vendor::Cpu).unwrap();
    let tensor = instance.default_device().zeros::<i64>(&[1]).unwrap();
    tensor.device()
};
println!("{:?}", device.info());
```

```compile_fail,E0597
use kurt::{KuInstance, Vendor};
let owner = {
    let instance = KuInstance::new(Vendor::Cpu).unwrap();
    instance.default_device().instance()
};
println!("{:?}", owner.as_raw());
```

Implicit KuArc destruction still needs the runtime, even without a later read:

```compile_fail,E0505
use kurt::{KuInstance, Vendor};
let instance = KuInstance::new(Vendor::Cpu).unwrap();
let _tensor = instance.default_device().zeros::<i64>(&[1]).unwrap();
drop(instance);
```

Nested arrays with CPU and CUDA objects cannot outlive the CUDA dependency or
the CPU dependency. These examples are compile-only and do not require hardware:

```compile_fail,E0597
use kurt::{KuArray, KuInstance, KuArc, Vendor};
let cpu = KuInstance::new(Vendor::Cpu).unwrap();
let outer = {
    let cuda = KuInstance::new(Vendor::Cuda).unwrap();
    let inner = KuArc::<KuArray>::new(&[
        cpu.default_device().zeros::<i64>(&[1]).unwrap().into(),
        cuda.default_device().zeros::<i64>(&[1]).unwrap().into(),
    ]).unwrap();
    KuArc::<KuArray>::new(&[inner.into()]).unwrap()
};
println!("{}", outer.len());
```

```compile_fail,E0597
use kurt::{KuArray, KuInstance, KuArc, Vendor};
let cuda = KuInstance::new(Vendor::Cuda).unwrap();
let outer = {
    let cpu = KuInstance::new(Vendor::Cpu).unwrap();
    let inner = KuArc::<KuArray>::new(&[
        cpu.default_device().zeros::<i64>(&[1]).unwrap().into(),
        cuda.default_device().zeros::<i64>(&[1]).unwrap().into(),
    ]).unwrap();
    KuArc::<KuArray>::new(&[inner.into()]).unwrap()
};
println!("{}", outer.len());
```

Bare views cannot be moved out of a borrowed owner or mutably dereferenced:

```compile_fail,E0507
use kurt::{KuArc, KuScalar};
let owner = KuArc::<KuScalar>::new(42_i64).unwrap();
let view = *owner;
println!("{:?}", view.value());
```

```compile_fail,E0596
use kurt::{KuArc, KuScalar};
let mut owner = KuArc::<KuScalar>::new(42_i64).unwrap();
let _: &mut KuScalar = &mut *owner;
```

NativeType remains sealed even for values implementing HasObjectKind, and an
owner is not itself a native view:

```compile_fail,E0277
struct Fake;
impl kurt::HasObjectKind for Fake {
    fn kind(&self) -> kurt::KuObjectKind { kurt::KuObjectKind::Scalar }
}
impl kurt::NativeType for Fake {}
```

```compile_fail,E0277
use kurt::{KuArc, KuScalar};
let _: Option<KuArc<KuArc<KuScalar>>> = None;
```

Runtime-dependent owners cannot be moved into a thread requiring `'static`:

```compile_fail,E0597
use kurt::{KuInstance, Vendor};
let instance = KuInstance::new(Vendor::Cpu).unwrap();
let tensor = instance.default_device().zeros::<i64>(&[1]).unwrap();
std::thread::spawn(move || tensor.len()).join().unwrap();
```

Another KuInstance clone does not rebind an existing borrow:

```compile_fail,E0505
use kurt::{KuInstance, Vendor};
let instance = KuInstance::new(Vendor::Cpu).unwrap();
let remaining = instance.clone();
let tensor = instance.default_device().zeros::<i64>(&[1]).unwrap();
drop(instance);
assert_eq!(tensor.len(), 1);
drop(tensor);
remaining.close().unwrap();
```

Borrowed bytes cannot escape their native owner:

```compile_fail,E0597
use kurt::{KuArc, KuString};
let bytes = {
    let value = KuArc::<KuString>::new("temporary").unwrap();
    value.as_bytes()
};
println!("{bytes:?}");
```

Downstream code cannot invent element layouts or native object capabilities:

```compile_fail,E0277
#[derive(Clone, Copy, Default)]
struct Invalid(bool);
impl kurt::Element for Invalid {
    const TYPE: kurt::PrimitiveType = kurt::PrimitiveType::Boolean;
}
```

```compile_fail,E0277
struct Fake;
impl kurt::HasObjectKind for Fake {
    fn kind(&self) -> kurt::KuObjectKind { kurt::KuObjectKind::Scalar }
}
impl kurt::NativeObject for Fake {
    fn as_raw(&self) -> kurt_sys::ku_object_t { std::ptr::null_mut() }
}
```

```compile_fail,E0277
fn needs_native<T: kurt::NativeObject + ?Sized>(_: &T) {}
needs_native(&String::from("not a runtime string"));
```

KuCCall keeps its KuInstance borrow:

```compile_fail,E0597
use kurt::{KuInstance, Vendor};
let builtin = {
    let instance = KuInstance::new(Vendor::Cpu).unwrap();
    instance.builtin("add").unwrap()
};
println!("{builtin:?}");
```

Frame contexts and builtins remain thread-confined:

```compile_fail,E0277
fn needs_send<T: Send>() {}
needs_send::<kurt::KuFrameContext<'static>>();
```

```compile_fail,E0277
fn needs_sync<T: Sync>() {}
needs_sync::<kurt::KuFrameContext<'static>>();
```

```compile_fail,E0277
fn needs_send<T: Send>() {}
needs_send::<kurt::KuCCall<'static>>();
```

```compile_fail,E0277
fn needs_sync<T: Sync>() {}
needs_sync::<kurt::KuCCall<'static>>();
```

