#!/usr/bin/env python3

import argparse
import concurrent.futures
import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys


TARGET_PATTERN = re.compile(r"(?:^|/)CMakeFiles/([^/]+)\.dir(?:/|$)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run syntax checks over translation units from compile_commands.json."
    )
    parser.add_argument("--build-dir", required=True, type=pathlib.Path)
    parser.add_argument("--target", action="append", default=[])
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--clang-check", default="clang-check")
    parser.add_argument(
        "--compiler-driver",
        action="store_true",
        help="run each compilation command with -fsyntax-only",
    )
    return parser.parse_args()


def target_name(entry: dict[str, object]) -> str | None:
    output = entry.get("output")
    if not isinstance(output, str):
        return None
    match = TARGET_PATTERN.search(output)
    return match.group(1) if match else None


def source_path(entry: dict[str, object]) -> pathlib.Path:
    source = entry.get("file")
    directory = entry.get("directory")
    if not isinstance(source, str) or not isinstance(directory, str):
        raise ValueError("each compilation entry requires string file and directory fields")
    path = pathlib.Path(source)
    return path if path.is_absolute() else pathlib.Path(directory) / path


def compiler_arguments(entry: dict[str, object]) -> list[str]:
    arguments = entry.get("arguments")
    if isinstance(arguments, list) and all(
        isinstance(argument, str) for argument in arguments
    ):
        return arguments.copy()

    command = entry.get("command")
    if isinstance(command, str):
        return shlex.split(command)
    raise ValueError("each compilation entry requires arguments or command")


def syntax_only_arguments(entry: dict[str, object]) -> list[str]:
    arguments = compiler_arguments(entry)
    result: list[str] = []
    options_with_values = {"-MF", "-MJ", "-MQ", "-MT", "-o"}
    options_without_values = {"-MD", "-MMD", "-c"}
    skip_next = False
    for argument in arguments:
        if skip_next:
            skip_next = False
            continue
        if argument in options_with_values:
            skip_next = True
            continue
        if argument in options_without_values:
            continue
        result.append(argument)
    result.append("-fsyntax-only")
    return result


def run_check(
    checker: str | None,
    build_dir: pathlib.Path,
    entry: dict[str, object],
) -> tuple[pathlib.Path, subprocess.CompletedProcess[str]]:
    source = source_path(entry)
    directory = entry["directory"]
    command = (
        syntax_only_arguments(entry)
        if checker is None
        else [checker, "-p", str(build_dir), str(source)]
    )
    result = subprocess.run(
        command,
        cwd=str(directory),
        check=False,
        capture_output=True,
        text=True,
    )
    return source, result


def main() -> int:
    args = parse_args()
    if args.jobs <= 0:
        raise SystemExit("--jobs must be greater than zero")

    build_dir = args.build_dir.resolve()
    database_path = build_dir / "compile_commands.json"
    if not database_path.is_file():
        raise SystemExit(f"missing compilation database: {database_path}")

    checker = None if args.compiler_driver else shutil.which(args.clang_check)
    if not args.compiler_driver and checker is None:
        raise SystemExit(f"clang-check is not executable: {args.clang_check}")

    try:
        database = json.loads(database_path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"cannot read compilation database: {error}") from error
    if not isinstance(database, list):
        raise SystemExit("compilation database root must be an array")

    targets = set(args.target)
    entries = [
        entry
        for entry in database
        if isinstance(entry, dict)
        and (not targets or target_name(entry) in targets)
    ]
    if not entries:
        selected = ", ".join(sorted(targets)) if targets else "all targets"
        raise SystemExit(f"no translation units selected for {selected}")

    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = [executor.submit(run_check, checker, build_dir, entry) for entry in entries]
        for future in concurrent.futures.as_completed(futures):
            source, result = future.result()
            if result.returncode == 0:
                continue
            failures += 1
            print(f"clang_syntax_check: {source} failed", file=sys.stderr)
            if result.stdout:
                print(result.stdout, end="", file=sys.stderr)
            if result.stderr:
                print(result.stderr, end="", file=sys.stderr)

    if failures:
        print(
            f"clang_syntax_check: {failures} of {len(entries)} translation units failed",
            file=sys.stderr,
        )
        return 1
    print(
        f"clang_syntax_check: checked {len(entries)} translation units "
        f"with {min(args.jobs, len(entries))} workers"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
