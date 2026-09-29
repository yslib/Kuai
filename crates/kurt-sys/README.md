# kurt-sys

Raw Rust bindings to the Kurt C API. For a safe Rust interface, use
[`kurt`](../kurt/README.md).

## Build and run

Requires CMake 4.1+ and a C++26 compiler (MSVC uses `/std:c++latest`).
From the repository root:

```bash
cargo build
cargo test -p kurt-sys -p kurt
```

Cargo builds the static host and the default CPU plugin under its build output
directory. Use `cargo run` and `cargo test` for development; Cargo supplies the
plugin search path. Build scripts do not copy plugins beside executables.

The `cpu` and `cuda` features select plugins and may be combined. CUDA requires
Linux or Windows and the CUDA toolkit. See [kurt-cpp](../../kurt-cpp/README.md) for
the compiler/toolchain setup. To build and test only the host:

```bash
cargo test -p kurt-sys -p kurt --no-default-features
```

To consume an existing static host and matching plugins, set the standard
`CMAKE_INSTALL_PREFIX` to their installation prefix. When running through Cargo,
add the plugin directory to `LD_LIBRARY_PATH` (Linux), `DYLD_LIBRARY_PATH`
(macOS), or `PATH` (Windows).

## Install

```bash
cargo xtask install --prefix dist
```

This builds Release executables and installs them with the selected plugins in
`dist/bin`. Keep that directory together when deploying. Use `--features cuda`
to include CUDA, `--no-default-features` for host-only, or `--profile dev` for
Debug. CUDA runtime libraries and the NVIDIA driver remain required on the
runtime machine. `cargo install` alone does not install these plugins.

In the kurt-build CUDA image, use the existing toolchain and set the GPU target:

```bash
CMAKE_TOOLCHAIN_FILE="$PWD/kurt-cpp/cmake/toolchains/docker-cuda.cmake" \
CUDAARCHS=80 cargo xtask install --prefix dist --features cuda
```

Windows CUDA support is experimental; see the [toolchain setup](../../kurt-cpp/README.md#toolchains-and-presets).
Install CUDA Toolkit 13.2.0 and use an x64 Developer PowerShell with Ninja available:

```powershell
$env:CMAKE_GENERATOR = "Ninja"
$env:CMAKE_TOOLCHAIN_FILE = "$PWD/kurt-cpp/cmake/toolchains/windows-cuda-nvcc.cmake"
$env:CUDAARCHS = "80"
cargo xtask install --prefix dist --features cuda
```

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

```bash
cargo fmt -p kurt-sys --check
cargo clippy -p kurt-sys --all-targets --locked -- -D warnings
```

To check host-only operation without plugins in the runtime search path:

```bash
cargo test -p kurt-sys --no-default-features --test host_only --locked -- --ignored
```
