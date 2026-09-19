# kuai-sys

`kuai-sys` is the unsafe Rust adapter for the adjacent, independently
buildable `kuai-runtime` C++ project. Its build script invokes the runtime's
CMake presets and links the installed `libkurt` shared library.

A plain Cargo build uses the local toolchain and the `release-cpu` preset:

```bash
cargo build -p kuai-sys
```

The build can be configured with these environment variables:

- `KUAI_RUNTIME_PRESET` selects a runtime CMake preset. Supported values are
  `debug`, `debug-cpu`, `release`, `release-all`, `release-cpu`,
  `release-cuda`, and `release-cuda-nvcc`.
- `KUAI_RUNTIME_BUILD_MODE` is either `local` (the default) or `docker`.
- `KUAI_RUNTIME_DOCKER_IMAGE` supplies the toolchain image and is required in
  Docker mode.
- Cargo's `NUM_JOBS` controls CMake build parallelism.

For example:

```bash
KUAI_RUNTIME_BUILD_MODE=docker \
KUAI_RUNTIME_DOCKER_IMAGE=ghcr.io/example/kuai-runtime-base:tag \
KUAI_RUNTIME_PRESET=release-cpu \
cargo test -p kuai-sys
```

Build and install artifacts are isolated below Cargo's `OUT_DIR`; the C++
project's own `build/` and `install/` directories are not used by Cargo.

## Bindings

The crate exposes the current public C API from
`kuai-runtime/src/kuai_c/include/kuai/kuai_c/`: instance and device management,
vendor and scheduler callback tables, frame contexts, objects, scalars,
strings, arrays, slices, completions, tensors, and builtin lookup and calls.
Kuai declarations are re-exported at the crate root. The bundled DLPack 1.3
types and constants live in `kuai_sys::dlpack`.

These are raw, unsafe bindings for a separate safe crate to build on. Integer
enums use their C integer types and named constants; structs and unions use
`#[repr(C)]`. C names, field order, pointer mutability, and function signatures
follow the public headers. Macros define opaque handles, integer constants,
and primitive payloads. No runtime dependencies or binding generator are required.

Required callbacks use `unsafe extern "C" fn(...)` directly. Slots that permit
null use `*const c_void`: `ku_scheduler_t.submit`, `ku_builtin_destroy_t`, the
DLPack deleters, and `DLPackDLTensorFromPyObjectNoSync`. Their documented type
aliases preserve the callable signatures. Set an absent callback with
`std::ptr::null()`; install a callback by casting its typed function pointer
to `*const c_void`. Before calling a raw callback, check for null and explicitly
convert it to the matching signature with `std::mem::transmute`.

Raw callback pointers point to code directly, not to a stored function pointer.
This representation relies on function and data pointers sharing the ABI on
the runtime's Linux targets. Rust function pointers themselves cannot contain
null; see the [Rust function pointer documentation](https://doc.rust-lang.org/std/primitive.fn.html#casting-to-and-from-integers).
Null checks, ownership, and safe calling interfaces belong in the higher-level crate.

The anonymous C payload union has the Rust name `ku_union_value_t`, accessed
through `ku_union_t.value`. Its active field must match `ku_union_t.tag`.
`ku_bool_t` is the runtime's three-state byte, not Rust's `bool`.
`ku_builtin_registration_t.match` is spelled `r#match` in Rust.

```rust
use std::mem::MaybeUninit;
use std::ptr;
use kuai_sys::*;

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

## Ownership and asynchronous calls

- Copying a raw handle or C descriptor does not retain it. Objects and
  completions require one release for each acquired reference. Instances and
  frame contexts instead have unique create/destroy lifecycles.
- Devices, builtin names, resolved call targets, and vendor tables borrow
  from the instance. String views borrow from their string object. Destroy
  frame contexts and device-dependent resources before destroying the instance.
- Direct input and output pointers must be valid and non-null unless a C API
  explicitly permits null. In particular, empty string/array inputs and empty
  tensor host buffers still need non-null pointers. `MaybeUninit` is suitable
  for output slots; inspect them only after the documented return status.
- Host buffers used by asynchronous copies must remain valid and free of
  conflicting accesses until completion. Do not observe a newly uploaded
  tensor until its completion reports success.
- Completion callbacks can run on worker threads, or immediately if already
  complete. They must not unwind. Waiting for completion does not wait for a
  callback to return; keep callback state alive independently.
- An exported DLPack descriptor owns a reference and must be passed to its
  deleter exactly once. Import consumes the descriptor only on success. The
  current runtime imports only CUDA device 0; CPU export works, while CPU
  import returns `KU_STATUS_NOT_SUPPORTED` and leaves ownership with the caller.
- `kuVendorModule` is a vendor-module entry point exported by
  `libkurt_<vendor>`, not by the linked `libkurt`. Ordinary runtime callers use
  `ku_instance_init` to load a backend; directly using the entry point requires
  resolving/linking the vendor module and supplying a valid host handle.

## Tests

```bash
cargo test -p kuai-sys --locked
cargo fmt -p kuai-sys --check
cargo clippy -p kuai-sys --all-targets --locked -- -D warnings
```

Value tests exercise the linked runtime on every preset. CPU integration tests
are enabled for `debug-cpu`, `release-cpu`, and `release-all`, and serialize
instance lifecycles because only one instance per vendor may be active. They
cover device capabilities, vendor memory operations, custom scheduling,
completion callbacks, tensor transfers, DLPack ownership, and builtin calls.
CUDA-only presets run the value tests without requiring a GPU; they do not
exercise device operations or successful DLPack import.
