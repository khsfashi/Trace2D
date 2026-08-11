#!/usr/bin/env python3
"""Final B0 Codex wrapper: elevated Windows sandbox + external NTFS ACL.

The final B0 owner-local path keeps the frozen Codex/model/isolation contract,
while adding three integration details learned before scored eligibility:

- discover the effective Windows sandbox identity before a Godot editor is
  launched, then reuse that exact in-process identity for the guarded model turn;
- canonicalize doubled Windows backslashes only for isolation evidence matching;
- classify a completed provider turn that exceeded the frozen resource budget as
  ``budget_exceeded`` rather than a tool transport failure.

Raw Codex trajectories and the frozen task/profile/budget remain unchanged.
"""
from __future__ import annotations

import json
import os
import shutil
from pathlib import Path
from typing import Any

import benchmark_b0_codex_chatgpt_wrapper as acl
import benchmark_b0_codex_wrapper as core

MODEL_ID = "gpt-5.5"
MODEL_REVISION = "gpt-5.5"
WINDOWS_SANDBOX_MODE = "elevated"
ISOLATION_BACKEND = "windows_ntfs_acl_v1_elevated"

_ORIGINAL_COMMAND_TEXTS = core.command_texts
_ORIGINAL_SETUP_CODEX_HOME = core.setup_codex_home
_ORIGINAL_RUN_TRIAL = core.run_trial
_PREPARED_IDENTITIES: dict[str, tuple[tuple[str, Any], tuple[str, Any]]] = {}


def write_external_acl_config(
    *,
    codex_home: Path,
    workspace: Path,
    read_only_roots: list[Path],
    lane: str,
    godot_mcp_server: Path | None,
    trace2d_mcp_server: Path | None,
) -> Path:
    config = acl.write_external_acl_config(
        codex_home=codex_home,
        workspace=workspace,
        read_only_roots=read_only_roots,
        lane=lane,
        godot_mcp_server=godot_mcp_server,
        trace2d_mcp_server=trace2d_mcp_server,
    )
    with config.open("a", encoding="utf-8", newline="\n") as stream:
        stream.write("\n[windows]\n")
        stream.write(f'sandbox = "{WINDOWS_SANDBOX_MODE}"\n')
    return config


def normalized_command_texts(events: list[dict[str, Any]]) -> list[str]:
    """Canonicalize Codex's Windows command display only for evidence matching."""
    output: list[str] = []
    for command in _ORIGINAL_COMMAND_TEXTS(events):
        normalized = command
        while "\\\\" in normalized:
            normalized = normalized.replace("\\\\", "\\")
        output.append(normalized)
    return output


def _identity_key(codex_home: Path) -> str:
    return os.path.normcase(str(codex_home.resolve()))


def setup_codex_home_with_identity(
    *,
    codex: str,
    workspace: Path,
    lane: str,
    result_root: Path,
    read_roots: list[Path],
    godot_mcp: Path | None,
    trace2d_mcp: Path | None,
) -> Path:
    """Prepare CODEX_HOME and discover the ACL identity before editor startup.

    The 2026-08-11 unscored calibration showed that running ``codex sandbox
    whoami`` only after the Godot editor/MCP stack was live could hit the
    60-second infrastructure timeout. Identity discovery does not depend on the
    editor, so perform it at the earlier deterministic setup boundary and keep
    the raw SID only in this wrapper process.
    """
    codex_home = _ORIGINAL_SETUP_CODEX_HOME(
        codex=codex,
        workspace=workspace,
        lane=lane,
        result_root=result_root,
        read_roots=read_roots,
        godot_mcp=godot_mcp,
        trace2d_mcp=trace2d_mcp,
    )
    host_identity = acl._host_sid(workspace)
    sandbox_identity = acl._sandbox_sid(
        codex=codex,
        workspace=workspace,
        codex_home=codex_home,
        read_roots=read_roots,
    )
    if sandbox_identity[0] == host_identity[0]:
        raise acl.AclIsolationError("Codex sandbox SID unexpectedly equals host SID")
    _PREPARED_IDENTITIES[_identity_key(codex_home)] = (host_identity, sandbox_identity)
    return codex_home


def _export_probe_acl_evidence(*, codex_home: Path, workspace: Path, strict: bool) -> None:
    if codex_home.parent.name.casefold() != ".probe-artifacts":
        return
    source = codex_home.parent / "acl-isolation.json"
    if not source.is_file():
        if strict:
            raise acl.AclIsolationError(
                "final ACL isolation turn completed without packageable acl-isolation evidence"
            )
        return
    destination = workspace.parent / "acl-isolation.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        shutil.copy2(source, destination)
    except OSError:
        if strict:
            raise


def guarded_run_codex(
    *,
    codex: str,
    workspace: Path,
    codex_home: Path,
    read_roots: list[Path],
    prompt: str,
    timeout: float,
):
    prepared = _PREPARED_IDENTITIES.pop(_identity_key(codex_home), None)
    original_host_sid = acl._host_sid
    original_sandbox_sid = acl._sandbox_sid
    if prepared is not None:
        host_identity, sandbox_identity = prepared
        acl._host_sid = lambda _workspace: host_identity
        acl._sandbox_sid = lambda **_kwargs: sandbox_identity

    try:
        try:
            result = acl.guarded_run_codex(
                codex=codex,
                workspace=workspace,
                codex_home=codex_home,
                read_roots=read_roots,
                prompt=prompt,
                timeout=timeout,
            )
        except BaseException:
            _export_probe_acl_evidence(
                codex_home=codex_home,
                workspace=workspace,
                strict=False,
            )
            raise
        _export_probe_acl_evidence(
            codex_home=codex_home,
            workspace=workspace,
            strict=True,
        )
        return result
    finally:
        acl._host_sid = original_host_sid
        acl._sandbox_sid = original_sandbox_sid


def _budget_details(result: dict[str, Any]) -> dict[str, Any]:
    metrics = result.get("metrics", {})
    limits = {
        "max_tool_calls": int(os.environ.get("TRACE2D_BENCH_MAX_TOOL_CALLS", "80")),
        "max_input_tokens": int(os.environ.get("TRACE2D_BENCH_MAX_INPUT_TOKENS", "100000")),
        "max_output_tokens": int(os.environ.get("TRACE2D_BENCH_MAX_OUTPUT_TOKENS", "20000")),
    }
    observed = {
        "tool_calls": int(metrics.get("tool_calls", 0)),
        "input_tokens": int(metrics.get("input_tokens", 0)),
        "output_tokens": int(metrics.get("output_tokens", 0)),
    }
    within = {
        "tool_calls": observed["tool_calls"] <= limits["max_tool_calls"],
        "input_tokens": observed["input_tokens"] <= limits["max_input_tokens"],
        "output_tokens": observed["output_tokens"] <= limits["max_output_tokens"],
    }
    return {
        "limits": limits,
        "observed": observed,
        "within": within,
        "exceeded": [name for name, ok in within.items() if not ok],
    }


def run_trial_with_budget_classification(args: Any) -> int:
    return_code = _ORIGINAL_RUN_TRIAL(args)
    result_path = Path(args.result_file).resolve()
    if not result_path.is_file():
        return return_code

    result = json.loads(result_path.read_text(encoding="utf-8"))
    wrapper = result.get("wrapper", {})
    provider_completed = (
        wrapper.get("process_return_code") == 0
        and wrapper.get("turn_completed") is True
    )
    if (
        result.get("status") == "tool_transport_failure"
        and provider_completed
        and wrapper.get("budget_ok") is False
    ):
        result["status"] = "budget_exceeded"
        result["budget"] = _budget_details(result)
        result_path.write_text(
            json.dumps(result, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    return return_code


def configure() -> None:
    acl.ISOLATION_BACKEND = ISOLATION_BACKEND
    acl.configure()
    core.MODEL_ID = MODEL_ID
    core.MODEL_REVISION = MODEL_REVISION
    core.PERMISSION_PROFILE = f"{acl.PERMISSION_PROFILE}+{ISOLATION_BACKEND}"
    core.write_isolated_config = write_external_acl_config
    core.setup_codex_home = setup_codex_home_with_identity
    core.run_codex = guarded_run_codex
    core.command_texts = normalized_command_texts
    core.run_trial = run_trial_with_budget_classification


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
