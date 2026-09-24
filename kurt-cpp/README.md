# kurt-cpp

C++ host runtime and CPU/CUDA plugins. Requires CMake 4.1+ and a C++26
compiler and standard library. Run these commands from `kurt-cpp/`.
Set `CXX=/path/to/clang++` before configuring if needed.

## Build the host

```bash
cmake --preset release
cmake --build --preset release
cmake --install build/release
```

For Debug, replace `release` with `debug`. Build and install directories are
`build/<preset>` and `install/<preset>`.

## Build a CPU plugin

Build and install the host first, then run:

```bash
cmake -S vendor --preset release-cpu \
  -DCMAKE_PREFIX_PATH="$PWD/install/release"
cmake --build build/release-cpu
cmake --install build/release-cpu
```

For Debug, use `debug-cpu` with `install/debug`.

## Build a CUDA plugin

Run in the [kurt-build CUDA image](https://github.com/yslib/kurt-build), after
building and installing the host there:

```bash
cmake -S vendor --preset release-cuda \
  -DCMAKE_PREFIX_PATH="$PWD/install/release" \
  -DCMAKE_CUDA_ARCHITECTURES=80
cmake --build build/release-cuda
cmake --install build/release-cuda
```

Set `CMAKE_CUDA_ARCHITECTURES` for the deployment GPU. Use the Clang preset
above; `release-cuda-nvcc` is experimental and currently does not build.
Running the CUDA plugin requires an NVIDIA GPU, driver, and CUDA runtime libraries.

## Docker and presets

For a host build in a kurt-build image, add
`--toolchain cmake/toolchains/docker-base.cmake` to the configure command.
For CPU plugins, use `release-cpu-docker` or `debug-cpu-docker`, including that
preset name in the build and install paths. Edit `cmake/toolchains/` when
compiler or SDK paths change.

List presets with `cmake --list-presets=all`, or
`cmake -S vendor --list-presets=all` for plugins. After configuring,
`cmake --workflow --preset release` repeats the host configure/build steps.

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
cmake --install build/release-cuda --prefix "$PWD/install/runtime"
```

Keep Debug and Release installations separate. For Rust usage and library
search paths, see [kurt-sys](../crates/kurt-sys/README.md).

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
