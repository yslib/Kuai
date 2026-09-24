# kurt

Safe Rust access to Kurt values, tensors, and builtins.

## Build and test

Follow [kurt-sys setup](../kurt-sys/README.md) to install the native runtime and
set `CMAKE_INSTALL_PREFIX` and the runtime library path. From the repository root:

```bash
cargo build -p kurt --locked
KUAI_RUNTIME_PRESET=release-cpu cargo test -p kurt -p kurt-sys --locked
cargo doc -p kurt --no-deps --open
```

Use `KUAI_RUNTIME_PRESET=release` for host-only tests.

## Values and arrays

Create owned values with `KuArc`. Clone to share ownership, and use `TryFrom`
to recover a typed value from an array element:

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

## CPU tensors

Keep the `KuInstance` alive while using its devices and tensors.
`tensor_from_slice` uploads values; `to_vec` downloads them:

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

Use `Vendor::Cuda` after installing the CUDA plugin and its runtime dependencies.
Use scoped threads when sharing tensors across threads.

## Builtins

Look up a builtin with `instance.builtin(name)` and create a frame context on
the device. Before using `call_unchecked`, satisfy the callable's argument and
safety requirements:

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

## Checks

With the native runtime paths configured:

```bash
cargo fmt --all --check
KUAI_RUNTIME_PRESET=release-cpu cargo clippy -p kurt -p kurt-sys --all-targets --locked -- -D warnings
KUAI_RUNTIME_PRESET=release-cpu cargo test -p kurt --doc --locked
```

On Linux with a CUDA plugin, compatible NVIDIA driver and GPU:

```bash
KUAI_RUNTIME_PRESET=release-cuda cargo test -p kurt --test cuda_threading --locked -- --ignored
```

For tests combining CPU and CUDA, install both plugins and use
`KUAI_RUNTIME_PRESET=release-all` instead.
