#!/usr/bin/env python3
"""Relocate KuPy's wheel-local runtime before handing other libraries to Delocate."""

from __future__ import annotations

import argparse
import os
import shutil
import tempfile
from pathlib import Path

from delocate import delocate_wheel
from delocate.tools import get_install_names, set_install_id, set_install_name
from delocate.wheeltools import InWheel


PACKAGE = "kupy"
RUNTIME_INPUT = Path("_runtime_input") / "install-local-release-cpu" / "lib"
RUNTIME_FILES = ("libkurt.dylib", "libkurt_cpu.dylib")


def _loader_path(binary: Path, target: Path) -> str:
    return "@loader_path/" + os.path.relpath(target, binary.parent).replace(os.sep, "/")


def _remove_empty_staging_dirs(staging: Path, package: Path) -> None:
    current = staging
    while current != package:
        try:
            current.rmdir()
        except OSError:
            return
        current = current.parent


def _prepare_wheel(root: Path) -> None:
    package = root / PACKAGE
    native_extensions = sorted(package.glob("_native*.so"))
    if len(native_extensions) != 1:
        raise RuntimeError(
            f"expected exactly one {PACKAGE} native extension, found {len(native_extensions)}"
        )

    staging = package / RUNTIME_INPUT
    inputs = {name: staging / name for name in RUNTIME_FILES}
    missing = [str(path) for path in inputs.values() if not path.is_file()]
    if missing:
        raise RuntimeError("missing wheel runtime input: " + ", ".join(missing))

    runtime = package / ".runtime"
    runtime.mkdir()
    for name, source in inputs.items():
        shutil.move(str(source), runtime / name)
    _remove_empty_staging_dirs(staging, package)

    host = runtime / "libkurt.dylib"
    backend = runtime / "libkurt_cpu.dylib"
    private_targets = {
        "libkurt.dylib": host,
        "libkurt_cpu.dylib": backend,
    }
    for binary in root.rglob("*"):
        if not binary.is_file():
            continue
        for install_name in get_install_names(binary):
            target = private_targets.get(Path(install_name).name)
            if target is not None:
                set_install_name(binary, install_name, _loader_path(binary, target))

    set_install_id(host, "/DLC/kupy/.runtime/libkurt.dylib")
    set_install_id(backend, "/DLC/kupy/.runtime/libkurt_cpu.dylib")


def repair_wheel(input_wheel: str | Path, output_directory: str | Path) -> Path:
    """Repair ``input_wheel`` into ``output_directory`` without changing input."""
    input_wheel = Path(input_wheel).resolve(strict=True)
    output_directory = Path(output_directory).resolve()
    with tempfile.TemporaryDirectory() as temporary:
        prepared = Path(temporary) / input_wheel.name
        with InWheel(input_wheel, prepared) as unpacked:
            _prepare_wheel(Path(unpacked))
        output_directory.mkdir(parents=True, exist_ok=True)
        requested_output = output_directory / input_wheel.name
        delocate_wheel(
            str(prepared),
            str(requested_output),
            lib_sdir=".dylibs",
            require_archs=[],
            ignore_missing=False,
            sanitize_rpaths=True,
        )
    wheels = sorted(output_directory.glob("*.whl"))
    if len(wheels) != 1:
        raise RuntimeError(f"expected one repaired wheel in {output_directory}, found {len(wheels)}")
    return wheels[0]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_wheel", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    repair_wheel(args.input_wheel, args.output_directory)


if __name__ == "__main__":
    main()
