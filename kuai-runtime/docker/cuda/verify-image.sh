#!/usr/bin/env bash
set -euo pipefail

image="${1:?usage: verify-image.sh IMAGE}"

"$(dirname "$0")/../base/verify-image.sh" "$image"

docker run --rm --interactive --entrypoint /bin/bash "$image" -s <<'VERIFY_IMAGE'
set -euo pipefail

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf "missing required command: %s\n" "$1" >&2
        return 1
    fi
}

[[ "${CUDA_HOME:-}" == "/usr/local/cuda" ]]
[[ "${CUDACXX:-}" == "/opt/llvm/bin/clang++" ]]
[[ "${CC:-}" == "/opt/llvm/bin/clang" ]]
[[ "${CXX:-}" == "/opt/llvm/bin/clang++" ]]

for command_name in nvcc ptxas nvlink; do
    require_command "$command_name"
done

nvcc --version | grep -F "release 12.8" >/dev/null
grep -Eq '"cuda"[[:space:]]*:[[:space:]]*\{' /usr/local/cuda/version.json
grep -Eq '"version"[[:space:]]*:[[:space:]]*"12\.8\.' /usr/local/cuda/version.json

for required_file in \
    /usr/local/cuda/include/cuda_runtime.h \
    /usr/local/cuda/include/thrust/version.h \
    /usr/local/cuda/include/cub/version.cuh \
    /usr/local/cuda/include/cuda/std/version \
    /usr/local/cuda/nvvm/libdevice/libdevice.10.bc \
    /usr/local/cuda/lib64/libcudart_static.a \
    /usr/local/cuda/lib64/libcudadevrt.a \
    /usr/local/cuda/lib64/libcublas.so \
    /usr/local/cuda/lib64/libcurand.so \
    /usr/local/cuda/lib64/libcusolver.so \
    /usr/local/cuda/lib64/libcusparse.so \
    /usr/local/cuda/lib64/libnvJitLink.so; do
    [[ -e "$required_file" ]] || {
        printf "missing required CUDA SDK file: %s\n" "$required_file" >&2
        exit 1
    }
done

compile_scratch="$(mktemp -d)"
trap "rm -rf \"$compile_scratch\"" EXIT
cat > "$compile_scratch/probe.cu" <<"EOF"
#include <cuda_runtime.h>

__global__ void increment(int *value) {
    auto add_one = [] __device__(int input) { return input + 1; };
    if (threadIdx.x == 0) {
        *value = add_one(*value);
    }
}
EOF

clang++ \
    --cuda-path=/usr/local/cuda \
    --cuda-gpu-arch=sm_80 \
    -std=c++20 \
    -c "$compile_scratch/probe.cu" \
    -o "$compile_scratch/probe-clang.o"

nvcc \
    -ccbin /opt/rh/gcc-toolset-14/root/usr/bin/g++ \
    -arch=sm_80 \
    -std=c++20 \
    --extended-lambda \
    -c "$compile_scratch/probe.cu" \
    -o "$compile_scratch/probe-nvcc.o"

[[ -s "$compile_scratch/probe-clang.o" ]]
[[ -s "$compile_scratch/probe-nvcc.o" ]]
VERIFY_IMAGE
