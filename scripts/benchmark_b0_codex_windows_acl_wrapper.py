#!/usr/bin/env python3
"""Final B0 Codex wrapper: built-in workspace profile + elevated Windows sandbox + external NTFS ACL.

The model-free ACL qualification proved the hard boundary only when Codex ran
under a Windows sandbox SID distinct from the host user. The first integrated
runner used a clean CODEX_HOME without pinning the Windows backend and therefore
failed closed when SID discovery returned the host SID. This shim freezes the
Windows backend to ``elevated`` before any matched lane trial exists, while
reusing the already-reviewed ACL lifecycle from benchmark_b0_codex_chatgpt_wrapper.
"""
from __future__ import annotations

from pathlib import Path

import benchmark_b0_codex_chatgpt_wrapper as acl
import benchmark_b0_codex_wrapper as core

MODEL_ID = "gpt-5.5"
MODEL_REVISION = "gpt-5.5"
WINDOWS_SANDBOX_MODE = "elevated"
ISOLATION_BACKEND = "windows_ntfs_acl_v1_elevated"


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


def configure() -> None:
    acl.ISOLATION_BACKEND = ISOLATION_BACKEND
    acl.configure()
    core.MODEL_ID = MODEL_ID
    core.MODEL_REVISION = MODEL_REVISION
    core.PERMISSION_PROFILE = f"{acl.PERMISSION_PROFILE}+{ISOLATION_BACKEND}"
    core.write_isolated_config = write_external_acl_config


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
