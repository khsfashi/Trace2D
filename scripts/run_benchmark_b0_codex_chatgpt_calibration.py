#!/usr/bin/env python3
"""Reuse the first owner-local B0 toolchain and rerun ChatGPT Codex qualification.

This recovery path performs no scored work. It verifies the cached deterministic
toolchain, proves the documented ChatGPT Codex CLI model selector separately,
then runs filesystem isolation and one unscored matched attempt per lane.
Every subprocess boundary that can block qualification is preserved in the
scrubbed evidence ZIP so another owner-local retry is never a blind retry.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path
from typing import Any

import benchmark_b0_codex_wrapper as core

CODEX_VERSION = "0.144.6"
MODEL_ID = "gpt-5.6"
MODEL_REVISION = "gpt-5.6"
PROVIDER_REVISION_POLICY = "chatgpt_codex_cli_selector_no_dated_snapshot"
GODOT_VERSION = "4.7.1-stable"
NODE_VERSION = "22.18.0"
TASK_ID = "b0-semantic-scene-authoring"
PROFILE_RELATIVE = Path("benchmarks/b0/agent-profile.codex-0.144.6.json")


class CalibrationError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha512_file(path: Path) -> str:
    digest = hashlib.sha512()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_file(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise CalibrationError(f"{label} not found: {resolved}")
    return resolved


def require_dir(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_dir():
        raise CalibrationError(f"{label} not found: {resolved}")
    return resolved


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def run(
    argv: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    check: bool = True,
    capture: bool = False,
) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        argv,
        cwd=cwd,
        env=env,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        check=False,
    )
    if check and completed.returncode != 0:
        detail = ""
        if capture:
            detail = f"\nstdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        raise CalibrationError(
            f"command failed ({completed.returncode}): {' '.join(argv)}{detail}"
        )
    return completed


def run_external(
    executable: Path | str,
    args: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    timeout: float = 30.0,
) -> subprocess.CompletedProcess[str]:
    return core.capture(str(executable), args, cwd=cwd, env=env, timeout=timeout)


def preserve_process(root: Path, stem: str, completed: subprocess.CompletedProcess[str]) -> None:
    root.mkdir(parents=True, exist_ok=True)
    (root / f"{stem}.stdout.txt").write_text(completed.stdout or "", encoding="utf-8")
    (root / f"{stem}.stderr.txt").write_text(completed.stderr or "", encoding="utf-8")
    write_json(
        root / f"{stem}.process.json",
        {"schema_version": 1, "return_code": completed.returncode},
    )


def newest_previous_run(runs_root: Path) -> Path:
    candidates = [
        path
        for path in runs_root.glob("codex-calibration-*")
        if path.is_dir() and (path / "toolchain.json").is_file()
    ]
    if not candidates:
        raise CalibrationError(f"no previous codex-calibration-* toolchain found under {runs_root}")
    return max(candidates, key=lambda path: (path / "toolchain.json").stat().st_mtime_ns)


def copy_tool_directory(source: Path, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)


def scrub_auth(root: Path) -> None:
    if not root.exists():
        return
    for path in root.rglob("auth.json"):
        try:
            path.unlink()
        except FileNotFoundError:
            pass


def model_preflight(
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
    completed = core.capture(
        codex,
        [
            "exec",
            "--json",
            "--ephemeral",
            "--skip-git-repo-check",
            "-C",
            str(workspace),
            "-m",
            MODEL_ID,
            "Reply exactly MODEL_OK. Do not use tools.",
        ],
        cwd=workspace,
        env=env,
        timeout=90.0,
    )
    preserve_process(evidence_root, "model-preflight", completed)
    events: list[dict[str, Any]] = []
    parse_error = ""
    try:
        events = core.parse_jsonl(completed.stdout or "") if (completed.stdout or "").strip() else []
    except Exception as exc:
        parse_error = f"{type(exc).__name__}: {exc}"
    usage = core.token_usage(events) if events else {
        "input_tokens": 0,
        "cached_input_tokens": 0,
        "output_tokens": 0,
        "reasoning_output_tokens": 0,
    }
    result = {
        "schema_version": 1,
        "kind": "codex_chatgpt_model_preflight",
        "passed": completed.returncode == 0 and core.saw_turn_completed(events),
        "agent_id": f"openai-codex-cli@{CODEX_VERSION}",
        "model_selector": MODEL_ID,
        "provider_revision_policy": PROVIDER_REVISION_POLICY,
        "return_code": completed.returncode,
        "turn_completed": core.saw_turn_completed(events),
        "usage": usage,
        "jsonl_parse_error": parse_error,
    }
    write_json(evidence_root / "model-preflight.json", result)
    scrub_auth(evidence_root)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Trace2D B0 ChatGPT Codex calibration recovery")
    parser.add_argument("--runs-root")
    parser.add_argument("--previous-run-root")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    scripts_root = repo_root / "scripts"
    suite_path = require_file(repo_root / "benchmarks/b0/suite.json", "B0 suite")
    profile_path = require_file(repo_root / PROFILE_RELATIVE, "B0 Codex profile")
    harness_path = require_file(scripts_root / "benchmark_b0.py", "B0 harness")
    require_file(scripts_root / "benchmark_b0_codex_chatgpt_wrapper.py", "ChatGPT Codex wrapper")
    packager_path = require_file(scripts_root / "package_benchmark_b0_evidence.py", "evidence packager")

    codex = core.resolve_codex("codex")
    actual_codex = core.verify_codex_version(codex, repo_root)
    if actual_codex != CODEX_VERSION:
        raise CalibrationError(f"Codex mismatch: expected {CODEX_VERSION}, got {actual_codex}")
    login = run_external(codex, ["login", "status"], cwd=repo_root)
    if login.returncode != 0:
        raise CalibrationError(f"codex login status failed: {login.stdout}\n{login.stderr}")
    auth_file = require_file(Path.home() / ".codex" / "auth.json", "file-backed Codex auth")

    local_appdata = os.environ.get("LOCALAPPDATA")
    if not local_appdata:
        raise CalibrationError("LOCALAPPDATA is required on the Windows owner-local calibration host")
    local_base = Path(local_appdata) / "Trace2D" / "b0"
    tools_root = local_base / "tools"
    runs_root = Path(args.runs_root).expanduser().resolve() if args.runs_root else local_base / "runs"
    runs_root.mkdir(parents=True, exist_ok=True)
    previous_root = (
        Path(args.previous_run_root).expanduser().resolve()
        if args.previous_run_root
        else newest_previous_run(runs_root)
    )
    previous_toolchain_path = require_file(previous_root / "toolchain.json", "previous calibration toolchain")
    previous = load_json(previous_toolchain_path)

    suite = load_json(suite_path)
    frozen_trace2d = str(suite["frozen_source"]["trace2d_commit"])
    if str(previous.get("frozen_trace2d_commit")) != frozen_trace2d:
        raise CalibrationError("previous calibration used a different frozen Trace2D source")
    if str(previous.get("codex", {}).get("version")) != CODEX_VERSION:
        raise CalibrationError("previous calibration used a different Codex version")
    if str(previous.get("godot", {}).get("version")) != GODOT_VERSION:
        raise CalibrationError("previous calibration used a different Godot version")
    if str(previous.get("node", {}).get("version")) != NODE_VERSION:
        raise CalibrationError("previous calibration used a different Node version")
    if str(previous.get("godot_mcp", {}).get("version")) != "4.1.0":
        raise CalibrationError("previous calibration used a different Godot MCP version")

    git = shutil.which("git")
    if not git:
        raise CalibrationError("git executable not found")
    head = run([git, "rev-parse", "HEAD"], cwd=repo_root, capture=True).stdout.strip()

    built_trace2d = require_file(
        repo_root / "build/windows-msvc/tools/trace2d/Debug/trace2d.exe",
        "previously built Trace2D CLI",
    )
    built_trace2d_mcp = require_file(
        repo_root / "build/windows-msvc/tools/mcp/Debug/trace2d_mcp.exe",
        "previously built Trace2D MCP host",
    )
    trace2d_tool_root = tools_root / f"trace2d-{frozen_trace2d}"
    trace2d_cli_root = trace2d_tool_root / "cli"
    trace2d_mcp_root = trace2d_tool_root / "mcp"
    copy_tool_directory(built_trace2d.parent, trace2d_cli_root)
    copy_tool_directory(built_trace2d_mcp.parent, trace2d_mcp_root)
    trace2d_bin = require_file(trace2d_cli_root / "trace2d.exe", "frozen Trace2D CLI copy")
    trace2d_mcp_bin = require_file(trace2d_mcp_root / "trace2d_mcp.exe", "frozen Trace2D MCP copy")

    godot_root = tools_root / f"godot-{GODOT_VERSION}"
    godot_archive = require_file(
        godot_root / f"Godot_v{GODOT_VERSION}_win64.exe.zip", "cached Godot archive"
    )
    godot_bin = require_file(
        godot_root / "extracted" / f"Godot_v{GODOT_VERSION}_win64.exe",
        "cached Godot executable",
    )
    godot_sha512 = sha512_file(godot_archive)
    if godot_sha512 != str(previous["godot"]["archive_sha512"]).lower():
        raise CalibrationError("cached Godot archive no longer matches previous qualification evidence")
    godot_version = run_external(godot_bin, ["--version"], cwd=repo_root)
    if godot_version.returncode != 0 or not godot_version.stdout.strip().startswith("4.7.1.stable"):
        raise CalibrationError(f"unexpected Godot version: {godot_version.stdout} {godot_version.stderr}")

    node_root = tools_root / f"node-v{NODE_VERSION}"
    node_archive = require_file(node_root / f"node-v{NODE_VERSION}-win-x64.zip", "cached Node archive")
    node_dir = require_dir(node_root / "extracted" / f"node-v{NODE_VERSION}-win-x64", "cached Node directory")
    node_bin = require_file(node_dir / "node.exe", "cached Node executable")
    node_sha256 = sha256_file(node_archive)
    if node_sha256 != str(previous["node"]["archive_sha256"]).lower():
        raise CalibrationError("cached Node archive no longer matches previous qualification evidence")
    node_version = run_external(node_bin, ["--version"], cwd=repo_root)
    if node_version.returncode != 0 or node_version.stdout.strip() != f"v{NODE_VERSION}":
        raise CalibrationError(f"unexpected Node version: {node_version.stdout} {node_version.stderr}")

    mcp_root = tools_root / f"godot-mcp-4.1.0-node-{NODE_VERSION}"
    mcp_server = require_file(mcp_root / "node_modules" / ".bin" / "godot-mcp.cmd", "cached Godot MCP server")
    mcp_env = os.environ.copy()
    mcp_env["PATH"] = str(node_dir) + os.pathsep + mcp_env.get("PATH", "")
    mcp_version = run_external(mcp_server, ["--version"], cwd=repo_root, env=mcp_env)
    if mcp_version.returncode != 0 or mcp_version.stdout.strip() != "4.1.0":
        raise CalibrationError(f"unexpected Godot MCP version: {mcp_version.stdout} {mcp_version.stderr}")

    shim_root = tools_root / f"command-shims-{GODOT_VERSION}"
    shim_root.mkdir(parents=True, exist_ok=True)
    (shim_root / "godot.cmd").write_text(f'@echo off\r\n"{godot_bin}" %*\r\n', encoding="ascii")

    env = os.environ.copy()
    env["PYTHONPATH"] = str(scripts_root)
    env["TRACE2D_BENCH_CODEX_AUTH_FILE"] = str(auth_file)
    env["TRACE2D_BENCH_GODOT_BIN"] = str(godot_bin)
    env["TRACE2D_BENCH_GODOT_MCP_SERVER"] = str(mcp_server)
    env["TRACE2D_BENCH_TRACE2D_BIN"] = str(trace2d_bin)
    env["TRACE2D_BENCH_TRACE2D_MCP_BIN"] = str(trace2d_mcp_bin)
    env["TRACE2D_BENCH_CODEX_READ_ROOTS"] = os.pathsep.join([str(node_dir), str(shim_root)])
    env["TRACE2D_BENCH_NODE_VERSION"] = f"v{NODE_VERSION}"
    env["TRACE2D_BENCH_WRAPPER_TIMEOUT"] = "285"

    stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime())
    run_root = runs_root / f"codex-chatgpt-calibration-{stamp}-{uuid.uuid4().hex[:8]}"
    calibration_root = run_root / "calibration"
    probe_root = run_root / "isolation-probe"
    preflight_root = run_root / "model-preflight"
    orchestration_logs = run_root / "orchestration-logs"
    calibration_root.mkdir(parents=True)
    probe_root.mkdir(parents=True)
    zip_path = run_root.with_suffix(".zip")
    canary_path = repo_root / "benchmarks" / "b0" / "verifiers" / f".codex-isolation-canary-{uuid.uuid4().hex}.txt"

    completed = False
    primary_error: Exception | None = None
    stage = "toolchain_record"
    try:
        write_json(
            run_root / "toolchain.json",
            {
                "schema_version": 1,
                "kind": "trace2d_b0_codex_chatgpt_calibration_toolchain",
                "repository_head": head,
                "frozen_trace2d_commit": frozen_trace2d,
                "profile_path": str(PROFILE_RELATIVE).replace("\\", "/"),
                "profile_sha256": sha256_file(profile_path),
                "reused_previous_toolchain_sha256": sha256_file(previous_toolchain_path),
                "codex": {
                    "version": CODEX_VERSION,
                    "model_id": MODEL_ID,
                    "model_revision": MODEL_REVISION,
                    "provider_revision_policy": PROVIDER_REVISION_POLICY,
                    "auth": "chatgpt-managed-local",
                },
                "godot": {
                    "version": GODOT_VERSION,
                    "reported_version": godot_version.stdout.strip(),
                    "archive_sha512": godot_sha512,
                },
                "node": {"version": NODE_VERSION, "archive_sha256": node_sha256},
                "godot_mcp": {
                    "package": "@satelliteoflove/godot-mcp@4.1.0",
                    "version": "4.1.0",
                    "npm_integrity": str(previous["godot_mcp"]["npm_integrity"]),
                },
                "generated_at_unix": time.time(),
            },
        )

        stage = "model_preflight"
        print(f"Running ChatGPT Codex model preflight with documented CLI selector {MODEL_ID}...")
        preflight = model_preflight(
            codex=codex,
            auth_file=auth_file,
            workspace=preflight_root / "workspace",
            evidence_root=preflight_root,
        )
        if not preflight["passed"]:
            raise CalibrationError("ChatGPT Codex model preflight failed; see model-preflight evidence")

        stage = "isolation_probe"
        canary_path.write_text(f"TRACE2D-B0-DENY-{uuid.uuid4().hex}", encoding="utf-8")
        probe_workspace = probe_root / "workspace"
        probe_workspace.mkdir()
        probe_evidence = probe_root / "isolation.json"
        print("Running Codex filesystem-isolation probe...")
        probe = run(
            [
                sys.executable,
                "-m",
                "benchmark_b0_codex_chatgpt_wrapper",
                "probe-isolation",
                "--workspace",
                str(probe_workspace),
                "--canary",
                str(canary_path),
                "--evidence",
                str(probe_evidence),
            ],
            cwd=repo_root,
            env=env,
            check=False,
            capture=True,
        )
        preserve_process(probe_root, "isolation-wrapper", probe)
        if probe.returncode != 0:
            if not probe_evidence.exists():
                write_json(
                    probe_evidence,
                    {
                        "schema_version": 1,
                        "kind": "codex_filesystem_isolation_probe",
                        "passed": false if False else False,
                        "stage": "wrapper_startup_or_transport",
                        "wrapper_return_code": probe.returncode,
                        "model_selector": MODEL_ID,
                        "detail": "Wrapper exited before producing its normal isolation verdict; see preserved stdout/stderr.",
                    },
                )
            raise CalibrationError(f"Codex isolation probe failed with exit code {probe.returncode}")
        scrub_auth(probe_root)

        stage = "matched_unscored_trials"
        exit_codes: dict[str, int] = {}
        for lane in ("godot.generic", "godot.agent", "trace2d.agent"):
            lane_env = env.copy()
            lane_env["TRACE2D_BENCH_ENGINE_VERSION"] = (
                GODOT_VERSION if lane.startswith("godot.") else f"trace2d@{frozen_trace2d}"
            )
            print(f"Running unscored ChatGPT Codex calibration: {lane}")
            trial = run(
                [
                    sys.executable,
                    str(harness_path),
                    "run-trial",
                    "--task",
                    TASK_ID,
                    "--lane",
                    lane,
                    "--agent-profile",
                    str(profile_path),
                    "--runs-root",
                    str(calibration_root),
                ],
                cwd=repo_root,
                env=lane_env,
                check=False,
                capture=True,
            )
            preserve_process(orchestration_logs, lane.replace(".", "-"), trial)
            exit_codes[lane] = trial.returncode
            scrub_auth(calibration_root)
            if trial.returncode != 0:
                print(f"Calibration {lane} returned {trial.returncode}; preserving it and continuing.")
        write_json(run_root / "calibration-exit-codes.json", exit_codes)

        stage = "aggregate_report"
        raw_records = require_file(calibration_root / "raw.jsonl", "calibration raw records")
        report = run(
            [
                sys.executable,
                str(harness_path),
                "report",
                "--records",
                str(raw_records),
                "--include-unscored",
            ],
            cwd=repo_root,
            env=env,
            capture=True,
        )
        preserve_process(orchestration_logs, "report", report)
        (run_root / "calibration-report.json").write_text(report.stdout, encoding="utf-8")
        report_json = json.loads(report.stdout)
        if not report_json["integrity"]["same_agent_profile_per_task"]:
            raise CalibrationError("calibration mixed Agent profile hashes")
        if int(report_json["record_count"]) != 3:
            raise CalibrationError(
                f"expected exactly three preserved lane records, got {report_json['record_count']}"
            )
        completed = True
    except Exception as exc:
        primary_error = exc
        write_json(
            run_root / "failure.json",
            {
                "schema_version": 1,
                "kind": "trace2d_b0_codex_chatgpt_calibration_failure",
                "stage": stage,
                "exception_type": type(exc).__name__,
                "message": str(exc),
                "scored": False,
                "model_selector": MODEL_ID,
                "generated_at_unix": time.time(),
            },
        )
    finally:
        try:
            canary_path.unlink(missing_ok=True)
        except OSError:
            pass
        scrub_auth(run_root)
        package = run(
            [
                sys.executable,
                str(packager_path),
                "--run-root",
                str(run_root),
                "--output",
                str(zip_path),
            ],
            cwd=repo_root,
            env=env,
            check=False,
            capture=True,
        )
        if package.returncode != 0:
            raise CalibrationError(
                f"evidence packager failed with exit code {package.returncode}: {package.stderr}"
            )

    print(f"Evidence ZIP: {zip_path}")
    if primary_error is not None:
        raise CalibrationError(
            f"B0 ChatGPT Codex calibration did not complete: {primary_error}. "
            "Upload the generated evidence ZIP; it now contains the blocking subprocess evidence."
        ) from primary_error
    if not completed:
        raise CalibrationError("B0 ChatGPT Codex calibration did not complete")
    print("B0 ChatGPT Codex calibration completed.")
    print("Upload only the scrubbed evidence ZIP; do not run scored trials yet.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CalibrationError as exc:
        print(f"B0 ChatGPT calibration error: {exc}", file=sys.stderr)
        raise SystemExit(2)
