#!/usr/bin/env python3
"""Independently verify one preserved Benchmark B2 candidate workspace."""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any

import benchmark_b0_stable_harness
import benchmark_b2_execution_freeze

TASK_ID = "b2-topdown-combat-v1"
LANES = ("godot.generic", "godot.agent", "trace2d.agent")
GODOT_VERIFIER = Path("benchmarks/b2/qualification/godot_verifier/B2GodotVerifier.gd")
GODOT_RUNNER = Path("benchmarks/b2/qualification/godot_verifier/B2GodotVerifierRunner.gd")
GODOT_PASS_MARKER = "b2-godot-verifier-pass"
TRACE2D_TARGET = "trace2d_b2_trace2d_candidate_verify"
TRACE2D_PASS_MARKER = "B2 Trace2D verifier accepted candidate"


class CandidateVerifierError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CandidateVerifierError(message)


def require_file(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    require(resolved.is_file(), f"{label} not found: {resolved}")
    return resolved


def require_dir(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    require(resolved.is_dir(), f"{label} not found: {resolved}")
    return resolved


def run(argv: list[str], *, cwd: Path, timeout: float) -> dict[str, Any]:
    started = time.perf_counter_ns()
    try:
        completed = subprocess.run(
            argv,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        return_code: int | None = completed.returncode
        stdout = completed.stdout
        stderr = completed.stderr
        timed_out = False
    except subprocess.TimeoutExpired as exc:
        return_code = None
        stdout = exc.stdout if isinstance(exc.stdout, str) else ""
        stderr = exc.stderr if isinstance(exc.stderr, str) else ""
        timed_out = True
    return {
        "argv": argv,
        "return_code": return_code,
        "timed_out": timed_out,
        "duration_ms": (time.perf_counter_ns() - started) / 1_000_000.0,
        "stdout": stdout,
        "stderr": stderr,
    }


def resolve_godot(explicit: str | None) -> Path:
    raw = explicit or os.environ.get("TRACE2D_B2_GODOT_BIN") or os.environ.get("TRACE2D_BENCH_GODOT_BIN")
    require(bool(raw), "Godot verifier requires --godot-bin or TRACE2D_B2_GODOT_BIN")
    return require_file(Path(str(raw)), "pinned Godot executable")


def process_verdict(process: dict[str, Any], pass_marker: str, verifier_id: str) -> dict[str, Any]:
    combined = str(process.get("stdout", "")) + "\n" + str(process.get("stderr", ""))
    if process["timed_out"]:
        status, code = "error", "verifier_timeout"
    elif process["return_code"] == 0 and pass_marker in combined:
        status, code = "pass", "accepted"
    elif process["return_code"] in (0, 1):
        status, code = "fail", "candidate_rejected"
    else:
        status, code = "error", "verifier_process_error"
    return {"status": status, "code": code, "verifier": verifier_id, "process": process}


def verify_godot(repo_root: Path, workspace: Path, godot: Path, timeout: float) -> dict[str, Any]:
    source_verifier = require_file(repo_root / GODOT_VERIFIER, "qualified Godot B2 verifier")
    source_runner = require_file(repo_root / GODOT_RUNNER, "qualified Godot B2 verifier runner")
    require_file(workspace / "project.godot", "candidate Godot project")
    require_file(workspace / "main.tscn", "candidate Godot main scene")
    before = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    with tempfile.TemporaryDirectory(prefix="trace2d-b2-scored-godot-") as temporary:
        candidate = Path(temporary) / "candidate"
        shutil.copytree(workspace, candidate)
        shutil.copy2(source_verifier, candidate / "__trace2d_b2_verifier.gd")
        shutil.copy2(source_runner, candidate / "__trace2d_b2_runner.gd")
        process = run(
            [
                str(godot),
                "--headless",
                "--path",
                str(candidate),
                "--fixed-fps",
                "60",
                "--script",
                "res://__trace2d_b2_runner.gd",
            ],
            cwd=candidate,
            timeout=timeout,
        )
    after = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    require(before == after, "preserved Godot candidate workspace mutated during verification")
    verdict = process_verdict(process, GODOT_PASS_MARKER, "b2-godot-qualified-v1")
    verdict["workspace_sha256_before"] = before
    verdict["workspace_sha256_after"] = after
    return verdict


def candidate_executable(build_dir: Path) -> Path:
    name = TRACE2D_TARGET + (".exe" if os.name == "nt" else "")
    matches = sorted(path for path in build_dir.rglob(name) if path.is_file())
    require(bool(matches), f"Trace2D B2 candidate verifier executable was not produced under {build_dir}")
    debug = [path for path in matches if any(part.casefold() == "debug" for part in path.parts)]
    return (debug[0] if debug else matches[0]).resolve()


def verify_trace2d(
    repo_root: Path,
    workspace: Path,
    cmake: str,
    configure_preset: str,
    build_dir: Path,
    timeout: float,
) -> dict[str, Any]:
    require_file(workspace / "B2Candidate.cpp", "Trace2D B2 candidate bridge source")
    before = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    build_dir.parent.mkdir(parents=True, exist_ok=True)
    configure = run(
        [
            cmake,
            "--preset",
            configure_preset,
            "-B",
            str(build_dir),
            f"-DTRACE2D_B2_CANDIDATE_DIR={workspace}",
        ],
        cwd=repo_root,
        timeout=timeout,
    )
    if configure["timed_out"] or configure["return_code"] != 0:
        return {
            "status": "error",
            "code": "candidate_verifier_configure_failed",
            "verifier": "b2-trace2d-qualified-v1",
            "configure": configure,
        }
    build = run(
        [
            cmake,
            "--build",
            str(build_dir),
            "--config",
            "Debug",
            "--target",
            TRACE2D_TARGET,
            "--parallel",
        ],
        cwd=repo_root,
        timeout=timeout,
    )
    if build["timed_out"] or build["return_code"] != 0:
        return {
            "status": "fail",
            "code": "candidate_compile_or_link_failed",
            "verifier": "b2-trace2d-qualified-v1",
            "configure": configure,
            "build": build,
        }
    executable = candidate_executable(build_dir)
    process = run([str(executable)], cwd=workspace, timeout=timeout)
    after = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    require(before == after, "preserved Trace2D candidate workspace mutated during verification")
    verdict = process_verdict(process, TRACE2D_PASS_MARKER, "b2-trace2d-qualified-v1")
    verdict.update(
        {
            "configure": configure,
            "build": build,
            "candidate_verifier_executable": str(executable),
            "workspace_sha256_before": before,
            "workspace_sha256_after": after,
        }
    )
    return verdict


def verify(args: argparse.Namespace) -> dict[str, Any]:
    repo_root = Path(args.repo_root).resolve() if args.repo_root else Path(__file__).resolve().parents[1]
    try:
        benchmark_b2_execution_freeze.validate_repository(repo_root)
    except Exception as exc:
        raise CandidateVerifierError(f"B2 execution freeze validation failed: {exc}") from exc
    workspace = require_dir(Path(args.workspace), "candidate workspace")
    require(args.task == TASK_ID, f"unknown B2 task: {args.task}")
    require(args.lane in LANES, f"unknown B2 lane: {args.lane}")
    timeout = float(args.timeout)
    if args.lane.startswith("godot."):
        verdict = verify_godot(repo_root, workspace, resolve_godot(args.godot_bin), timeout)
    else:
        verdict = verify_trace2d(
            repo_root,
            workspace,
            args.cmake,
            args.configure_preset,
            Path(args.trace2d_build_dir).expanduser().resolve(),
            timeout,
        )
    return {
        "schema_version": 1,
        "kind": "trace2d_b2_candidate_verification",
        "benchmark_id": "trace2d-b2",
        "task_id": args.task,
        "lane_id": args.lane,
        "workspace": str(workspace),
        "verdict": verdict,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify one frozen B2 candidate workspace")
    parser.add_argument("--task", required=True, choices=(TASK_ID,))
    parser.add_argument("--lane", required=True, choices=LANES)
    parser.add_argument("--workspace", required=True)
    parser.add_argument("--repo-root")
    parser.add_argument("--godot-bin")
    parser.add_argument("--trace2d-build-dir", default=".trace2d-benchmark-b2/candidate-verifier-build")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--configure-preset", default="windows-msvc")
    parser.add_argument("--timeout", type=float, default=180.0)
    parser.add_argument("--output")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        result = verify(args)
    except CandidateVerifierError as exc:
        result = {
            "schema_version": 1,
            "kind": "trace2d_b2_candidate_verification",
            "task_id": args.task,
            "lane_id": args.lane,
            "verdict": {"status": "error", "code": "verifier_setup_error", "message": str(exc)},
        }
    encoded = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        output = Path(args.output).expanduser().resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    status = result.get("verdict", {}).get("status")
    return 0 if status == "pass" else 1 if status == "fail" else 2


if __name__ == "__main__":
    raise SystemExit(main())
