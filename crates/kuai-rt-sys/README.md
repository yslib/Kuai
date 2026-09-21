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
The adapter always configures `KURT_REQUIRE_ATOMIC_REF_COUNT=ON`, requiring atomic
native reference counting in the runtime and all enabled vendor modules.

## Bindings

The crate exposes the current public C API from
`kuai-runtime/src/kuai_c/include/kuai/kuai_c/`: instance and device management,
vendor and scheduler callback tables, frame contexts, objects, scalars,
strings, arrays, slices, completions, tensors, and builtin lookup and calls.
Kuai declarations are re-exported at the crate root. Tensor inspection uses
`ku_tensor_info_t`, `ku_tensor_get_info`, and `ku_tensor_get_data`; it does not
depend on an interchange protocol.

These are raw, unsafe bindings for a separate safe crate to build on. Integer
enums use their C integer types and named constants; structs and unions use
`#[repr(C)]`. C names, field order, pointer mutability, and function signatures
follow the public headers. Macros define opaque handles, integer constants,
and primitive payloads. No runtime dependencies or binding generator are required.

Required callbacks use `unsafe extern "C" fn(...)` directly. Slots that permit
null use `*const c_void`: `ku_scheduler_t.submit` and `ku_builtin_destroy_t`.
Their documented type
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
- Each device owns an explicit default stream distinct from the vendor's
  per-thread stream. The returned stream is borrowed; callers must not destroy
  it. Teardown drains host transfers and the default stream before releasing
  the memory pool and destroying the stream. Default-stream and memory-pool
  cleanup reports selection or synchronization failures as diagnostic statuses
  and abandons those resources when safe release cannot be established.
- `ku_tensor_get_device` returns the tensor's exact device handle borrowed from
  its instance, without retaining it. Do not release the device; it remains
  valid after the tensor is released while the instance is alive. Tensors must
  still be released before their instance is destroyed.
- `ku_tensor_get_info` copies the type/rank and borrows immutable shape/stride
  arrays from the tensor. Strides are in element units. Keep any owned reference
  to that same tensor and its instance alive while reading the arrays. Copy the
  arrays separately if metadata needs to outlive the tensor. The query does not
  allocate, retain, or synchronize. Wrong-kind objects return
  `KU_STATUS_TYPE_MISMATCH` and reset type/rank/pointers to NONE/zero/null.
- Rank zero has null shape/stride pointers and one element. Never pass these
  null pointers to Rust `slice::from_raw_parts`, even for a zero-length slice;
  handle rank zero separately. Otherwise multiply shape extents in their
  original order to derive the element count; a zero extent gives zero.
  Native construction checks those prefix products against `ku_size_t`
  overflow. Byte counts and conversions to other size types still need checks.
- `ku_tensor_get_data` borrows the address of the logical first element in the
  device address space; empty tensors succeed with null data. Wrong-kind objects
  return `KU_STATUS_TYPE_MISMATCH` and clear the pointer. Do not free it or infer
  exclusive access, allocation capacity, or host accessibility. Keep tensor and
  instance owners alive through pending operations, respecting storage access
  and synchronization rules. The query itself never synchronizes or retains.
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
- `kuVendorModule` is a vendor-module entry point exported by
  `libkurt_<vendor>`, not by the linked `libkurt`. Ordinary runtime callers use
  `ku_instance_init` to load a backend; directly using the entry point requires
  resolving/linking the vendor module and supplying a valid host handle.

## Tensor API migration

`ku_tensor_get_size` has been removed; derive the count from
`ku_tensor_get_info` as described above. The `kuai_sys::dlpack` module and
`ku_tensor_to_dlpack` / `ku_tensor_from_dlpack` functions have also been removed.
Use info/data queries for native tensor inspection and downloads via
`ku_device_copy_async`. They do not replace external-memory import, which is
not currently exposed by the tensor C API. Protocol adapters belong in higher
language bindings, not this raw runtime ABI.

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
completion callbacks, tensor transfers, borrowed tensor metadata/data, and builtin calls.
CUDA-only presets run the value tests without requiring a GPU; they do not
exercise device operations.
