#!/usr/bin/env python3
"""Qualify an external Windows ACL boundary for Trace2D Benchmark B0.

This probe performs no model call and no engine trial. It asks Codex CLI to run
ordinary commands through its native Windows sandbox, discovers the effective
sandbox SID, applies a temporary NTFS deny ACE for that SID to a throwaway
canary directory, and proves two facts with the built-in `:workspace` profile:

1. the sandbox can write inside its candidate workspace; and
2. the same sandbox identity cannot read the ACL-protected canary outside it.

The rejected custom Codex permission profile is intentionally not involved.
Every post-setup failure is packaged as scrubbed evidence so qualification never
depends on copying terminal output back into the repository by hand.

Codex 0.144 selects the host sandbox implementation by platform. On native
Windows, `codex sandbox` is already the WindowsCommand surface; adding a legacy
`windows` positional token makes Codex try to execute a program literally named
`windows`. Keep this invocation shape covered by unit tests.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import uuid
import zipfile
from pathlib import Path
from typing import Any

import benchmark_b0_codex_wrapper as core

EXPECTED_CODEX_VERSION = "0.144.6"
SID_PATTERN = re.compile(r"S-1-(?:\d+-)+\d+", re.IGNORECASE)


class ProbeError(RuntimeError):
    pass


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def write_json(path: Path, value: Any) -> None:
    write_text(path, json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def parse_sid(text: str) -> str:
    match = SID_PATTERN.search(text)
    if match is None:
        raise ProbeError("unable to find a Windows SID in sandbox identity output")
    return match.group(0).upper()


def redact(text: str, secret: str = "") -> str:
    value = text
    if secret:
        value = value.replace(secret, "<REDACTED_RANDOM_CANARY>")
    value = SID_PATTERN.sub("<REDACTED_WINDOWS_SID>", value)
    return value


def run_host(argv: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
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


def run_codex_sandbox(
    codex: str,
    *,
    profile: str,
    cwd: Path,
    command: list[str],
    timeout: float = 60.0,
) -> subprocess.CompletedProcess[str]:
    return core.capture(
        codex,
        [
            "sandbox",
            "--permission-profile",
            profile,
            "--cd",
            str(cwd),
            "--",
            *command,
        ],
        cwd=cwd,
        timeout=timeout,
    )


def process_record(
    completed: subprocess.CompletedProcess[str],
    *,
    secret: str = "",
) -> dict[str, Any]:
    return {
        "return_code": completed.returncode,
        "stdout": redact(completed.stdout or "", secret),
        "stderr": redact(completed.stderr or "", secret),
    }


def package_evidence(root: Path, output: Path) -> None:
    temporary = output.with_suffix(output.suffix + ".tmp")
    if temporary.exists():
        temporary.unlink()
    try:
        with zipfile.ZipFile(
            temporary,
            "w",
            compression=zipfile.ZIP_DEFLATED,
            compresslevel=6,
        ) as archive:
            for path in sorted(root.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(root).as_posix())
        os.replace(temporary, output)
    finally:
        if temporary.exists():
            temporary.unlink()


def main() -> int:
    parser = argparse.ArgumentParser(description="Qualify B0 Windows ACL isolation backend")
    parser.add_argument("--output-root")
    args = parser.parse_args()

    if os.name != "nt":
        raise ProbeError("this qualification probe must run on native Windows")

    repo_root = Path(__file__).resolve().parent.parent
    codex = core.resolve_codex("codex")
    version = core.verify_codex_version(codex, repo_root)
    if version != EXPECTED_CODEX_VERSION:
        raise ProbeError(f"expected Codex {EXPECTED_CODEX_VERSION}, got {version}")

    local_appdata = os.environ.get("LOCALAPPDATA")
    if not local_appdata and not args.output_root:
        raise ProbeError("LOCALAPPDATA is required unless --output-root is supplied")
    base = (
        Path(args.output_root).expanduser().resolve()
        if args.output_root
        else Path(local_appdata) / "Trace2D" / "b0" / "acl-probes"
    )
    stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime())
    root = base / f"windows-acl-isolation-{stamp}-{uuid.uuid4().hex[:8]}"
    output = root.with_suffix(".zip")
    workspace = root / "workspace"
    forbidden = root / "held-out"
    evidence_dir = root / "evidence"
    workspace.mkdir(parents=True, exist_ok=True)
    forbidden.mkdir(parents=True, exist_ok=True)
    evidence_dir.mkdir(parents=True, exist_ok=True)

    secret = f"TRACE2D-B0-ACL-DENY-{uuid.uuid4().hex}"
    canary = forbidden / "CANARY.txt"
    canary.write_text(secret, encoding="utf-8")

    result: dict[str, Any] = {
        "schema_version": 1,
        "kind": "trace2d_b0_windows_acl_isolation_backend_probe",
        "passed": False,
        "codex_version": version,
        "stage": "created",
        "scored": False,
        "model_called": False,
        "engine_trial_started": False,
        "canary_sha256": sha256_text(secret),
        "sandbox_cli_shape": "codex sandbox --permission-profile <name> --cd <dir> -- <command>",
    }
    sandbox_sid = ""
    acl_applied = False
    acl_cleanup_succeeded = True

    windir = Path(os.environ.get("WINDIR", r"C:\Windows"))
    whoami = str(windir / "System32" / "whoami.exe")
    cmd = str(Path(os.environ.get("COMSPEC", str(windir / "System32" / "cmd.exe"))))
    icacls = shutil.which("icacls") or str(windir / "System32" / "icacls.exe")

    try:
        host_identity = run_host([whoami, "/user", "/fo", "csv", "/nh"], repo_root)
        write_json(evidence_dir / "host-identity.process.json", process_record(host_identity))
        if host_identity.returncode != 0:
            raise ProbeError("host identity probe failed")
        host_sid = parse_sid(host_identity.stdout)
        result["host_sid_sha256"] = sha256_text(host_sid)
        result["stage"] = "host_identity"

        sandbox_identity = run_codex_sandbox(
            codex,
            profile=":read-only",
            cwd=workspace,
            command=[whoami, "/user", "/fo", "csv", "/nh"],
        )
        write_json(evidence_dir / "sandbox-identity.process.json", process_record(sandbox_identity))
        if sandbox_identity.returncode != 0:
            raise ProbeError("Codex direct Windows sandbox identity probe failed")
        sandbox_sid = parse_sid(sandbox_identity.stdout)
        result["sandbox_sid_sha256"] = sha256_text(sandbox_sid)
        result["sandbox_sid_differs_from_host"] = sandbox_sid != host_sid
        result["stage"] = "sandbox_identity"
        if sandbox_sid == host_sid:
            raise ProbeError("Codex Windows sandbox uses the host SID")

        deny_arg = f"*{sandbox_sid}:(OI)(CI)(F)"
        acl_apply = run_host([icacls, str(forbidden), "/deny", deny_arg], repo_root)
        write_json(evidence_dir / "acl-apply.process.json", process_record(acl_apply))
        if acl_apply.returncode != 0:
            raise ProbeError("failed to apply temporary deny ACE for Codex sandbox SID")
        acl_applied = True
        result["stage"] = "acl_applied"

        host_canary_ok = canary.read_text(encoding="utf-8") == secret
        result["host_canary_access_preserved"] = host_canary_ok
        if not host_canary_ok:
            raise ProbeError("host lost access to the temporary canary")

        allowed = run_codex_sandbox(
            codex,
            profile=":workspace",
            cwd=workspace,
            command=[
                cmd,
                "/d",
                "/s",
                "/c",
                "echo ACL_WORKSPACE_OK>ACL_WORKSPACE_RESULT.txt",
            ],
        )
        write_json(evidence_dir / "workspace-write.process.json", process_record(allowed))
        allowed_file = workspace / "ACL_WORKSPACE_RESULT.txt"
        workspace_write_ok = (
            allowed.returncode == 0
            and allowed_file.is_file()
            and allowed_file.read_text(encoding="utf-8", errors="replace").strip()
            == "ACL_WORKSPACE_OK"
        )
        result["workspace_profile"] = ":workspace"
        result["workspace_write_proved"] = workspace_write_ok
        result["stage"] = "workspace_write"
        if not workspace_write_ok:
            raise ProbeError("Codex :workspace sandbox did not prove candidate workspace write")

        denied = run_codex_sandbox(
            codex,
            profile=":workspace",
            cwd=workspace,
            command=[cmd, "/d", "/s", "/c", f'type "{canary}"'],
        )
        leaked = secret in (denied.stdout or "") or secret in (denied.stderr or "")
        write_json(
            evidence_dir / "held-out-read.process.json",
            process_record(denied, secret=secret),
        )
        external_read_denied = denied.returncode != 0 and not leaked
        result["external_acl_read_denied"] = external_read_denied
        result["canary_secret_leaked"] = leaked
        result["stage"] = "external_read"
        if not external_read_denied:
            raise ProbeError("sandbox identity could read the ACL-protected canary")

        result["passed"] = True
        result["stage"] = "mechanism_passed"
    except (ProbeError, OSError, subprocess.SubprocessError) as exc:
        result["passed"] = False
        result["error_type"] = type(exc).__name__
        result["error"] = str(exc)
    finally:
        if acl_applied and sandbox_sid:
            acl_remove = run_host(
                [icacls, str(forbidden), "/remove:d", f"*{sandbox_sid}"],
                repo_root,
            )
            write_json(evidence_dir / "acl-remove.process.json", process_record(acl_remove))
            acl_cleanup_succeeded = acl_remove.returncode == 0
        result["acl_cleanup_succeeded"] = acl_cleanup_succeeded
        if not acl_cleanup_succeeded:
            result["passed"] = False
            result["stage"] = "acl_cleanup_failed"
        try:
            canary.unlink(missing_ok=True)
        except OSError:
            result["passed"] = False
            result["canary_cleanup_succeeded"] = False
        else:
            result["canary_cleanup_succeeded"] = True
        write_json(evidence_dir / "result.json", result)
        package_evidence(root, output)

    print(json.dumps(result, indent=2, ensure_ascii=False))
    print(f"Evidence ZIP: {output}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ProbeError, OSError, subprocess.SubprocessError) as exc:
        print(f"B0 Windows ACL isolation probe error: {exc}", file=sys.stderr)
        raise SystemExit(2)
