# kurt

A safe Rust interface to `kurt-sys` with intrusive native ownership and borrowed
runtime dependencies. `KuArc<T>` owns one C reference; `T` is a lightweight Rust
view. Fallible operations return `Result`. The crate uses only the standard
library and the sys dependency.

```rust
use kurt::{KuArc, KuArray, KuScalar, KuString, ScalarValue};

let number = KuArc::<KuScalar>::new(42_i64)?;
let text = KuArc::<KuString>::new(b"Kuai\0runtime")?;
let array = KuArc::<KuArray>::new(&[number.clone().into(), text.into()])?;
assert_eq!(array.len(), 2);
let number = KuArc::<KuScalar>::try_from(array.get(0)?)?;
assert_eq!(number.value(), ScalarValue::I64(42));
# Ok::<(), kurt::Error>(())
```

## Native ownership and views

The C++ allocation holds the payload and intrusive reference count. A
`KuArc<T>` contains an inline Rust view of that allocation. `Deref` returns
`&self.view`; it does not reinterpret C++ memory as a Rust struct. There is no
Rust reference-count control block per object, no per-object KuInstance Arc, and
no list of runtime roots in a KuArray.

`KuScalar`, `KuString`, and `KuSlice` contain a native pointer. `KuTensor<'i>` and
`KuArray<'r>` add a lifetime marker for their runtime dependencies. `KuObject<'r>`
is an erased view containing only a private native pointer and lifetime marker,
without a cached tag or typed payload. `KuObjectKind` is the classification enum:
`Scalar`, `Tensor`, `Array`, `String`, and `Slice`.
`KuObjectKind` classifies the C API's supported object kinds; its name does not
imply ABI-layout equivalence with the C++ `KuObjectKind` RTTI wrapper.
Bare views have no public constructors, Clone, Copy, Default, or Drop.
Borrow them through `&*owner`; construct owners with `KuArc::<KuScalar>::new`,
`KuArc::<KuString>::new`, `KuArc::<KuSlice>::new`, and `KuArc::<KuArray>::new`.
KuDevice tensor factories also return KuArc owners.

`KuArc::clone()` and `KuArc::retain(&view)` each call `ku_object_retain` once.
Drop calls `ku_object_release` once and never panics on a diagnostic status.
Retaining copies only the sealed lightweight Rust view; it preserves the
view's runtime lifetime rather than tying the new owner to the short `&view`
borrow. It never copies tensor data or array elements. Public `NativeType` is
a sealed native view trait requiring `NativeObject` and `HasObjectKind`; KuArc
itself is not a NativeType.
No mutable dereference, safe raw adoption, or extraction of a bare owned view
is exposed.

`HasObjectKind` is an open, safe classification trait with `fn kind(&self) ->
KuObjectKind`. Ordinary Rust values may implement it; doing so does not establish
a native handle, prove that a native object's type matches the reported kind,
or justify unchecked casts. `NativeObject` and `NativeType` remain sealed.
Import `use kurt::HasObjectKind;` for `kind()` method syntax: these methods
are no longer inherent. Each concrete view returns its constant KuObjectKind
without a native query. Erased `KuObject` queries the native object every time,
then checks the status with a debug assertion and validates the supported kind.
`KuArc<T>` forwards classification to its stored view without an extra retain,
release, or allocation.

Generic code can classify both owners and borrowed views:

```rust
use kurt::{HasObjectKind, KuArc, KuObjectKind, KuScalar};

fn kind_of<T: HasObjectKind + ?Sized>(value: &T) -> KuObjectKind { value.kind() }

let scalar = KuArc::<KuScalar>::new(42_i64)?;
assert_eq!(kind_of(&scalar), KuObjectKind::Scalar);
assert_eq!(kind_of(&*scalar), KuObjectKind::Scalar);
# Ok::<(), kurt::Error>(())
```

Consuming conversions between typed owners and `KuArc<KuObject<'r>>` transfer
the existing reference without retain/release. A wrong-kind `TryFrom` returns
`TypeMismatch` and releases the consumed reference.
Match `object.kind()` to choose a checked conversion;
matching the classification does not refine the Rust type of `object`.
Calling `kind()` then `TryFrom` may query the native kind twice. There is no
unsafe shortcut in this safe API. Clone before conversion only when the
original owner is still needed, or retain the erased view explicitly:

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

## Instances, devices, and arrays

`KuInstance` is the explicit runtime owner. KuInstance clones share its one
`Arc<InstanceInner>`. `close(self)` reports native shutdown errors when it is
the last owner; another KuInstance clone causes `ResourcesInUse`. Ordinary Drop
releases one KuInstance owner. KuDevice and object views borrow a KuInstance
wrapper, so Rust prevents that wrapper from being dropped or closed while dependent
owners or views are still needed. Only one native KuInstance per vendor may be
active at a time; after destruction, that vendor can be initialized again.

`KuInstance::default_device()` returns an infallible `KuDevice<'i>`; `device(id)`
is fallible. KuDevice is Copy and carries only its native pointer and runtime
borrow. Neither copying it nor looking it up retains the runtime or allocates
another native device. Factories preserve `'i`, not the short borrow of the
KuDevice variable used for the call.

`KuTensor::device()` queries the tensor's actual native KuDevice and returns
`KuDevice<'i>`. `KuDevice::instance()` queries its exact native owner and returns a
Copy `KuInstanceRef<'i>`. Neither query takes a KuInstance argument, creates a
KuInstance owner, retains, allocates, or synchronizes. `KuInstance::as_raw()` and
`KuInstanceRef::as_raw()` expose the corresponding borrowed native identity.
Numeric device IDs alone do not establish identity; device-specific transfers
reject `DeviceMismatch` when native KuDevice pointers differ.

The native KuArray stores and retains its native object elements. Rust creates
only a temporary pointer list during `KuArc::<KuArray<'r>>::new`; it does not
keep Rust owners or runtime roots in the resulting KuArray. `KuArray::get` returns
an independently owned `KuArc<KuObject<'r>>`, valid after its parent KuArray drops.
Its runtime lifetime remains the array's conservative common lifetime.

Temporary KuDevice and KuArray variables may end before an extracted tensor:

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

Arrays accept mixed devices and KuInstance owners. All transitive dependencies
must remain alive for their common lifetime, including those of nested arrays.
Extracting a nested KuArray conservatively preserves that lifetime even when it
happens to contain only leaves. Empty arrays and arrays constructed entirely
from independent leaves need no KuInstance.

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

Downcasting an erased KuObject to an independent KuScalar, KuString, or KuSlice
removes its conservative runtime lifetime. Retaining an erased view alone keeps
that lifetime; the checked leaf conversion establishes independence:

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

An explicit KuInstance clone keeps the runtime alive, but does not rebind an
existing view's borrow to a different Rust wrapper. Create views from the
wrapper that will remain alive:

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

## Synchronous tensors and typed access

`KuDevice::tensor_from_slice(shape, values)`, `zeros`, and `to_vec` are synchronous.
Construction checks rank, extents, element counts, and byte sizes before native
submission. It borrows the input slice directly, immediately waits for the
native completion, releases it, and returns an initialized tensor owner.
`zeros` supplies initialized host storage through the same path. `to_vec`
checks exact KuDevice identity, dtype, layout, and sizes, copies into an
initialized Vec, then waits before returning it. It borrows the source tensor
without cloning or retaining it. Empty downloads skip copying after validation.

Internal guards wait before borrowed buffers or unpublished tensor references
can be cleaned up, including errors and unwinding, and release the completion
exactly once. Safe Rust exposes no async transfers or completion objects.
Native/sys asynchronous APIs remain available through `kurt-sys`.

```no_run
use kurt::{KuInstance, Vendor};

let instance = KuInstance::new(Vendor::Cpu)?;
let device = instance.default_device();
let mut input = vec![3_i64, 5];
let tensor = device.tensor_from_slice(&[2], &input)?;
input.fill(0);
drop(input);
assert_eq!(device.to_vec::<i64>(&tensor)?, [3, 5]);
# Ok::<(), kurt::Error>(())
```

The sealed Element types are `i8`, `i16`, `i32`, `i64`, `f32`, `f64`, and
`Boolean`. Rust `bool` is not byte-compatible with the runtime's three-state
boolean. ScalarValue preserves every primitive kind, including NONE.
KuString preserves arbitrary bytes, including invalid UTF-8 and embedded NUL.
`as_bytes()` borrows those bytes; fallible `to_str()` validates UTF-8. SliceSpec
uses optional bounds and `NonZeroI64` for an explicit step.

KuArray length, KuTensor length/device/metadata, KuScalar value, KuString bytes,
KuSlice spec, KuObject kind, and KuDevice instance queries are infallible for valid
typed views. Construction and checked native-kind classification establish
their immutable initialized payloads and runtime lifetimes. Each native query
executes before a debug assertion checks its success status; release builds
rely on that invariant. Real failures remain fallible: allocation, unsupported
shapes, array bounds, type/device mismatches, transfers, and UTF-8 validation.

KuTensor metadata copies dtype, shape, and element strides. These constructors
produce contiguous column-major tensors, and `to_vec` requires that layout;
other tensors, including builtin results, may have different strides. Rank-zero
tensors have empty shape/stride vectors and one element; any zero extent makes
a tensor empty. No safe host reference to device memory is exposed.
External-memory import and DLPack are not provided.

## Threads and native interoperation

KuInstance, KuDevice, KuInstanceRef, native views, and their KuArc owners support
Send and Sync. Runtime-dependent lifetimes still apply: use scoped threads
while the KuInstance remains outside the scope. Clone retains only the native
object, so it cannot turn a runtime borrow into a `'static` owner.

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

Independent leaves can move to ordinary unscoped threads:

```rust
use kurt::{KuArc, KuScalar, ScalarValue};
let scalar = KuArc::<KuScalar>::new(42_i64)?;
std::thread::spawn(move || assert_eq!(scalar.value(), ScalarValue::I64(42)))
    .join().unwrap();
# Ok::<(), kurt::Error>(())
```

Every native KuDevice owns a stable explicit default stream, distinct from the
vendor's per-thread stream. Transfers depend on that stream. Concurrent host
submissions need caller synchronization if a particular order is required;
Send and Sync alone do not define that order.

`NativeObject` is sealed and implemented by all views and KuArc owners. Its
`as_raw()` borrows a native object without retaining or transferring ownership:

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

Raw pointers erase Rust's borrow information. Keep the native owner and all
runtime dependencies alive while using them and obey each unsafe FFI contract.
An independent C retain keeps an object reference alive, but never extends its
runtime lifetime. KuDevice and KuInstance use their separate native handle APIs.

## Builtins and frame contexts

The builtin concept is language-independent: a builtin may be implemented in
Rust or C++. `KuCCall` describes a callable normalized through the C ABI, not
its implementation language. `KuInstance::builtin()` returns
`Result<KuCCall<'_>>`, and `builtin_names()` lists available builtins. This
rename adds no Rust-native registration or execution API; a future Rust-native
builtin abstraction need not use `KuCCall`.

`KuFrameContext<'r>` owns its native frame context and borrows `KuDevice<'r>`.
`device()` lends that KuDevice; native frame destruction occurs while its runtime
is live. `KuCCall<'i>` borrows its callable's KuInstance. Both remain explicitly
`!Send` and `!Sync`.

Lookup and frame creation are safe. `KuCCall::call_unchecked` remains unsafe:
the caller must establish semantic preconditions such as index bounds,
supported shapes, and arithmetic domains. The callable must not mutate
immutable arguments, retain borrowed arguments beyond the synchronized default
stream work, or return uninitialized data. Every result must be a valid
materializable object whose transitive device resources belong to the context's
KuInstance. Checking the C frame layout cannot establish those guarantees for
arbitrary vendor code.

The wrapper checks exact native KuInstance identity between the callable and
context. Argument device compatibility is interpreted by the runtime; the wrapper
neither compares tensor devices with the context nor traverses nested arrays for
device validation. It manages result slots and native references and synchronizes
the context's default-stream work on success and failure. `None` is a nullable C
argument/result, distinct from ScalarValue::None.
Results are `KuArc<KuObject<'r>>` with the context's runtime lifetime; they need
not borrow the local arguments, context variable, or callable:

```no_run
use kurt::{KuArc, KuCCall, KuInstance, KuObject, KuTensor, Vendor};

let instance = KuInstance::new(Vendor::Cpu)?;
let result = {
    let mut context = instance.default_device().frame_context()?;
    let add: KuCCall<'_> = instance.builtin("add")?;
    let left: KuArc<KuObject<'_>> = instance.default_device().tensor_from_slice(&[2], &[1_i64, 2])?.into();
    let right: KuArc<KuObject<'_>> = instance.default_device().tensor_from_slice(&[2], &[10_i64, 20])?.into();
    // SAFETY: matching initialized integer tensors use non-overflowing addition;
    // this builtin neither mutates nor retains borrowed arguments after the call.
    let mut outputs = unsafe { add.call_unchecked(&mut context, &[Some(&*left), Some(&*right)], &[]) }?;
    outputs.pop().unwrap().unwrap()
};
let tensor = KuArc::<KuTensor>::try_from(result)?;
assert_eq!(tensor.device().to_vec::<i64>(&tensor)?, [11, 22]);
drop(tensor);
instance.close()?;
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

## Migration and verification

The runtime object API uses these names, with no compatibility aliases for the
previous names. Ownership, lifetimes, and method names are unchanged.

| Previous name | Current name |
| --- | --- |
| `Object<'r>` | `KuObject<'r>` |
| `Tensor<'i>` | `KuTensor<'i>` |
| `Array<'r>` | `KuArray<'r>` |
| `Scalar` | `KuScalar` |
| `ByteString` | `KuString` |
| `Slice` | `KuSlice` |
| `Instance` | `KuInstance` |
| `InstanceRef<'i>` | `KuInstanceRef<'i>` |
| `Device<'i>` | `KuDevice<'i>` |
| `FrameContext<'r>` | `KuFrameContext<'r>` |
| `Builtin<'i>` | `KuCCall<'i>` |
| `ValueKind` | `KuObjectKind` |
| `HasValueKind` | `HasObjectKind` |

`KuArc`, helper types, and sealed native capability traits keep their names.
The `Ku` prefix does not make every type a native view or a valid `KuArc`
payload. Classification variants and standard-library `String` also keep their
names.

Use KuArc owners instead of constructing or cloning bare views. Convert typed
owners with `.into()` and `KuArc::<Typed>::try_from(object)`. Import
`use kurt::HasObjectKind;` because `kind()` is now a trait method rather than
an inherent method. `NativeType` is a sealed native view trait requiring
`HasObjectKind`; classification alone does not grant native view or handle access.
Match `object.kind()` for classification, then use a checked conversion for payload
access. `KuArc::retain(&*object)` creates another erased owner. Keep a KuInstance
alive around its borrowed KuDevice, KuTensor, KuObject, KuArray, and KuFrameContext;
queries recover native identity, not runtime ownership. Use synchronous
`tensor_from_slice`, `zeros`, and `to_vec` for safe transfers.

The sys crate controls CMake. Its default preset is `release-cpu`; existing
`KUAI_RUNTIME_*` environment variables apply:

```bash
KUAI_RUNTIME_BUILD_MODE=local KUAI_RUNTIME_PRESET=release-cpu cargo test -p kurt -p kurt-sys --locked -j 8
KUAI_RUNTIME_BUILD_MODE=local KUAI_RUNTIME_PRESET=release-cpu cargo test -p kurt -p kurt-sys --locked --release -j 8
KUAI_RUNTIME_BUILD_MODE=local KUAI_RUNTIME_PRESET=release-cpu cargo test -p kurt --doc --locked -- --show-output
cargo fmt --all --check
KUAI_RUNTIME_BUILD_MODE=local KUAI_RUNTIME_PRESET=release-cpu cargo clippy -p kurt -p kurt-sys --all-targets --locked -- -D warnings
```

Runtime examples marked `no_run` are compiled, not executed by rustdoc. CPU
integration tests execute native retention/conversion, queried identity,
temporary-parent lifetimes, nested arrays, builtin outputs, synchronous host
buffer safety, and scoped threading. They run for CPU-enabled presets
(`debug-cpu`, `release-cpu`, `release-all`); independent value tests and doctests
also work on CUDA-only builds without hardware.

`tests/cuda_threading.rs` compiles on CPU-only presets. Four CUDA hardware tests
are ignored by default: default-stream identity, transfer selection restoration,
cross-thread tensor and final KuInstance destruction, and mixed-device nested
arrays. The mixed-device test requires two GPUs for its two-device assertions
and returns early on one GPU. A fifth ignored test, compiled with CPU support,
combines CPU and CUDA dependencies with numeric device ID 0 and verifies exact
identity and cross-vendor transfer rejection after parent arrays are released.
A sixth ignored test requires CPU and CUDA support and verifies that builtin
arguments from another device, including nested arrays, reach native argument
validation instead of being rejected by the wrapper.

On Linux with a compatible CUDA toolchain and hardware, run:

```bash
KUAI_RUNTIME_BUILD_MODE=local KUAI_RUNTIME_PRESET=release-cuda cargo test -p kurt --test cuda_threading --locked -- --ignored
KUAI_RUNTIME_BUILD_MODE=local KUAI_RUNTIME_PRESET=release-all cargo test -p kurt --test cuda_threading --locked -- --ignored
```

An unavailable backend fails an explicitly requested hardware run. CPU-only
tests do not validate CUDA execution or operations across multiple vendors.
