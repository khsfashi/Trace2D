#!/usr/bin/env python3
"""Stable B0 harness entrypoint for owner-local matched runs.

This shim keeps the versioned core harness unchanged while applying two
pre-eligibility corrections discovered by the real unscored Windows run:

- project-generated Godot ``.godot`` cache content is not part of the authored
  candidate artifact hash and may disappear asynchronously after Godot exits;
- a provider turn that completes but exceeds the frozen task budget is a
  ``budget_exceeded`` implementation outcome, not a transport failure.

The shim does not alter task prompts, verifiers, Agent profile, budgets, lane
selection, or raw provider evidence.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
from typing import Any

import benchmark_b0 as core

WORKSPACE_HASH_POLICY = "authored_files_excluding_godot_cache_v1"
_VOLATILE_DIRECTORY_NAMES = {".godot"}
_ORIGINAL_CLASSIFY_AGENT_RESULT = core.classify_agent_result


def _walk_error(error: OSError) -> None:
    raise core.HarnessError(f"workspace traversal failed while hashing: {error}") from error


def stable_tree_hash(root: Path) -> str:
    """Hash authored workspace files while excluding engine-owned Godot cache."""
    root = Path(root).resolve()
    entries: list[tuple[str, str]] = []
    for current, directories, files in os.walk(
        root,
        topdown=True,
        followlinks=False,
        onerror=_walk_error,
    ):
        directories[:] = sorted(
            name
            for name in directories
            if name.casefold() not in _VOLATILE_DIRECTORY_NAMES
        )
        current_path = Path(current)
        for name in sorted(files):
            path = current_path / name
            if path.is_symlink():
                continue
            try:
                if not path.is_file():
                    continue
                relative = path.relative_to(root).as_posix()
                file_hash = core.sha256_file(path)
            except FileNotFoundError as exc:
                raise core.HarnessError(
                    f"authored workspace file disappeared while hashing: {path}"
                ) from exc
            entries.append((relative, file_hash))

    digest = hashlib.sha256()
    for relative, file_hash in sorted(entries):
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(file_hash.encode("ascii"))
        digest.update(b"\0")
    return digest.hexdigest()


def classify_agent_result(
    *,
    process: dict[str, Any],
    agent_result: dict[str, Any] | None,
    verifier: dict[str, Any] | None,
    integrity_ok: bool,
) -> tuple[str, str]:
    if (
        integrity_ok
        and not process["timed_out"]
        and agent_result is not None
        and int(agent_result.get("human_interventions", 0)) == 0
        and agent_result.get("status") == "budget_exceeded"
    ):
        return "budget_exceeded", core.FAILURE_DOMAINS["budget_exceeded"]
    return _ORIGINAL_CLASSIFY_AGENT_RESULT(
        process=process,
        agent_result=agent_result,
        verifier=verifier,
        integrity_ok=integrity_ok,
    )


def configure() -> None:
    core.FAILURE_DOMAINS["budget_exceeded"] = "implementation"
    core.tree_hash = stable_tree_hash
    core.classify_agent_result = classify_agent_result


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
