#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

mode="${KUAI_FORMAT_MODE:-docker}"
local_formatter="${KUAI_CLANG_FORMAT:-clang-format}"
docker_formatter="${KUAI_DOCKER_CLANG_FORMAT:-clang-format}"

case "$mode" in
    local)
        exec "$local_formatter" "$@"
        ;;
    docker)
        image="${KUAI_FORMAT_DOCKER_IMAGE:?KUAI_FORMAT_DOCKER_IMAGE is required in docker mode}"
        exec docker run --rm \
            -u "$(id -u):$(id -g)" \
            -v "$repo_root:$repo_root" \
            -w "$repo_root" \
            "$image" "$docker_formatter" "$@"
        ;;
    *)
        printf 'clang_format: unsupported KUAI_FORMAT_MODE=%s\n' "$mode" >&2
        exit 2
        ;;
esac
