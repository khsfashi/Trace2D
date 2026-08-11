#!/usr/bin/env python3
"""Reuse the first owner-local B0 toolchain and rerun only ChatGPT Codex qualification.

The original Windows calibration already built Trace2D and pinned/verified the
Godot, Node and Godot-MCP toolchain before the provider rejected an unsupported
dated API snapshot. This recovery path verifies that preserved toolchain, swaps
only the provider-selectable ChatGPT Codex model identity, then runs the real
isolation probe and one unscored attempt in each matched lane.
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
MODEL_ID = "gpt-5.6-sol"
MODEL_REVISION = "gpt-5.6-sol"
PROVIDER_REVISION_POLICY = "chatgpt_managed_identifier_no_dated_snapshot"
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
    return core.capture(
        str(executable),
        args,
        cwd=cwd,
        env=env,
        timeout=timeout,
    )


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def newest_previous_run(runs_root: Path) -> Path:
    candidates: list[Path] = []
    for path in runs_root.glob("codex-calibration-*"):
        if path.is_dir() and (path / "toolchain.json").is_file():
            candidates.append(path)
    if not candidates:
        raise CalibrationError(
            f"no previous codex-calibration-* toolchain found under {runs_root}"
        )
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
    (shim_root / "godot.cmd").write_text(
        f'@echo off\r\n"{godot_bin}" %*\r\n', encoding="ascii"
    )

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
    calibration_root.mkdir(parents=True)
    probe_root.mkdir(parents=True)
    zip_path = run_root.with_suffix(".zip")
    canary_path = repo_root / "benchmarks" / "b0" / "verifiers" / f".codex-isolation-canary-{uuid.uuid4().hex}.txt"

    completed = False
    primary_error: Exception | None = None
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

        canary_path.write_text(f"TRACE2D-B0-DENY-{uuid.uuid4().hex}", encoding="utf-8")
        probe_workspace = probe_root / "workspace"
        probe_workspace.mkdir()
        probe_evidence = probe_root / "isolation.json"
        print(f"Running ChatGPT Codex model/isolation probe with {MODEL_ID}...")
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
        )
        if probe.returncode != 0:
            raise CalibrationError(f"Codex ChatGPT isolation probe failed with exit code {probe.returncode}")
        scrub_auth(probe_root)

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
            )
            exit_codes[lane] = trial.returncode
            scrub_auth(calibration_root)
            if trial.returncode != 0:
                print(f"Calibration {lane} returned {trial.returncode}; preserving it and continuing.")
        write_json(run_root / "calibration-exit-codes.json", exit_codes)

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
        (run_root / "calibration-report.json").write_text(report.stdout, encoding="utf-8")
        report_json = json.loads(report.stdout)
        if not report_json["integrity"]["same_agent_profile_per_task"]:
            raise CalibrationError("calibration mixed Agent profile hashes")
        if int(report_json["record_count"]) != 3:
            raise CalibrationError(
                f"expected exactly three preserved lane records, got {report_json['record_count']}"
            )
        completed = True
    except Exception as exc:  # preserve failed infrastructure evidence too
        primary_error = exc
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
        )
        if package.returncode != 0:
            raise CalibrationError(f"evidence packager failed with exit code {package.returncode}")

    print(f"Evidence ZIP: {zip_path}")
    if primary_error is not None:
        raise CalibrationError(
            f"B0 ChatGPT Codex calibration did not complete: {primary_error}. "
            "Upload the generated evidence ZIP for classification."
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
