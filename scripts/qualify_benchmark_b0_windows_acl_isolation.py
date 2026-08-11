#!/usr/bin/env python3
"""Qualify an external Windows ACL boundary for Trace2D Benchmark B0.

This probe performs no model call and no engine trial. It asks Codex CLI to run
ordinary commands through its native Windows sandbox, discovers the effective
sandbox SID, applies a temporary NTFS deny ACE for that SID to a throwaway
canary directory, and proves two facts with the built-in `:workspace` profile:

1. the sandbox can write inside its candidate workspace; and
2. the same sandbox identity cannot read the ACL-protected canary outside it.

The current custom Codex permission profile is intentionally not involved. B0
uses this probe only to decide whether an OS-owned boundary can replace the
native-Windows Codex read-deny backend that leaked the owner-local canary.
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
        raise ProbeError(f"unable to find Windows SID in output: {text.strip()}")
    return match.group(0).upper()


def redact(text: str, secret: str) -> str:
    return text.replace(secret, "<REDACTED_RANDOM_CANARY>")


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
            "windows",
            "--permissions-profile",
            profile,
            "--cd",
            str(cwd),
            "--",
            *command,
        ],
        cwd=cwd,
        timeout=timeout,
    )


def process_record(completed: subprocess.CompletedProcess[str], *, secret: str = "") -> dict[str, Any]:
    stdout = completed.stdout or ""
    stderr = completed.stderr or ""
    if secret:
        stdout = redact(stdout, secret)
        stderr = redact(stderr, secret)
    return {
        "return_code": completed.returncode,
        "stdout": stdout,
        "stderr": stderr,
    }


def package_evidence(root: Path, output: Path) -> None:
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(root.rglob("*")):
            if path.is_file() and path != output:
                archive.write(path, path.relative_to(root).as_posix())


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
    base = Path(args.output_root).expanduser().resolve() if args.output_root else Path(local_appdata) / "Trace2D" / "b0" / "acl-probes"
    stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime())
    root = base / f"windows-acl-isolation-{stamp}-{uuid.uuid4().hex[:8]}"
    workspace = root / "workspace"
    forbidden = root / "held-out"
    evidence_dir = root / "evidence"
    workspace.mkdir(parents=True, exist_ok=True)
    forbidden.mkdir(parents=True, exist_ok=True)
    evidence_dir.mkdir(parents=True, exist_ok=True)

    secret = f"TRACE2D-B0-ACL-DENY-{uuid.uuid4().hex}"
    canary = forbidden / "CANARY.txt"
    canary.write_text(secret, encoding="utf-8")

    whoami = str(Path(os.environ.get("WINDIR", r"C:\Windows")) / "System32" / "whoami.exe")
    cmd = str(Path(os.environ.get("COMSPEC", r"C:\Windows\System32\cmd.exe")))

    host_identity = run_host([whoami, "/user", "/fo", "csv", "/nh"], repo_root)
    if host_identity.returncode != 0:
        raise ProbeError(f"host whoami failed: {host_identity.stderr}")
    host_sid = parse_sid(host_identity.stdout)

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
    if sandbox_sid == host_sid:
        raise ProbeError("Codex Windows sandbox uses the host SID; ACL guard would not isolate the Agent")

    icacls = shutil.which("icacls") or str(Path(os.environ.get("WINDIR", r"C:\Windows")) / "System32" / "icacls.exe")
    deny_arg = f"*{sandbox_sid}:(OI)(CI)(F)"
    acl_apply = run_host([icacls, str(forbidden), "/deny", deny_arg], repo_root)
    write_json(evidence_dir / "acl-apply.process.json", process_record(acl_apply))
    if acl_apply.returncode != 0:
        raise ProbeError("failed to apply temporary deny ACE for Codex sandbox SID")

    try:
        # The owner/orchestrator must retain access while the sandbox SID is denied.
        host_canary_ok = canary.read_text(encoding="utf-8") == secret

        allowed_command = [cmd, "/d", "/s", "/c", "echo ACL_WORKSPACE_OK>ACL_WORKSPACE_RESULT.txt"]
        allowed = run_codex_sandbox(
            codex,
            profile=":workspace",
            cwd=workspace,
            command=allowed_command,
        )
        write_json(evidence_dir / "workspace-write.process.json", process_record(allowed))
        allowed_file = workspace / "ACL_WORKSPACE_RESULT.txt"
        workspace_write_ok = (
            allowed.returncode == 0
            and allowed_file.is_file()
            and allowed_file.read_text(encoding="utf-8", errors="replace").strip() == "ACL_WORKSPACE_OK"
        )

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

        result = {
            "schema_version": 1,
            "kind": "trace2d_b0_windows_acl_isolation_backend_probe",
            "passed": bool(host_canary_ok and workspace_write_ok and external_read_denied),
            "codex_version": version,
            "host_sid_sha256": sha256_text(host_sid),
            "sandbox_sid_sha256": sha256_text(sandbox_sid),
            "sandbox_sid_differs_from_host": sandbox_sid != host_sid,
            "host_canary_access_preserved": host_canary_ok,
            "workspace_profile": ":workspace",
            "workspace_write_proved": workspace_write_ok,
            "external_acl_read_denied": external_read_denied,
            "canary_secret_leaked": leaked,
            "canary_sha256": sha256_text(secret),
            "scored": False,
            "model_called": False,
            "engine_trial_started": False,
        }
        write_json(evidence_dir / "result.json", result)
    finally:
        acl_remove = run_host([icacls, str(forbidden), "/remove:d", f"*{sandbox_sid}"], repo_root)
        write_json(evidence_dir / "acl-remove.process.json", process_record(acl_remove))
        try:
            canary.unlink(missing_ok=True)
        except OSError:
            pass

    output = root.with_suffix(".zip")
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
