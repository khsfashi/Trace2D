#!/usr/bin/env python3
"""Final B0 Codex wrapper: built-in workspace profile + elevated Windows sandbox + external NTFS ACL.

The model-free ACL qualification proved the hard boundary only when Codex ran
under a Windows sandbox SID distinct from the host user. The first integrated
runner used a clean CODEX_HOME without pinning the Windows backend and therefore
failed closed when SID discovery returned the host SID. This shim freezes the
Windows backend to ``elevated`` before any matched lane trial exists, while
reusing the already-reviewed ACL lifecycle from benchmark_b0_codex_chatgpt_wrapper.

Codex JSONL on Windows may preserve command paths with doubled backslashes. The
isolation verifier canonicalizes that representation only for the command-path
attempt matcher; the actual command trajectory remains untouched. Probe ACL
evidence that would otherwise live under the package-excluded .probe-artifacts
directory is also exported beside the isolation verdict for independent review.
"""
from __future__ import annotations

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


def configure() -> None:
    acl.ISOLATION_BACKEND = ISOLATION_BACKEND
    acl.configure()
    core.MODEL_ID = MODEL_ID
    core.MODEL_REVISION = MODEL_REVISION
    core.PERMISSION_PROFILE = f"{acl.PERMISSION_PROFILE}+{ISOLATION_BACKEND}"
    core.write_isolated_config = write_external_acl_config
    core.run_codex = guarded_run_codex
    core.command_texts = normalized_command_texts


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
