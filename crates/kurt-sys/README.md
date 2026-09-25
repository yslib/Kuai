# kurt-sys

Raw Rust bindings to the Kurt C API. For a safe Rust interface, use
[`kurt`](../kurt/README.md).

## Build and run

Build the host and optional CPU/CUDA plugins using the
[kurt-cpp instructions](../../kurt-cpp/README.md). Use the default static host
build. From the repository root, assemble an installation:

```bash
export CMAKE_INSTALL_PREFIX="$PWD/kurt-cpp/install/runtime"
cmake --install kurt-cpp/build/release --prefix "$CMAKE_INSTALL_PREFIX"
# For CPU support:
cmake --install kurt-cpp/build/release-cpu --prefix "$CMAKE_INSTALL_PREFIX"
```

For plugins, set the runtime library path for your platform:

```bash
# Linux
export LD_LIBRARY_PATH="$CMAKE_INSTALL_PREFIX/lib:${LD_LIBRARY_PATH:-}"
# macOS
export DYLD_LIBRARY_PATH="$CMAKE_INSTALL_PREFIX/lib:${DYLD_LIBRARY_PATH:-}"
```

Then build or test against that installation:

```bash
cargo build -p kurt-sys --locked
KUAI_RUNTIME_PRESET=release-cpu cargo test -p kurt-sys --locked
```

For host-only tests, point `CMAKE_INSTALL_PREFIX` to `kurt-cpp/install/release`
and use `KUAI_RUNTIME_PRESET=release`. The statically linked host needs no
runtime library path.

## Call the C API

Import the C names from `kurt_sys`. Check each status before reading outputs,
release owned handles, and keep instances alive while using their devices and
objects. Asynchronous buffers must remain valid until completion.

```rust
use std::mem::MaybeUninit;
use std::ptr;
use kurt_sys::*;

let input = ku_union_t {
    value: ku_union_value_t { i64: 42 },
    tag: KU_PRIMITIVE_I64,
};

// SAFETY: the descriptor and output slots are valid; the payload matches its
// tag. The owned object is released after reading its copied value.
unsafe {
    let mut object = ptr::null_mut();
    assert_eq!(ku_scalar_create(&input, &mut object), KU_STATUS_SUCCESS);
    let mut output = MaybeUninit::uninit();
    let status = ku_scalar_get_value(object, output.as_mut_ptr());
    assert_eq!(ku_object_release(object), KU_STATUS_SUCCESS);
    assert_eq!(status, KU_STATUS_SUCCESS);
    let output = output.assume_init();
    assert_eq!(output.tag, KU_PRIMITIVE_I64);
    assert_eq!(output.value.i64, 42);
}
```

See the [C headers](../../kurt-cpp/src/kuai_c/include/kuai/kuai_c) and generated
API documentation for individual function contracts:

```bash
cargo doc -p kurt-sys --no-deps --open
```

## Checks

With the runtime paths set as above:

```bash
cargo fmt -p kurt-sys --check
cargo clippy -p kurt-sys --all-targets --locked -- -D warnings
```

To check host-only operation, use the standalone host installation and run:

```bash
KUAI_RUNTIME_PRESET=release cargo test -p kurt-sys --test host_only --locked -- --ignored
```
