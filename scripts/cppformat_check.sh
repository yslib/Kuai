#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
formatter="${CLANG_FORMAT:-$script_dir/clang_format.sh}"

usage() {
    cat <<'USAGE'
Usage: scripts/cppformat_check.sh [--exclude PATH] PATH [PATH ...]

Checks C/C++ source formatting. Set KUAI_FORMAT_MODE=local to use a local
clang-format, or set KUAI_FORMAT_DOCKER_IMAGE for the default Docker mode.
USAGE
}

is_cpp_file() {
    case "$1" in
        *.c | *.cc | *.cpp | *.cxx | *.h | *.hh | *.hpp | *.hxx | *.cu | *.cuh) return 0 ;;
        *) return 1 ;;
    esac
}

resolve_path() {
    local path="$1"
    if [[ -e "$path" ]]; then
        realpath "$path"
    elif [[ -e "$repo_root/$path" ]]; then
        realpath "$repo_root/$path"
    else
        printf 'cppformat_check: path does not exist: %s\n' "$path" >&2
        return 1
    fi
}

is_excluded_path() {
    local path="$1"
    local exclude
    for exclude in "${exclude_paths[@]}"; do
        if [[ "$path" == "$exclude" || "$path" == "$exclude/"* ]]; then
            return 0
        fi
    done
    return 1
}

collect_files() {
    local target
    for target in "$@"; do
        target="$(resolve_path "$target")"
        if [[ -f "$target" ]]; then
            if is_cpp_file "$target" && ! is_excluded_path "$target"; then
                printf '%s\0' "$target"
            fi
        elif [[ -d "$target" ]]; then
            find "$target" \
                \( -type d \( -name .git -o -name build -o -name 'cmake-build-*' -o -name __pycache__ \) -prune \) -o \
                -type f \( \
                    -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o \
                    -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' -o \
                    -name '*.cu' -o -name '*.cuh' \
                \) -print0 |
                while IFS= read -r -d '' file; do
                    if ! is_excluded_path "$file"; then
                        printf '%s\0' "$file"
                    fi
                done
        fi
    done
}

targets=()
excludes=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h | --help)
            usage
            exit 0
            ;;
        --exclude)
            if [[ $# -lt 2 ]]; then
                echo "cppformat_check: --exclude requires a path" >&2
                exit 2
            fi
            excludes+=("$2")
            shift 2
            ;;
        --exclude=*)
            excludes+=("${1#--exclude=}")
            shift
            ;;
        --)
            shift
            break
            ;;
        -*)
            printf 'cppformat_check: unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
        *)
            targets+=("$1")
            shift
            ;;
    esac
done

while [[ $# -gt 0 ]]; do
    targets+=("$1")
    shift
done

if [[ ${#targets[@]} -eq 0 ]]; then
    echo "cppformat_check: requires at least one path" >&2
    usage >&2
    exit 2
fi

exclude_paths=()
for exclude in "${excludes[@]}"; do
    exclude_paths+=("$(resolve_path "$exclude")")
done

mapfile -d '' files < <(collect_files "${targets[@]}")
if [[ ${#files[@]} -eq 0 ]]; then
    echo "cppformat_check: no C/C++ files to check"
    exit 0
fi

file_list="$(mktemp "$repo_root/.cppformat-files.XXXXXX")"
trap 'rm -f "$file_list"' EXIT
printf '%s\n' "${files[@]}" >"$file_list"

if ! formatter_output="$("$formatter" --dry-run --Werror --files="$file_list" 2>&1)"; then
    printf '%s\n' "$formatter_output" >&2
    echo "cppformat_check: C/C++ formatting check failed" >&2
    exit 1
fi

printf 'cppformat_check: checked %d file(s).\n' "${#files[@]}"
