#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
preset="${1:-release-cpu}"

usage() {
    cat <<'USAGE'
Usage: ./build-wheel.sh PRESET

Builds and installs the selected kuai runtime SDK with the local toolchain,
then builds the standalone runtime-only Python wheel against that SDK.

Supported presets:
  release, debug, release-cpu, debug-cpu, release-cuda,
  release-cuda-nvcc, release-all

Environment:
  KUAI_PYTHON       Python used to build the wheel (default: python3).
  KU_VENV_PYTHON    Optional Python used to install the resulting wheel.
USAGE
}

if [[ "$preset" == "-h" || "$preset" == "--help" ]]; then
    usage
    exit 0
fi

case "$preset" in
    release | debug | release-cpu | debug-cpu | release-cuda | release-cuda-nvcc | release-all) ;;
    *)
        usage >&2
        exit 2
        ;;
esac

export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-64}"

sdk_dir="$repo_root/install/$preset"
kuai_dir="$sdk_dir/lib/cmake/kuai"
wheel_dir="$repo_root/python/dist/$preset"
python_executable="${KUAI_PYTHON:-python3}"
venv_python="${KU_VENV_PYTHON:-}"

cmake --workflow "$preset"
cmake --install "$repo_root/build/$preset"
"$python_executable" -m build \
    --wheel \
    --no-isolation \
    --outdir "$wheel_dir" \
    --config-setting "build-dir=build/$preset" \
    --config-setting "cmake.define.kuai_DIR=$kuai_dir" \
    "$repo_root/python"

shopt -s nullglob
wheels=("$wheel_dir"/kupy-*.whl)
shopt -u nullglob
if [[ "${#wheels[@]}" -ne 1 ]]; then
    printf 'Expected exactly one wheel in %s, found %d.\n' \
        "$wheel_dir" "${#wheels[@]}" >&2
    exit 1
fi

if [[ -n "$venv_python" ]]; then
    "$venv_python" -m pip install --force-reinstall "${wheels[0]}"
fi
