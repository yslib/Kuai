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
