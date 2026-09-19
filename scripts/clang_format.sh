#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(realpath "$script_dir/..")"

version="$(<"$repo_root/kuai-runtime/.clang-format-version")"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "clang_format: invalid version in kuai-runtime/.clang-format-version" >&2
    exit 2
fi

if ! command -v uvx >/dev/null 2>&1; then
    echo "clang_format: uvx is required; install uv: https://docs.astral.sh/uv/getting-started/installation/" >&2
    exit 127
fi

# Resolve the exact native binary release on every platform, never a formatter
# from the host PATH. Keep local runs and CI on the same tool and style versions.
formatter=(uvx --from "clang-format==$version" clang-format)
actual_version="$("${formatter[@]}" --version)"
if [[ "$actual_version" != "clang-format version $version" ]]; then
    printf 'clang_format: expected clang-format version %s, got %s\n' "$version" "$actual_version" >&2
    exit 2
fi

exec "${formatter[@]}" "--style=file:$repo_root/kuai-runtime/.clang-format" "$@"
