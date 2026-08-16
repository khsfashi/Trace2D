#!/usr/bin/env python3
"""Owner-local initial-scoring harness for frozen Benchmark B2.

The harness executes exactly one preregistered slot at a time. It refuses slot
skips/reruns, never retries an Agent turn, applies the already-qualified
independent verifier, retains presentation captures separately from the gameplay
verdict, and appends each scheduled attempt to B0's hash-chained raw record.
"""
from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import benchmark_b0
import benchmark_b0_stable_harness
import benchmark_b2_execution_freeze

BENCHMARK_ID = "trace2d-b2"
TASK_ID = "b2-topdown-combat-v1"
ADAPTER_MODULE = "benchmark_b2_codex_windows_acl_wrapper"
EXECUTION_PATH = Path("benchmarks/b2/execution-v1.json")
PREREG_PATH = Path("benchmarks/b2/preregistration-v1.json")
PROMPT_PATH = Path("benchmarks/b2/tasks/b2-topdown-combat-v1/PROMPT.md")
PROFILE_PATH = Path("benchmarks/b0/agent-profile.codex-0.144.6.json")
PRESENTATION_ROOT = Path(".trace2d-b2-evidence/presentation")
PRESENTATION_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp", ".bmp"}
FAILURE_DOMAINS = {
    "success": "success",
    "budget_exceeded": "implementation",
    "timeout": "implementation",
    "agent_setup_failure": "infrastructure",
    "tool_transport_failure": "infrastructure",
    "verifier_failure": "infrastructure",
    "deterministic_failure": "implementation",
    "presentation_missing": "implementation",
    "human_intervention": "human",
    "benchmark_integrity_failure": "integrity",
}


class B2HarnessError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise B2HarnessError(f"failed to read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise B2HarnessError(f"expected JSON object: {path}")
    return value


def sha256_file(path: Path) -> str:
    return benchmark_b0.sha256_file(path)


def run_process(
    argv: list[str], *, cwd: Path, timeout: float, env: dict[str, str] | None = None
) -> dict[str, Any]:
    process_env = os.environ.copy()
    if env:
        process_env.update(env)
    started = time.perf_counter_ns()
    try:
        completed = subprocess.run(
            argv,
            cwd=cwd,
            env=process_env,
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


def load_contract(repo_root: Path) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    try:
        benchmark_b2_execution_freeze.validate_repository(repo_root)
    except Exception as exc:
        raise B2HarnessError(f"B2 execution freeze validation failed: {exc}") from exc
    execution = load_json(repo_root / EXECUTION_PATH)
    prereg = load_json(repo_root / PREREG_PATH)
    profile = load_json(repo_root / PROFILE_PATH)
    if execution.get("state") != "frozen_pre_score":
        raise B2HarnessError("B2 execution freeze is not in frozen_pre_score state")
    if prereg.get("scoring_gate", {}).get("allowed") is not True:
        raise B2HarnessError("B2 scoring gate is not open")
    if prereg.get("scoring_gate", {}).get("scored_results_observed") is not False:
        raise B2HarnessError("committed B2 scoring-gate history changed unexpectedly")
    canonical = benchmark_b0.sha256_json(profile)
    expected = prereg.get("agent_identity", {}).get("canonical_sha256")
    if canonical != expected:
        raise B2HarnessError("frozen B2 Agent profile canonical SHA-256 mismatch")
    slots = execution.get("slots")
    if not isinstance(slots, list) or len(slots) != 9:
        raise B2HarnessError("frozen B2 execution schedule must contain exactly nine slots")
    return execution, prereg, profile


def existing_records(raw_path: Path, execution: dict[str, Any]) -> list[dict[str, Any]]:
    if not raw_path.exists():
        return []
    records = benchmark_b0.verify_jsonl_chain(raw_path)
    slots = execution["slots"]
    if len(records) > len(slots):
        raise B2HarnessError("raw record contains more attempts than the frozen B2 schedule")
    for index, record in enumerate(records):
        expected = slots[index]
        if record.get("benchmark_id") != BENCHMARK_ID or record.get("phase") != "initial":
            raise B2HarnessError("raw record contains a non-initial or foreign B2 record")
        if record.get("slot") != expected["slot"]:
            raise B2HarnessError("raw record slot order differs from frozen schedule")
        if record.get("lane_id") != expected["lane"] or record.get("task_id") != expected["task"]:
            raise B2HarnessError("raw record task/lane differs from frozen schedule")
        if record.get("repetition") != expected["repetition"]:
            raise B2HarnessError("raw record repetition differs from frozen schedule")
    return records


def verify_starter(workspace: Path, lane: str, execution: dict[str, Any]) -> None:
    entry = execution["lane_starters"][lane]
    files = entry["files"]
    observed = sorted(
        path.relative_to(workspace).as_posix()
        for path in workspace.rglob("*")
        if path.is_file()
    )
    if observed != sorted(files):
        raise B2HarnessError(f"copied starter file set drifted for {lane}: {observed}")
    for relative, expected in files.items():
        actual = sha256_file(workspace / relative)
        if actual != expected:
            raise B2HarnessError(f"copied starter SHA-256 mismatch for {lane}/{relative}")


def agent_command(workspace: Path, prompt: Path, lane: str, result: Path) -> list[str]:
    return [
        sys.executable,
        "-m",
        ADAPTER_MODULE,
        "run",
        "--workspace",
        str(workspace),
        "--prompt-file",
        str(prompt),
        "--lane",
        lane,
        "--result-file",
        str(result),
    ]


def verifier_command(repo_root: Path, workspace: Path, lane: str, output: Path, slot: int) -> list[str]:
    build_root = os.environ.get("TRACE2D_B2_VERIFY_BUILD_ROOT", "").strip()
    if not build_root:
        build_root = str(repo_root / ".trace2d-benchmark-b2" / "candidate-verifier")
    command = [
        sys.executable,
        str(repo_root / "scripts/verify_benchmark_b2_candidate.py"),
        "--task",
        TASK_ID,
        "--lane",
        lane,
        "--workspace",
        str(workspace),
        "--repo-root",
        str(repo_root),
        "--trace2d-build-dir",
        str(Path(build_root) / f"slot-{slot:02d}"),
        "--output",
        str(output),
    ]
    godot = os.environ.get("TRACE2D_B2_GODOT_BIN") or os.environ.get("TRACE2D_BENCH_GODOT_BIN")
    if godot:
        command.extend(["--godot-bin", godot])
    return command


def presentation_files(workspace: Path) -> list[dict[str, Any]]:
    root = workspace / PRESENTATION_ROOT
    if not root.is_dir():
        return []
    result: list[dict[str, Any]] = []
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        if path.suffix.casefold() not in PRESENTATION_SUFFIXES:
            continue
        result.append(
            {
                "path": path.relative_to(workspace).as_posix(),
                "sha256": sha256_file(path),
                "bytes": path.stat().st_size,
            }
        )
    return result


def classify(
    process: dict[str, Any],
    agent_result: dict[str, Any] | None,
    identity_ok: bool,
    verifier: dict[str, Any] | None,
    captures: list[dict[str, Any]],
    integrity_ok: bool,
) -> tuple[str, str]:
    if not integrity_ok:
        status = "benchmark_integrity_failure"
    elif process["timed_out"]:
        status = "timeout"
    elif agent_result is None or not identity_ok:
        status = "agent_setup_failure"
    elif int(agent_result.get("human_interventions", 0)) != 0:
        status = "human_intervention"
    elif agent_result.get("status") == "budget_exceeded":
        status = "budget_exceeded"
    elif agent_result.get("status") == "tool_transport_failure":
        status = "tool_transport_failure"
    elif agent_result.get("status") != "completed" or process["return_code"] != 0:
        status = "agent_setup_failure"
    elif verifier is None or verifier.get("verdict", {}).get("status") == "error":
        status = "verifier_failure"
    elif verifier.get("verdict", {}).get("status") != "pass":
        status = "deterministic_failure"
    elif not captures:
        status = "presentation_missing"
    else:
        status = "success"
    return status, FAILURE_DOMAINS[status]


def run_slot(args: argparse.Namespace) -> dict[str, Any]:
    repo_root = repository_root()
    execution, prereg, profile = load_contract(repo_root)
    runs_root = Path(args.runs_root).expanduser().resolve()
    runs_root.mkdir(parents=True, exist_ok=True)
    raw_path = runs_root / "raw.jsonl"
    records = existing_records(raw_path, execution)
    next_index = len(records)
    if next_index >= len(execution["slots"]):
        raise B2HarnessError("all nine frozen initial B2 slots are already recorded")
    expected = execution["slots"][next_index]
    if args.slot != expected["slot"]:
        raise B2HarnessError(
            f"next frozen B2 slot is {expected['slot']} ({expected['lane']}), not requested slot {args.slot}"
        )

    slot = int(expected["slot"])
    lane = str(expected["lane"])
    trial_id = f"slot-{slot:02d}-{lane.replace('.', '-')}-r{expected['repetition']}"
    trial_root = runs_root / "trials" / trial_id
    if trial_root.exists():
        raise B2HarnessError(f"slot trial directory already exists; rerun forbidden: {trial_root}")
    trial_root.mkdir(parents=True)
    workspace = trial_root / "workspace"
    starter = repo_root / execution["lane_starters"][lane]["root"]
    shutil.copytree(starter, workspace)
    verify_starter(workspace, lane, execution)

    prompt = trial_root / "frozen-task-prompt.md"
    shutil.copy2(repo_root / PROMPT_PATH, prompt)
    if sha256_file(prompt) != sha256_file(repo_root / PROMPT_PATH):
        raise B2HarnessError("frozen B2 task prompt copy drifted")

    agent_result_path = trial_root / "agent-result.json"
    verifier_result_path = trial_root / "verifier-result.json"
    budget = prereg["budget"]
    started_at = utc_now()
    process = run_process(
        agent_command(workspace, prompt, lane, agent_result_path),
        cwd=workspace,
        timeout=float(budget["wall_seconds"]),
        env={
            "TRACE2D_BENCH_SUITE_ID": BENCHMARK_ID,
            "TRACE2D_BENCH_LANE": lane,
            "TRACE2D_BENCH_TASK": TASK_ID,
            "TRACE2D_BENCH_WORKSPACE": str(workspace),
            "TRACE2D_BENCH_PROMPT_FILE": str(prompt),
            "TRACE2D_BENCH_AGENT_RESULT": str(agent_result_path),
            "TRACE2D_BENCH_PRESENTATION_ROOT": str(PRESENTATION_ROOT),
            "TRACE2D_BENCH_MAX_TOOL_CALLS": str(budget["max_tool_calls"]),
            "TRACE2D_BENCH_MAX_INPUT_TOKENS": str(budget["max_input_tokens"]),
            "TRACE2D_BENCH_MAX_OUTPUT_TOKENS": str(budget["max_output_tokens"]),
            "TRACE2D_BENCH_WRAPPER_TIMEOUT": str(max(1, int(budget["wall_seconds"]) - 15)),
        },
    )
    (trial_root / "adapter.stdout.txt").write_text(process["stdout"], encoding="utf-8")
    (trial_root / "adapter.stderr.txt").write_text(process["stderr"], encoding="utf-8")

    agent_result: dict[str, Any] | None = None
    if agent_result_path.is_file():
        try:
            agent_result = load_json(agent_result_path)
        except B2HarnessError:
            agent_result = None
    identity_ok = False
    if agent_result is not None:
        identity = agent_result.get("model", {})
        identity_ok = (
            identity.get("agent_id") == profile.get("agent_id")
            and identity.get("model_id") == profile.get("model_id")
            and identity.get("model_revision") == profile.get("model_revision")
        )

    integrity_ok = True
    try:
        benchmark_b2_execution_freeze.validate_repository(repo_root)
    except Exception:
        integrity_ok = False

    verifier: dict[str, Any] | None = None
    verifier_process: dict[str, Any] | None = None
    if integrity_ok and not process["timed_out"]:
        verifier_process = run_process(
            verifier_command(repo_root, workspace, lane, verifier_result_path, slot),
            cwd=repo_root,
            timeout=300.0,
        )
        (trial_root / "verifier.stdout.txt").write_text(verifier_process["stdout"], encoding="utf-8")
        (trial_root / "verifier.stderr.txt").write_text(verifier_process["stderr"], encoding="utf-8")
        if verifier_result_path.is_file():
            try:
                verifier = load_json(verifier_result_path)
            except B2HarnessError:
                verifier = None

    captures = presentation_files(workspace)
    status, failure_domain = classify(
        process, agent_result, identity_ok, verifier, captures, integrity_ok
    )
    metrics = agent_result.get("metrics", {}) if agent_result else {}
    record = {
        "schema_version": 1,
        "kind": "trace2d_b2_scored_attempt",
        "benchmark_id": BENCHMARK_ID,
        "phase": "initial",
        "scored": True,
        "slot": slot,
        "repetition": int(expected["repetition"]),
        "task_id": TASK_ID,
        "lane_id": lane,
        "trial_id": trial_id,
        "started_at": started_at,
        "finished_at": utc_now(),
        "execution_contract_sha256": sha256_file(repo_root / EXECUTION_PATH),
        "preregistration_sha256": sha256_file(repo_root / PREREG_PATH),
        "task_prompt_sha256": sha256_file(repo_root / PROMPT_PATH),
        "agent_profile_sha256": benchmark_b0.sha256_json(profile),
        "status": status,
        "failure_domain": failure_domain,
        "agent_identity_ok": identity_ok,
        "agent_result": agent_result,
        "metrics": {
            "revisions": int(metrics.get("revisions", 0)),
            "tool_calls": int(metrics.get("tool_calls", 0)),
            "input_tokens": int(metrics.get("input_tokens", 0)),
            "output_tokens": int(metrics.get("output_tokens", 0)),
            "visual_feedback_calls": int(metrics.get("visual_feedback_calls", 0)),
            "human_interventions": int((agent_result or {}).get("human_interventions", 0)),
            "wall_ms": float(process["duration_ms"]),
            "verifier_ms": float((verifier_process or {}).get("duration_ms", 0.0)),
            "normalized_operations": metrics.get("normalized_operations", {}),
            "engine_native_operations": metrics.get("engine_native_operations", {}),
        },
        "deterministic_verifier": verifier,
        "presentation": {
            "required": True,
            "authoritative_for_gameplay": False,
            "capture_root": PRESENTATION_ROOT.as_posix(),
            "captures": captures,
        },
        "workspace_sha256": benchmark_b0_stable_harness.stable_tree_hash(workspace),
        "environment": {
            "os": platform.system(),
            "os_release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "godot_path": os.environ.get("TRACE2D_B2_GODOT_BIN", os.environ.get("TRACE2D_BENCH_GODOT_BIN", "")),
            "trace2d_path": os.environ.get("TRACE2D_BENCH_TRACE2D_BIN", ""),
        },
        "integrity": {
            "schedule_prefix_valid": True,
            "repo_freeze_unchanged_after_agent": integrity_ok,
            "automatic_retries": 0,
            "replacement_trials": 0,
        },
        "artifacts": {
            "trial_root": str(trial_root),
            "workspace": str(workspace),
            "agent_result": str(agent_result_path),
            "verifier_result": str(verifier_result_path),
            "adapter_stdout": str(trial_root / "adapter.stdout.txt"),
            "adapter_stderr": str(trial_root / "adapter.stderr.txt"),
            "verifier_stdout": str(trial_root / "verifier.stdout.txt"),
            "verifier_stderr": str(trial_root / "verifier.stderr.txt"),
        },
    }
    return benchmark_b0.append_hash_chained_jsonl(raw_path, record)


def next_slot(args: argparse.Namespace) -> dict[str, Any]:
    repo_root = repository_root()
    execution, _, _ = load_contract(repo_root)
    runs_root = Path(args.runs_root).expanduser().resolve()
    records = existing_records(runs_root / "raw.jsonl", execution)
    if len(records) == len(execution["slots"]):
        return {"benchmark_id": BENCHMARK_ID, "initial_scoring_complete": True, "next_slot": None}
    slot = execution["slots"][len(records)]
    return {
        "benchmark_id": BENCHMARK_ID,
        "initial_scoring_complete": False,
        "completed_slots": len(records),
        "next_slot": slot,
    }


def validate(_: argparse.Namespace) -> dict[str, Any]:
    repo_root = repository_root()
    execution, prereg, profile = load_contract(repo_root)
    return {
        "benchmark_id": BENCHMARK_ID,
        "scoring_gate_open": prereg["scoring_gate"]["allowed"],
        "slot_count": len(execution["slots"]),
        "agent_profile_sha256": benchmark_b0.sha256_json(profile),
        "execution_contract_sha256": sha256_file(repo_root / EXECUTION_PATH),
        "valid": True,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D Benchmark B2 owner-local scored harness")
    commands = parser.add_subparsers(dest="command", required=True)

    check = commands.add_parser("validate")
    check.set_defaults(handler=validate)

    upcoming = commands.add_parser("next-slot")
    upcoming.add_argument("--runs-root", required=True)
    upcoming.set_defaults(handler=next_slot)

    run = commands.add_parser("run-slot")
    run.add_argument("--runs-root", required=True)
    run.add_argument("--slot", type=int, required=True, choices=range(1, 10))
    run.set_defaults(handler=run_slot)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        result = args.handler(args)
    except (B2HarnessError, OSError, subprocess.SubprocessError, ValueError) as exc:
        print(f"B2 scored harness error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
