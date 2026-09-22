# Python facade migration reference

This directory preserves the old C++ binding and Python facade as reference
for a future Rust implementation. Its CMake/packaging files and the parent
`build-wheel.sh` are historical build references, not a supported or verified
wheel workflow. A runnable C++ Python extension is not maintained here.

The non-DLPack facade remains documented by the sources:

- `Instance`: vendor/device selection, cached Device identity, and flush.
- `Device`: id/type/instance properties and flush.
- `Tensor`: device/dtype/shape/strides/ndim/size properties.
- `to_device` / `to_host`: NumPy dtype mapping and column-major host transfers.
- `kuai_builtin` / `builtins`: argument validation, boxing and result conversion.

The C++ reference queries tensor info/data through the protocol-neutral C API.
The returned arrays and storage pointers borrow their native tensor, so the
reference's owning handle must remain alive during their use and any transfer.

`from_dlpack`, `_from_dlpack`, `Tensor.__dlpack__`, and
`Tensor.__dlpack_device__` have been removed. DLPack descriptors, capsule
ownership, version negotiation, and synchronization will be implemented in
the Rust Python binding instead; that replacement is not present here yet.
