# KuPy Rust-backed Python facade

`kupy` provides a synchronous Python facade for the Kuai CPU runtime. The
Rust/PyO3 extension exposes `Instance`, `Device`, and `Tensor`; the Python layer
provides NumPy transfers, `builtins`, and `kuai_builtin` declarations. This
package replaces the former C++ Python binding.

## Use the facade

After installing the repaired wheel, create an explicit CPU instance and use
its device for host transfers. A tensor operand selects its device for a
builtin call; an all-scalar call needs `device=`:

```python
import numpy as np
import kupy

instance = kupy.Instance("cpu")
device = instance.default_device
source = np.array([[1, 2], [3, 4]], dtype=np.int32)
tensor = kupy.to_device(source, device=device)
result = kupy.builtins.add(tensor, 10)
np.testing.assert_array_equal(kupy.to_host(result), source + 10)
assert result.device is device
assert kupy.builtins.add(2, 3, device=device) == 5
```

`instance.device(0)` and `instance.default_device` return the same live Python
`Device` object through a weak cache. A `Tensor` retains its `Device`, which
retains its `Instance`; the result remains usable after its inputs or local
instance variable are deleted. At the Python boundary, the Rust adapter keeps
the native `KuArc` tensor and a stable boxed `KuInstance` clone alive together.
This does not change `kurt`'s borrowed public Rust API or add a second native
object reference count. A host download returns an independent, writable NumPy
array.

`to_device` accepts NumPy arrays with native-endian `bool`, `int8`, `int16`,
`int32`, `int64`, `float32`, or `float64` dtype and rank at most eight. Uploads
and downloads copy synchronously. Multi-dimensional arrays are normalized to
column-major storage; `Tensor.strides` reports element strides. Noncontiguous
NumPy inputs are copied into that layout. DLPack and asynchronous transfers are
not available.

The `builtins` module declares the audited CPU builtin names. The
`@kupy.kuai_builtin` decorator uses a declaration's function name as the
native name, while `@kupy.kuai_builtin("bitAnd")` supplies an alias; declaration
bodies are not executed. Its operands are fixed positional parameters, with
an optional keyword-only `device=` on calls. Operands may be same-device
`Tensor` objects or Python `bool`, `int`, `float`, `str`, `None`, and `slice`
values. A tensor operand infers the device. Integer host values must fit
signed 64 bits. Unknown or unaudited names raise `NotImplementedError`;
`dot` is declared but is not registered on the CPU backend.

The CPU builtin boundary checks names, arity, device identity, tensor metadata,
output allocation and selected arithmetic domains before native dispatch.
Individual builtin families may have stricter rank and dtype limits than host
transfers. Some checks download tensor values to the host and replay an
operation in O(n) time, so these calls are useful for front-end integration
testing rather than performance measurements. `cumsum` still relies on native
arithmetic domain and grouping assumptions; the guards do not establish every
numeric precondition. Native null values and type promotion/broadcasting follow
Kuai, not NumPy: a null scalar Boolean becomes Python `None`, a null tensor
Boolean downloads as `True`, and numeric nulls use their native sentinels (signed
integer minimum and lowest finite floating-point value, not NaN). For example,
`normal` accepts a zero count but positive counts are currently reported as
`ValueError` because the CPU backend returns `NOT_SUPPORTED`. Other native
errors remain possible after validation.

Calls use the conventional CPython GIL. A builtin creates a call-local frame
and arguments on the calling thread; this facade provides no asynchronous
execution or DLPack synchronization. External writers must be synchronized
while `to_device` copies its NumPy input, while guards take host snapshots, and
while `to_host` downloads tensor data. This facade is not a general CUDA or
public `Array`/runtime extension interface.

`__version__` and `runtime_info()` remain diagnostics. Importing `kupy` does
not create a hidden instance. `runtime_info()` briefly creates and closes its
own CPU instance, so calling it while an explicit CPU instance is alive raises
`RuntimeError` under the runtime's single-instance rule.

## Supported scope

The first supported workflow builds a version-specific (not `abi3`) wheel for a
native macOS CPU and a conventional GIL-enabled CPython 3.10 or newer. It does
not support free-threaded CPython, cross-compilation, Linux, Windows, CUDA,
sdists, editable installs, or distributing a raw `maturin` wheel directly.

The workflow was verified on macOS 26.6.2 arm64 with CPython 3.14.7 and NumPy
2.5.3. Packaged binaries currently have a macOS 26.0 deployment minimum. This
comes from the native binaries; `pyproject.toml` sets that target explicitly
because maturin's generic Apple Silicon default is 11.0, which is too low for
the C++ runtime's libc++ availability.

Prerequisites are:

- `uv` and `uvx`;
- Cargo with a Rust toolchain supporting edition 2024;
- the existing local C++26 CPU build toolchain, including CMake 4.1 or newer;
- macOS Command Line Tools.

No Docker setup or CMake changes are required. `maturin==1.15.0` and
`delocate==0.13.0` are pinned in `pyproject.toml`; Rust dependencies are locked
in the repository `Cargo.lock`.

## Build and install

From the repository root, create an explicit virtual environment. Activation
is optional; use that environment's interpreter for the diagnostic:

```sh
uv venv --python python3 /path/to/venv
cargo xtask python build --python /path/to/venv/bin/python
cargo xtask python install --venv /path/to/venv
/path/to/venv/bin/python -c 'import kupy; print(kupy.runtime_info())'
```

`build` requires the interpreter supplied with `--python`; it never falls back
to a system or `PATH` Python. `install` requires an already-existing virtual
environment with `venv/bin/python`, rebuilds the wheel, and reinstalls `kupy`
even at the same version. It likewise neither activates nor creates an
environment. A successful repaired wheel is retained below Cargo's target
directory in `wheels/` (normally `target/wheels/`) and its path is printed.

The commands select the matching native Apple Rust target explicitly, so a
Cargo `build.target` cannot redirect the wheel. They reject cross-target
settings such as `CARGO_BUILD_TARGET` and `PYO3_CROSS`, and accept only the
local `release-cpu` runtime configuration (or those runtime variables unset).

## Wheel contents

The raw wheel emitted by maturin is an intermediate artifact. The `cargo xtask
python` commands run the package repair step before retaining or installing a
wheel. That step privately bundles `libkurt.dylib` and `libkurt_cpu.dylib` in
`kupy/.runtime` and rewrites their Mach-O references to use relative loader
paths. `libkurt_cpu.dylib` remains a dynamically loaded backend, so both
libraries are included deliberately. Delocate places any additional
non-system dependencies in `kupy/.dylibs`.

The repaired wheel is self-contained with respect to those private runtime
libraries; it must not depend on Cargo output paths. The original CMake/Cargo
artifacts are not altered. This does not create a general source distribution
or standalone repository: a future extraction will need to change the `kurt`
dependency sourcing, which is currently a workspace path dependency.

## Checks

Run the focused orchestration and repair tests from the repository root:

```sh
cargo fmt --all -- --check
cargo test -p kurt-python --locked
cargo test -p xtask --locked
uvx --from delocate==0.13.0 python -m unittest discover -s kurt-python/tests -p test_repair.py -v
```

After installing into the explicit environment, run the six installed-package
test modules in isolated mode from outside the source tree, using absolute test
paths. Remove Python and dyld path overrides from that test process:

```sh
cd /tmp
for test in test_runtime.py test_instances.py test_transfers.py test_facade.py test_builtins.py test_lifetimes.py; do
  env -u PYTHONPATH -u DYLD_LIBRARY_PATH -u DYLD_FALLBACK_LIBRARY_PATH -u DYLD_INSERT_LIBRARIES \
    /path/to/venv/bin/python -I /absolute/path/to/kuai/kurt-python/tests/"$test" -v
done
```
