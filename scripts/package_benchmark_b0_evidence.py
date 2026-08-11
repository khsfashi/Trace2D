#!/usr/bin/env python3
"""Create a scrubbed B0 evidence ZIP without transient Codex/runtime caches."""
from __future__ import annotations

import argparse
import os
import sys
import zipfile
from pathlib import Path

SKIP_DIR_NAMES = {
    "codex-home",
    ".probe-artifacts",
    ".godot",
    "__pycache__",
}
FORBIDDEN_FILE_NAMES = {"auth.json"}


class PackagingError(RuntimeError):
    pass


def should_skip_dir(name: str) -> bool:
    return name.casefold() in {value.casefold() for value in SKIP_DIR_NAMES}


def is_forbidden_file(name: str) -> bool:
    return name.casefold() in {value.casefold() for value in FORBIDDEN_FILE_NAMES}


def iter_evidence_files(root: Path):
    for current, dirs, files in os.walk(root, topdown=True, followlinks=False):
        dirs[:] = sorted(name for name in dirs if not should_skip_dir(name))
        current_path = Path(current)
        for name in sorted(files):
            if is_forbidden_file(name):
                raise PackagingError(
                    f"refusing to package credential-like file outside skipped transient homes: "
                    f"{current_path / name}"
                )
            path = current_path / name
            if path.is_symlink():
                continue
            yield path


def package(root: Path, output: Path) -> int:
    root = root.resolve()
    output = output.resolve()
    if not root.is_dir():
        raise PackagingError(f"run root does not exist: {root}")
    if output == root or root in output.parents:
        raise PackagingError("output ZIP must live outside the run root")

    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    if temporary.exists():
        temporary.unlink()

    count = 0
    try:
        with zipfile.ZipFile(
            temporary,
            mode="w",
            compression=zipfile.ZIP_DEFLATED,
            compresslevel=6,
        ) as archive:
            for path in iter_evidence_files(root):
                archive.write(path, path.relative_to(root).as_posix())
                count += 1
        if count == 0:
            raise PackagingError("run root contained no packageable evidence files")
        os.replace(temporary, output)
    finally:
        if temporary.exists():
            temporary.unlink()
    return count


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Package scrubbed Trace2D B0 evidence")
    parser.add_argument("--run-root", required=True)
    parser.add_argument("--output", required=True)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        count = package(Path(args.run_root), Path(args.output))
    except (PackagingError, OSError, zipfile.BadZipFile) as exc:
        print(f"B0 evidence packaging error: {exc}", file=sys.stderr)
        return 2
    print(f"Packaged {count} scrubbed evidence files: {Path(args.output).resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
