#!/usr/bin/env python3
"""Race-safe Windows entrypoint for owner-local ChatGPT Codex B0 recovery.

Two Windows-specific hazards are isolated here without changing the frozen B0
harness semantics:

1. Codex may delete transient plugin-cache directories while cleanup is walking
   them. Cleanup therefore never descends into a ``codex-home`` tree.
2. npm exposes Codex as ``codex.cmd``. A free-form positional prompt passed
   through ``cmd.exe /c`` can be split into multiple CLI arguments. Codex exec
   supports reading the prompt from stdin, so the model preflight uses the same
   stdin ``-`` transport already used by the real three-lane wrapper.

The final allowlist evidence packager remains the independent credential guard.
"""
from __future__ import annotations

import os
import shutil
from pathlib import Path
from typing import Any

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


def stdin_model_preflight(
    *,
    codex: str,
    auth_file: Path,
    workspace: Path,
    evidence_root: Path,
) -> dict[str, Any]:
    """Prove the selected ChatGPT Codex model using stdin prompt transport.

    The normal benchmark wrapper already invokes ``codex exec ... -`` and writes
    the prompt to stdin. Keep preflight transport identical so Windows ``.cmd``
    quoting cannot turn prompt words into extra clap arguments.
    """

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
    calibration.scrub_auth = scrub_transient_codex_state
    calibration.model_preflight = stdin_model_preflight
    return calibration.main()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except calibration.CalibrationError as exc:
        print(f"B0 ChatGPT calibration error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
