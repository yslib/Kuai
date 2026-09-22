#!/usr/bin/env bash
set -euo pipefail

destination="${1:?usage: setup-sdk.sh DESTINATION CACHE_DIR LOCK_FILE}"
cache_dir="${2:?usage: setup-sdk.sh DESTINATION CACHE_DIR LOCK_FILE}"
lock_file="${3:?usage: setup-sdk.sh DESTINATION CACHE_DIR LOCK_FILE}"
redist_root="https://developer.download.nvidia.com/compute/cuda/redist"

mkdir -p "$destination" "$cache_dir"

download_component() {
    local expected_sha256="$1"
    local relative_path="$2"
    local archive="$cache_dir/$(basename "$relative_path")"

    if [[ -f "$archive" ]] \
        && echo "$expected_sha256  $archive" | sha256sum --check --status; then
        return
    fi

    if [[ -f "$archive.part" ]] \
        && echo "$expected_sha256  $archive.part" | sha256sum --check --status; then
        mv "$archive.part" "$archive"
        return
    fi

    curl \
        --continue-at - \
        --fail \
        --location \
        --retry 5 \
        --silent \
        --show-error \
        "$redist_root/$relative_path" \
        --output "$archive.part"
    echo "$expected_sha256  $archive.part" | sha256sum --check
    mv "$archive.part" "$archive"
}

pids=()
while read -r expected_sha256 relative_path; do
    [[ -n "${expected_sha256:-}" && "${expected_sha256:0:1}" != "#" ]] \
        || continue
    download_component "$expected_sha256" "$relative_path" &
    pids+=("$!")
done < "$lock_file"

for pid in "${pids[@]}"; do
    wait "$pid"
done

while read -r expected_sha256 relative_path; do
    [[ -n "${expected_sha256:-}" && "${expected_sha256:0:1}" != "#" ]] \
        || continue
    archive="$cache_dir/$(basename "$relative_path")"
    echo "$expected_sha256  $archive" | sha256sum --check
    tar --extract \
        --xz \
        --file "$archive" \
        --directory "$destination" \
        --strip-components=1
done < "$lock_file"

# kuai links the CUDA math libraries dynamically. Keep only the static
# runtime archives required by nvcc/Clang device and host link steps.
find "$destination/lib" \
    -maxdepth 1 \
    -type f \
    -name '*.a' \
    ! -name 'libcudart_static.a' \
    ! -name 'libcudadevrt.a' \
    ! -name 'libculibos.a' \
    -delete

ln -s lib "$destination/lib64"
printf '%s\n' 'CUDA Version 12.8.1' > "$destination/version.txt"
printf '%s\n' \
    '{' \
    '  "cuda": {' \
    '    "name": "CUDA SDK",' \
    '    "version": "12.8.1"' \
    '  }' \
    '}' \
    > "$destination/version.json"
