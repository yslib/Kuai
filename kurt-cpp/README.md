# kurt-cpp

C++ host runtime and CPU/CUDA plugins. Requires CMake 4.1+ and a C++26
compiler and standard library, or MSVC with `/std:c++latest`.
Run these commands from `kurt-cpp/`.
Set `CXX=/path/to/clang++` before configuring if needed.

## Build the host

```bash
cmake --preset release
cmake --build --preset release
cmake --install build/release
```

For Debug, replace `release` with `debug`. Build and install directories are
`build/<preset>` and `install/<preset>`.

The host defaults to a static library. For CMake consumers that need a shared
host, configure a separate build with `-DBUILD_SHARED_LIBS=ON`. Rust consumes
the default static installation.

## Build a CPU plugin

Supported on Linux and macOS; Windows/MSVC support is experimental.
Build and install the host first, then run:

```bash
cmake -S vendor --preset release-cpu \
  -DCMAKE_PREFIX_PATH="$PWD/install/release"
cmake --build build/release-cpu
cmake --install build/release-cpu
```

For Debug, use `debug-cpu` with `install/debug`.

## Build a CUDA plugin

CUDA plugins target Linux. Run in the
[kurt-build CUDA image](https://github.com/yslib/kurt-build), after building and
installing the host there:

```bash
cmake -S vendor --preset release-cuda-linux-clang \
  -DCMAKE_PREFIX_PATH="$PWD/install/release" \
  -DCMAKE_CUDA_ARCHITECTURES=80
cmake --build build/release-cuda-linux-clang
cmake --install build/release-cuda-linux-clang
```

Set `CMAKE_CUDA_ARCHITECTURES` for the deployment GPU. Use the Clang preset
above; `release-cuda-nvcc` is experimental and currently does not build.
Running the CUDA plugin requires an NVIDIA GPU, driver, and CUDA runtime libraries.

## Toolchains and presets

Select matching host and CPU toolchains for your environment:

| Environment | Host configure option | CPU preset |
| --- | --- | --- |
| macOS arm64, Homebrew LLVM 22 | `--toolchain cmake/toolchains/macos-clang.cmake` | `release-cpu-macos-clang` |
| kurt-build Linux image | `--toolchain cmake/toolchains/docker-base.cmake` | `release-cpu-linux-clang` |
| Windows x64, Visual Studio 2022 | `--toolchain cmake/toolchains/windows-msvc.cmake` | `release-cpu-windows-msvc` |

Use the selected CPU preset name in the build and install paths. For Debug,
replace `release` with `debug`. Edit `cmake/toolchains/` when compiler or SDK
paths change.

List presets with `cmake --list-presets=all`, or
`cmake -S vendor --list-presets=all` for plugins. After configuring,
`cmake --workflow --preset release` repeats the host configure/build steps.

On Windows, use an x64 Developer PowerShell with CMake 4.1+ and Ninja:

```powershell
cmake --preset release -G Ninja --toolchain cmake/toolchains/windows-msvc.cmake
cmake --build --preset release --parallel 4
cmake --install build/release
cmake -S vendor --preset release-cpu-windows-msvc -G Ninja "-DCMAKE_PREFIX_PATH=$PWD/install/release"
cmake --build build/release-cpu-windows-msvc --parallel 2
cmake --install build/release-cpu-windows-msvc
```

Windows CUDA is not supported yet.

## Use the host from CMake

In the consuming project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 4.1)
project(example LANGUAGES CXX)
find_package(kuai CONFIG REQUIRED)
add_executable(example main.cpp)
target_link_libraries(example PRIVATE kuai::kurt)
```

From that project's directory:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/kurt-cpp/install/release
cmake --build build
```

## Deploy host and plugins

From `kurt-cpp/`, install the host and desired plugins into one directory:

```bash
cmake --install build/release --prefix "$PWD/install/runtime"
cmake --install build/release-cpu --prefix "$PWD/install/runtime"
# If built:
cmake --install build/release-cuda-linux-clang --prefix "$PWD/install/runtime"
```

Keep Debug and Release installations separate. For Rust usage and library
search paths, see [kurt-sys](../crates/kurt-sys/README.md).

At runtime, set `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH` (macOS) to the
plugin installation's `lib/` directory. On Windows, add the plugin installation's
`bin/` directory to `PATH`. A statically linked host needs no host shared library
at runtime.

TODO: Fully decouple plugins from the host implementation. Plugins currently
include referenced host archive objects; future host global state could be
duplicated. Build host and plugins from matching sources and C++ toolchains.

To locate an installed plugin from a deployment project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 4.1)
project(locate_plugin LANGUAGES NONE)
find_package(kuai_vendor_cpu CONFIG REQUIRED)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/plugin.txt"
    CONTENT "$<TARGET_FILE:kuai::kurt_cpu>\n")
```

Configure it with `CMAKE_PREFIX_PATH=/path/to/kurt-cpp/install/release-cpu`.
For CUDA, use `kuai_vendor_cuda` and `kuai::kurt_cuda`. Use these targets to
locate plugins, not as application link targets.
