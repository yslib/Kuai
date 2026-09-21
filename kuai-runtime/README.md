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
