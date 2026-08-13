#!/usr/bin/env python3
"""Restore frozen B1 contract/fixture files from canonical Git blobs.

Windows self-hosted worktrees may retain CRLF materialization from an older
checkout even after `.gitattributes` starts forcing LF. B1 freeze hashes are
byte-level hashes of the repository blobs, so the owner runner must compare
against the canonical Git bytes, not stale worktree newline conversion.

This tool is intentionally narrow: it only touches paths already listed in the
frozen B1 manifest, verifies each HEAD blob against the preregistered SHA-256,
and refuses to write if repository bytes themselves drifted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from typing import Any


class MaterializeError(RuntimeError):
    pass


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _load_manifest(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise MaterializeError(f"failed to read freeze manifest {path}: {exc}") from exc
    if not isinstance(value, dict) or value.get("state") != "frozen" or value.get("algorithm") != "sha256":
        raise MaterializeError("B1 freeze manifest must remain a frozen sha256 manifest")
    files = value.get("files")
    if not isinstance(files, list) or not files:
        raise MaterializeError("B1 freeze manifest contains no files")
    return value


def _safe_repo_path(repo_root: Path, relative: str) -> Path:
    if not relative or "\\" in relative:
        raise MaterializeError(f"invalid frozen repository path: {relative!r}")
    root = repo_root.resolve()
    path = (root / relative).resolve()
    if path == root or root not in path.parents:
        raise MaterializeError(f"frozen path escapes repository root: {relative}")
    return path


def _git_blob(repo_root: Path, relative: str) -> bytes:
    completed = subprocess.run(
        ["git", "cat-file", "blob", f"HEAD:{relative}"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        stderr = completed.stderr.decode("utf-8", errors="replace")
        raise MaterializeError(f"cannot read canonical Git blob for {relative}: {stderr.strip()}")
    return completed.stdout


def materialize(repo_root: Path, manifest_path: Path, *, check_only: bool = False) -> dict[str, Any]:
    repo_root = repo_root.resolve()
    manifest = _load_manifest(manifest_path)
    repaired: list[str] = []
    checked = 0

    for entry in manifest["files"]:
        if not isinstance(entry, dict):
            raise MaterializeError("invalid freeze manifest entry")
        relative = entry.get("path")
        expected = entry.get("sha256")
        if not isinstance(relative, str) or not isinstance(expected, str) or len(expected) != 64:
            raise MaterializeError("invalid freeze manifest path/digest entry")
        target = _safe_repo_path(repo_root, relative)
        blob = _git_blob(repo_root, relative)
        blob_digest = _sha256(blob)
        if blob_digest != expected:
            raise MaterializeError(
                f"repository blob drifted from frozen digest: {relative}; expected {expected}, got {blob_digest}"
            )
        checked += 1
        current = target.read_bytes() if target.is_file() else None
        if current != blob:
            if check_only:
                raise MaterializeError(f"working tree differs from canonical frozen bytes: {relative}")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(blob)
            repaired.append(relative)
        if _sha256(target.read_bytes()) != expected:
            raise MaterializeError(f"failed to materialize frozen bytes: {relative}")

    return {"checked": checked, "repaired": repaired, "repaired_count": len(repaired)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--manifest", default="benchmarks/b1/freeze-manifest.json")
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).expanduser().resolve()
    manifest_path = (repo_root / args.manifest).resolve()
    result = materialize(repo_root, manifest_path, check_only=args.check_only)
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except MaterializeError as exc:
        print(f"B1 frozen-byte materialization error: {exc}")
        raise SystemExit(2)
