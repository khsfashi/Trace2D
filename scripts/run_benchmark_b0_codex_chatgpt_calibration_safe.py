#!/usr/bin/env python3
"""Race-safe entrypoint for the owner-local ChatGPT Codex B0 recovery calibration.

Codex may delete transient plugin-cache directories while a run is being cleaned.
The original recovery script used Path.rglob("auth.json"), which can fail on
Windows if one of those directories disappears between scandir operations.

This entrypoint keeps the frozen calibration implementation unchanged but
replaces only its cleanup hook with a top-down walk that never descends into a
`codex-home` tree. Those homes are ephemeral and are removed wholesale. Any
unexpected auth.json outside a codex-home is also removed best-effort; the final
allowlist evidence packager independently refuses a remaining credential.
"""
from __future__ import annotations

import os
import shutil
from pathlib import Path

import run_benchmark_b0_codex_chatgpt_calibration as calibration


def scrub_transient_codex_state(root: Path) -> None:
    root = Path(root)
    if not root.exists():
        return

    def ignore_walk_error(_error: OSError) -> None:
        # Transient Codex cache entries are allowed to disappear while walking.
        return

    for current, dirnames, filenames in os.walk(
        root,
        topdown=True,
        onerror=ignore_walk_error,
    ):
        current_path = Path(current)

        # Never recurse into Codex homes. They contain the copied credential and
        # volatile plugin caches, neither of which belongs in benchmark evidence.
        for dirname in list(dirnames):
            if dirname.casefold() != "codex-home":
                continue
            shutil.rmtree(current_path / dirname, ignore_errors=True)
            dirnames.remove(dirname)

        # Defense in depth for an auth file that somehow lives outside codex-home.
        for filename in filenames:
            if filename.casefold() != "auth.json":
                continue
            try:
                (current_path / filename).unlink(missing_ok=True)
            except OSError:
                pass


def main() -> int:
    calibration.scrub_auth = scrub_transient_codex_state
    return calibration.main()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except calibration.CalibrationError as exc:
        print(f"B0 ChatGPT calibration error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
