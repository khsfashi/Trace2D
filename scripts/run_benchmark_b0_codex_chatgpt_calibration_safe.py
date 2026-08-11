#!/usr/bin/env python3
"""Retired owner-local B0 calibration entrypoint.

This file is intentionally kept so old commands fail closed instead of silently
re-running a rejected benchmark-integrity boundary.

Owner-local evidence on 2026-08-11 proved that the Codex 0.144.6 native-Windows
custom permission profile used by this recovery path did not isolate the held-out
verifier boundary: the exact random canary read succeeded and the secret became
model-visible while candidate-workspace writes were blocked by policy.

Therefore this entrypoint MUST NOT launch the model, isolation probe, or any B0
lane. The next gate is the model-free external Windows ACL mechanism probe:

    python .\\scripts\\qualify_benchmark_b0_windows_acl_isolation.py

Only after that mechanism is independently proven and integrated into a new
runner may three-lane calibration resume.
"""
from __future__ import annotations

from pathlib import Path

import run_benchmark_b0_codex_chatgpt_calibration as calibration

FROZEN_MODEL_ID = "gpt-5.5"
FROZEN_PROVIDER_REVISION_POLICY = "chatgpt_codex_cli_selector_no_dated_snapshot"
RETIRED_ISOLATION_BACKEND = "codex_native_windows_custom_permission_profile"
REPLACEMENT_PROBE = Path("scripts/qualify_benchmark_b0_windows_acl_isolation.py")


def retirement_message() -> str:
    return (
        "B0 calibration is blocked: the Codex native-Windows custom permission "
        "profile was rejected after a real held-out canary leak. Do not rerun "
        "this calibration path. Qualify the replacement external Windows ACL "
        "boundary first with: python .\\scripts\\qualify_benchmark_b0_windows_acl_isolation.py"
    )


def main() -> int:
    raise calibration.CalibrationError(retirement_message())


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except calibration.CalibrationError as exc:
        print(f"B0 ChatGPT calibration error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
