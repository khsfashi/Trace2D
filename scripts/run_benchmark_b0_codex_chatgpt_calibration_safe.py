#!/usr/bin/env python3
"""Race-safe Windows entrypoint for owner-local ChatGPT Codex B0 recovery.

Four owner-local hazards are isolated here without changing the B0 task,
filesystem boundary, or scoring semantics:

1. Codex may delete transient plugin-cache directories while cleanup is walking
   them. Cleanup therefore never descends into a ``codex-home`` tree.
2. npm exposes Codex as ``codex.cmd``. Free-form positional prompts can be split
   by ``cmd.exe``; model preflight therefore uses stdin ``-`` transport, matching
   the real three-lane wrapper.
3. GPT-5.6 is unavailable to the owner's real ChatGPT Codex session. Before any
   scored result exists, the cohort is frozen to the proven ``gpt-5.5`` selector.
4. The original isolation probe used a 90 second process timeout while real B0
   Agent trials allow 285 seconds under the same reasoning setting. The safe
   recovery path aligns the probe process ceiling with the real wrapper ceiling
   so a slow tool-using turn is not misclassified as an isolation failure.

Before deleting transient Codex homes, the safe entrypoint copies Codex's public
``.sandbox/sandbox.log`` into a packageable ``codex-sandbox-logs`` directory when
it exists. Credentials, ``.sandbox-secrets`` and transient homes remain excluded
by cleanup and the final evidence packager.
"""
from __future__ import annotations

import os
import shutil
from pathlib import Path
from typing import Any

import run_benchmark_b0_codex_chatgpt_calibration as calibration

FROZEN_MODEL_ID = "gpt-5.5"
FROZEN_PROVIDER_REVISION_POLICY = "chatgpt_codex_cli_selector_no_dated_snapshot"
ISOLATION_TIMEOUT_SECONDS = 285.0
_BASE_RUN = calibration.run


def preserve_sandbox_log(codex_home: Path, destination: Path) -> None:
    source = codex_home / ".sandbox" / "sandbox.log"
    if not source.is_file():
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        shutil.copy2(source, destination)
    except OSError:
        # Diagnostics are best-effort and must never block credential cleanup.
        pass


def sandbox_log_destination(root: Path, current_path: Path) -> Path:
    """Return a stable path outside directories excluded by the ZIP packager."""
    try:
        relative = current_path.relative_to(root)
        parts = relative.parts
    except ValueError:
        parts = ("external",)
    label = "__".join(parts) if parts else "root"
    safe_label = "".join(ch if ch.isalnum() or ch in {"-", "_", "."} else "_" for ch in label)
    return root / "codex-sandbox-logs" / f"{safe_label}.log"


def scrub_transient_codex_state(root: Path) -> None:
    root = Path(root)
    if not root.exists():
        return

    def ignore_walk_error(_error: OSError) -> None:
        return

    for current, dirnames, filenames in os.walk(
        root,
        topdown=True,
        onerror=ignore_walk_error,
    ):
        current_path = Path(current)
        for dirname in list(dirnames):
            if dirname.casefold() != "codex-home":
                continue
            codex_home = current_path / dirname
            preserve_sandbox_log(
                codex_home,
                sandbox_log_destination(root, current_path),
            )
            shutil.rmtree(codex_home, ignore_errors=True)
            dirnames.remove(dirname)
        for filename in filenames:
            if filename.casefold() != "auth.json":
                continue
            try:
                (current_path / filename).unlink(missing_ok=True)
            except OSError:
                pass


def run_with_matched_isolation_timeout(
    argv: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    check: bool = True,
    capture: bool = False,
):
    adjusted = list(argv)
    if "probe-isolation" in adjusted and "--timeout" not in adjusted:
        adjusted.extend(["--timeout", str(ISOLATION_TIMEOUT_SECONDS)])
    return _BASE_RUN(
        adjusted,
        cwd=cwd,
        env=env,
        check=check,
        capture=capture,
    )


def stdin_model_preflight(
    *,
    codex: str,
    auth_file: Path,
    workspace: Path,
    evidence_root: Path,
) -> dict[str, Any]:
    workspace.mkdir(parents=True, exist_ok=True)
    codex_home = evidence_root / "codex-home"
    codex_home.mkdir(parents=True, exist_ok=True)
    shutil.copy2(auth_file, codex_home / "auth.json")

    env = os.environ.copy()
    env["CODEX_HOME"] = str(codex_home)
    env["CODEX_CI"] = "1"
    prompt = "Reply exactly MODEL_OK. Do not use tools."
    completed = calibration.core.capture(
        codex,
        [
            "exec",
            "--json",
            "--ephemeral",
            "--skip-git-repo-check",
            "-C",
            str(workspace),
            "-m",
            calibration.MODEL_ID,
            "-",
        ],
        cwd=workspace,
        env=env,
        stdin_text=prompt,
        timeout=90.0,
    )
    calibration.preserve_process(evidence_root, "model-preflight", completed)

    events: list[dict[str, Any]] = []
    parse_error = ""
    try:
        if (completed.stdout or "").strip():
            events = calibration.core.parse_jsonl(completed.stdout or "")
    except Exception as exc:
        parse_error = f"{type(exc).__name__}: {exc}"

    usage = (
        calibration.core.token_usage(events)
        if events
        else {
            "input_tokens": 0,
            "cached_input_tokens": 0,
            "output_tokens": 0,
            "reasoning_output_tokens": 0,
        }
    )
    result = {
        "schema_version": 1,
        "kind": "codex_chatgpt_model_preflight",
        "passed": completed.returncode == 0
        and calibration.core.saw_turn_completed(events),
        "agent_id": f"openai-codex-cli@{calibration.CODEX_VERSION}",
        "model_selector": calibration.MODEL_ID,
        "provider_revision_policy": calibration.PROVIDER_REVISION_POLICY,
        "prompt_transport": "stdin_dash",
        "return_code": completed.returncode,
        "turn_completed": calibration.core.saw_turn_completed(events),
        "usage": usage,
        "jsonl_parse_error": parse_error,
    }
    calibration.write_json(evidence_root / "model-preflight.json", result)
    scrub_transient_codex_state(evidence_root)
    return result


def main() -> int:
    calibration.MODEL_ID = FROZEN_MODEL_ID
    calibration.MODEL_REVISION = FROZEN_MODEL_ID
    calibration.PROVIDER_REVISION_POLICY = FROZEN_PROVIDER_REVISION_POLICY
    calibration.core.MODEL_ID = FROZEN_MODEL_ID
    calibration.core.MODEL_REVISION = FROZEN_MODEL_ID
    calibration.run = run_with_matched_isolation_timeout
    calibration.scrub_auth = scrub_transient_codex_state
    calibration.model_preflight = stdin_model_preflight
    return calibration.main()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except calibration.CalibrationError as exc:
        print(f"B0 ChatGPT calibration error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
