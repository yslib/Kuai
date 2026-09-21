# kuai-runtime

`kuai-runtime` is an independent C++ project. It does not depend on or inspect
the enclosing Rust workspace.

## Build

A local build requires CMake 4.1 or newer and a compatible C++ toolchain. The
CUDA presets additionally require the CUDA SDK described by the preset.

Configure and build the CPU runtime directly from this directory:

```bash
cmake --preset release-cpu
cmake --build --preset release-cpu
```

The corresponding CUDA workflow is:

```bash
cmake --preset release-cuda
cmake --build --preset release-cuda
```

Use `cmake --list-presets` to list the remaining debug, native-CUDA, and
combined presets. Build and install trees are written below `build/` and
`install/`.

Reference counting is atomic by default. Consumers that require cross-thread
ownership can configure with `-DKURT_REQUIRE_ATOMIC_REF_COUNT=ON`; this applies
to the runtime and all enabled vendor modules and rejects builds that disable
`KURT_ATOMIC_REF_COUNT`. The requirement option defaults to `OFF` for standalone
builds; the Rust sys adapter always enables it.

Each device owns an explicit default stream, independent of the backend's
per-thread stream. CUDA creates it as a nonblocking stream. Device shutdown
stops and waits for host transfers, selects the device, synchronizes the default
stream, releases its memory pool, and destroys the stream. Cleanup restores the
caller's selected device. During default-stream and memory-pool cleanup, failed
device selection or synchronization returns the first diagnostic error and
abandons those resources when safe release cannot be established. It does not
retry their release. This policy does not extend the host-transfer lanes'
existing backend-error recovery guarantees.

## Python wheel

The optional runtime-only Python binding remains part of this C++ project. It
requires a local Python environment containing the packages declared by
`python/pyproject.toml`.

```bash
./build-wheel.sh release-cpu
```

Set `KUAI_PYTHON` to choose the build interpreter. Set `KU_VENV_PYTHON` to
install the resulting wheel into another environment after it is built.

## Formatting and CI images

The C++ formatting rules live in `.clang-format`. The `docker/*.ci.lock` files
pin the toolchain images consumed by the enclosing repository's CI checks;
they are not required for a local CMake build.
