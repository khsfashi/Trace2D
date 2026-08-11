#!/usr/bin/env python3
"""ChatGPT-managed B0 Codex wrapper with external Windows ACL isolation.

The model/budget contract remains frozen to Codex CLI 0.144.6 + gpt-5.5/high.
The rejected native-Windows custom permission profile is not used. Codex runs
with the built-in :workspace profile and shell network access disabled, while
the host applies an NTFS deny ACE for the real Codex sandbox SID to the Trace2D
repository for the entire model turn. The host removes that ACE in a finally
path and emits scrubbed per-turn ACL evidence.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Any

import benchmark_b0_codex_wrapper as core

MODEL_ID = "gpt-5.5"
MODEL_REVISION = "gpt-5.5"
PROVIDER_REVISION_POLICY = "chatgpt_codex_cli_selector_no_dated_snapshot"
PERMISSION_PROFILE = ":workspace"
ISOLATION_BACKEND = "windows_ntfs_acl_v1"
SID_PATTERN = re.compile(r"S-1-(?:\d+-)+\d+", re.IGNORECASE)

_ORIGINAL_RUN_CODEX = core.run_codex


class AclIsolationError(core.WrapperError):
    pass


def _sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def _parse_sid(text: str) -> str:
    match = SID_PATTERN.search(text)
    if match is None:
        raise AclIsolationError("unable to discover Windows SID for ACL isolation")
    return match.group(0).upper()


def _redact(text: str) -> str:
    return SID_PATTERN.sub("<REDACTED_WINDOWS_SID>", text)


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def _process_record(completed: subprocess.CompletedProcess[str]) -> dict[str, Any]:
    return {
        "return_code": completed.returncode,
        "stdout": _redact(completed.stdout or ""),
        "stderr": _redact(completed.stderr or ""),
    }


def _host_capture(argv: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        argv,
        cwd=cwd,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def _windows_tools() -> tuple[str, str]:
    windir = Path(os.environ.get("WINDIR", r"C:\Windows"))
    whoami = str(windir / "System32" / "whoami.exe")
    icacls = shutil.which("icacls") or str(windir / "System32" / "icacls.exe")
    return whoami, icacls


def _sandbox_sid(
    *,
    codex: str,
    workspace: Path,
    codex_home: Path,
    read_roots: list[Path],
) -> tuple[str, subprocess.CompletedProcess[str]]:
    if os.name != "nt":
        raise AclIsolationError("windows_ntfs_acl_v1 requires native Windows")
    whoami, _ = _windows_tools()
    env = core.codex_environment(codex_home, read_roots)
    completed = core.capture(
        codex,
        [
            "sandbox",
            "--permission-profile",
            ":read-only",
            "--cd",
            str(workspace),
            "--",
            whoami,
            "/user",
            "/fo",
            "csv",
            "/nh",
        ],
        cwd=workspace,
        env=env,
        timeout=60.0,
    )
    if completed.returncode != 0:
        raise AclIsolationError("Codex sandbox SID discovery failed")
    return _parse_sid(completed.stdout), completed


def _host_sid(workspace: Path) -> tuple[str, subprocess.CompletedProcess[str]]:
    whoami, _ = _windows_tools()
    completed = _host_capture([whoami, "/user", "/fo", "csv", "/nh"], workspace)
    if completed.returncode != 0:
        raise AclIsolationError("host SID discovery failed")
    return _parse_sid(completed.stdout), completed


def protected_roots() -> list[Path]:
    # This is deliberately not user-configurable downward. The repository contains
    # the held-out verifier, task fixtures and harness implementation.
    roots = [Path(__file__).resolve().parents[1]]
    extra = os.environ.get("TRACE2D_BENCH_ACL_EXTRA_PROTECTED_ROOTS", "")
    for raw in extra.split(os.pathsep):
        if raw.strip():
            roots.append(Path(raw.strip()).expanduser().resolve())
    result: list[Path] = []
    seen: set[str] = set()
    for root in roots:
        resolved = root.resolve()
        key = os.path.normcase(str(resolved))
        if key not in seen:
            seen.add(key)
            result.append(resolved)
    return result


def write_external_acl_config(
    *,
    codex_home: Path,
    workspace: Path,
    read_only_roots: list[Path],
    lane: str,
    godot_mcp_server: Path | None,
    trace2d_mcp_server: Path | None,
) -> Path:
    del read_only_roots  # Built-in :workspace owns runtime/tool readability.
    lines = [
        f"model = {core.toml_string(MODEL_REVISION)}",
        f"model_reasoning_effort = {core.toml_string(core.REASONING_EFFORT)}",
        'approval_policy = "never"',
        'default_permissions = ":workspace"',
        'web_search = "disabled"',
        "allow_login_shell = false",
        "",
        "[sandbox_workspace_write]",
        "network_access = false",
        "",
        "[shell_environment_policy]",
        'inherit = "core"',
    ]

    if lane == "godot.agent":
        if godot_mcp_server is None:
            raise core.WrapperError("TRACE2D_BENCH_GODOT_MCP_SERVER is required for godot.agent")
        if os.name == "nt" and godot_mcp_server.suffix.lower() in {".cmd", ".bat"}:
            command = os.environ.get("COMSPEC", "cmd.exe")
            command_line = subprocess.list2cmdline([str(godot_mcp_server)])
            mcp_args = ["/d", "/s", "/c", command_line]
        else:
            command = str(godot_mcp_server)
            mcp_args = []
        lines.extend(
            [
                "",
                "[mcp_servers.godot]",
                f"command = {core.toml_string(command)}",
                "args = [" + ", ".join(core.toml_string(value) for value in mcp_args) + "]",
                "startup_timeout_sec = 20",
                "tool_timeout_sec = 45",
            ]
        )
    elif lane == "trace2d.agent":
        if trace2d_mcp_server is None:
            raise core.WrapperError("TRACE2D_BENCH_TRACE2D_MCP_BIN is required for trace2d.agent")
        scene = workspace / "scene.trace2d.toml"
        lines.extend(
            [
                "",
                "[mcp_servers.trace2d]",
                f"command = {core.toml_string(str(trace2d_mcp_server))}",
                "args = ["
                + ", ".join(
                    core.toml_string(value)
                    for value in ["--scene", str(scene), "--seed", "42"]
                )
                + "]",
                "startup_timeout_sec = 10",
                "tool_timeout_sec = 30",
            ]
        )

    codex_home.mkdir(parents=True, exist_ok=True)
    config = codex_home / "config.toml"
    config.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return config


def guarded_run_codex(
    *,
    codex: str,
    workspace: Path,
    codex_home: Path,
    read_roots: list[Path],
    prompt: str,
    timeout: float,
):
    if os.name != "nt":
        raise AclIsolationError("B0 external ACL wrapper is native-Windows only")

    evidence_path = codex_home.parent / "acl-isolation.json"
    host_sid, host_identity = _host_sid(workspace)
    sandbox_sid, sandbox_identity = _sandbox_sid(
        codex=codex,
        workspace=workspace,
        codex_home=codex_home,
        read_roots=read_roots,
    )
    if sandbox_sid == host_sid:
        raise AclIsolationError("Codex sandbox SID unexpectedly equals host SID")

    _, icacls = _windows_tools()
    applied: list[Path] = []
    apply_records: list[dict[str, Any]] = []
    cleanup_records: list[dict[str, Any]] = []
    result: tuple[subprocess.CompletedProcess[str], list[dict[str, Any]]] | None = None
    primary_error: BaseException | None = None
    cleanup_ok = True

    roots = protected_roots()
    evidence: dict[str, Any] = {
        "schema_version": 1,
        "kind": "trace2d_b0_codex_external_acl_turn",
        "backend": ISOLATION_BACKEND,
        "permission_profile": PERMISSION_PROFILE,
        "sandbox_sid_differs_from_host": True,
        "host_sid_sha256": _sha256_text(host_sid),
        "sandbox_sid_sha256": _sha256_text(sandbox_sid),
        "host_identity_process": _process_record(host_identity),
        "sandbox_identity_process": _process_record(sandbox_identity),
        "protected_roots": [
            {"path_sha256": _sha256_text(os.path.normcase(str(root)))} for root in roots
        ],
        "acl_apply": apply_records,
        "acl_cleanup": cleanup_records,
        "acl_apply_succeeded": False,
        "acl_cleanup_succeeded": False,
        "model_turn_started": False,
    }

    try:
        for root in roots:
            completed = _host_capture(
                [icacls, str(root), "/deny", f"*{sandbox_sid}:(OI)(CI)(F)"],
                workspace,
            )
            apply_records.append(_process_record(completed))
            if completed.returncode != 0:
                raise AclIsolationError("failed to apply B0 repository deny ACE")
            applied.append(root)
        evidence["acl_apply_succeeded"] = True
        evidence["model_turn_started"] = True
        result = _ORIGINAL_RUN_CODEX(
            codex=codex,
            workspace=workspace,
            codex_home=codex_home,
            read_roots=read_roots,
            prompt=prompt,
            timeout=timeout,
        )
    except BaseException as exc:
        primary_error = exc
    finally:
        for root in reversed(applied):
            completed = _host_capture(
                [icacls, str(root), "/remove:d", f"*{sandbox_sid}"],
                workspace,
            )
            cleanup_records.append(_process_record(completed))
            if completed.returncode != 0:
                cleanup_ok = False
        evidence["acl_cleanup_succeeded"] = cleanup_ok
        evidence["passed"] = bool(evidence["acl_apply_succeeded"] and cleanup_ok)
        _write_json(evidence_path, evidence)

    if not cleanup_ok:
        raise AclIsolationError("failed to remove B0 repository deny ACE; stop benchmark execution")
    if primary_error is not None:
        raise primary_error
    if result is None:
        raise AclIsolationError("Codex turn produced no result under external ACL guard")
    return result


def configure() -> None:
    core.MODEL_ID = MODEL_ID
    core.MODEL_REVISION = MODEL_REVISION
    core.PERMISSION_PROFILE = f"{PERMISSION_PROFILE}+{ISOLATION_BACKEND}"
    core.write_isolated_config = write_external_acl_config
    core.run_codex = guarded_run_codex


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
