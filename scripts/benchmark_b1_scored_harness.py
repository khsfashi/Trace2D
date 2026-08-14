#!/usr/bin/env python3
"""Owner-local scored harness for the frozen Trace2D Benchmark B1 cohort.

B1 reuses B0's append-only hash chain, raw metric vocabulary, stable authored
workspace hashing and aggregate reporter. It adds only B1-specific suite policy,
selected environment adapter invocation and frozen candidate-verifier dispatch.

The logical Agent profile remains the frozen B0 profile. B1 records its execution
adapter separately because the preregistered strongest Godot baseline uses a
different bridge lifecycle than B0. No task answer, verifier rule, or hidden
production authority is supplied through that adapter.
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
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import benchmark_b0
import benchmark_b0_stable_harness
import benchmark_b1
import benchmark_b1_scored_policy

SCHEMA_VERSION = 1
EXECUTION_ADAPTER_MODULE = "benchmark_b1_codex_windows_acl_wrapper"
FAILURE_DOMAINS = {
    "success": "success",
    "environment_failure": "infrastructure",
    "harness_setup_failure": "infrastructure",
    "agent_setup_failure": "infrastructure",
    "tool_transport_failure": "infrastructure",
    "timeout": "implementation",
    "budget_exceeded": "implementation",
    "engine_build_test_failure": "implementation",
    "verifier_failure": "infrastructure",
    "human_intervention": "human",
    "benchmark_integrity_failure": "integrity",
}


class B1HarnessError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise B1HarnessError(f"failed to read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise B1HarnessError(f"expected JSON object: {path}")
    return value


def _sha256_file(path: Path) -> str:
    return benchmark_b0.sha256_file(path)


def _run(
    argv: list[str],
    *,
    cwd: Path,
    timeout: float,
    env: dict[str, str] | None = None,
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


def _find_task(suite: dict[str, Any], task_id: str) -> dict[str, Any]:
    for task in suite.get("tasks", []):
        if isinstance(task, dict) and task.get("id") == task_id:
            return task
    raise B1HarnessError(f"unknown B1 task: {task_id}")


def _find_lane(suite: dict[str, Any], lane_id: str) -> dict[str, Any]:
    for lane in suite.get("lanes", []):
        if isinstance(lane, dict) and lane.get("id") == lane_id:
            return lane
    raise B1HarnessError(f"unknown B1 lane: {lane_id}")


def _load_contract(repo_root: Path) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], list[dict[str, Any]]]:
    policy_path = repo_root / "benchmarks/b1/scored-cohort-v1.json"
    policy, schedule = benchmark_b1_scored_policy.load_and_validate_policy(policy_path, repo_root)
    suite = benchmark_b1.load_and_validate_suite(repo_root / "benchmarks/b1/suite.json", repo_root)
    profile = _load_json(repo_root / str(policy["agent_profile"]))
    return policy, suite, profile, schedule


def _protected_snapshot(repo_root: Path) -> dict[str, str]:
    manifest = _load_json(repo_root / "benchmarks/b1/freeze-manifest.json")
    protected: list[Path] = []
    for entry in manifest.get("files", []):
        if not isinstance(entry, dict) or not isinstance(entry.get("path"), str):
            raise B1HarnessError("B1 freeze manifest contains malformed file entry")
        protected.append((repo_root / entry["path"]).resolve())
    protected.extend(
        [
            (repo_root / "benchmarks/b1/freeze-manifest.json").resolve(),
            (repo_root / "benchmarks/b1/fixture-qualification.json").resolve(),
            (repo_root / "benchmarks/b1/scored-cohort-v1.json").resolve(),
            (repo_root / "benchmarks/b1/godot-ai-python-freeze.txt").resolve(),
            (repo_root / "scripts/benchmark_b1_scored_harness.py").resolve(),
            (repo_root / "scripts/benchmark_b1_codex_windows_acl_wrapper.py").resolve(),
            (repo_root / "scripts/verify_benchmark_b1_candidate.py").resolve(),
        ]
    )
    result: dict[str, str] = {}
    for path in protected:
        if not path.is_file():
            raise B1HarnessError(f"protected benchmark file missing: {path}")
        result[str(path)] = _sha256_file(path)
    return result


def _protected_unchanged(snapshot: dict[str, str]) -> bool:
    for raw, expected in snapshot.items():
        path = Path(raw)
        if not path.is_file() or _sha256_file(path) != expected:
            return False
    return True


def _stable_tree_hash(root: Path) -> str:
    return benchmark_b0_stable_harness.stable_tree_hash(root)


def _agent_command(
    *,
    workspace: Path,
    prompt: Path,
    lane_id: str,
    result_file: Path,
) -> list[str]:
    return [
        sys.executable,
        "-m",
        EXECUTION_ADAPTER_MODULE,
        "run",
        "--workspace",
        str(workspace),
        "--prompt-file",
        str(prompt),
        "--lane",
        lane_id,
        "--result-file",
        str(result_file),
    ]


def _verifier_command(
    *,
    repo_root: Path,
    workspace: Path,
    task_id: str,
    lane_id: str,
    output: Path,
) -> list[str]:
    script = repo_root / "scripts/verify_benchmark_b1_candidate.py"
    build_dir = os.environ.get("TRACE2D_B1_ANIMATION_VERIFY_BUILD_DIR", "").strip()
    if not build_dir:
        build_dir = str(repo_root / ".trace2d-benchmark-b1" / "animation-verifier-build")
    command = [
        sys.executable,
        str(script),
        "--task",
        task_id,
        "--lane",
        lane_id,
        "--workspace",
        str(workspace),
        "--repo-root",
        str(repo_root),
        "--animation-build-dir",
        build_dir,
        "--output",
        str(output),
    ]
    godot = os.environ.get("TRACE2D_B1_GODOT_BIN") or os.environ.get("TRACE2D_BENCH_GODOT_BIN")
    if godot:
        command.extend(["--godot-bin", godot])
    native = os.environ.get("TRACE2D_B1_FIXTURE_VERIFY_BIN", "").strip()
    if native:
        command.extend(["--trace2d-fixture-verifier", native])
    return command


def _classify(
    *,
    process: dict[str, Any],
    agent_result: dict[str, Any] | None,
    verifier: dict[str, Any] | None,
    integrity_ok: bool,
    identity_ok: bool,
) -> tuple[str, str]:
    if not integrity_ok:
        status = "benchmark_integrity_failure"
    elif process["timed_out"]:
        status = "timeout"
    elif agent_result is None or not identity_ok:
        status = "agent_setup_failure"
    elif int(agent_result.get("human_interventions", 0)) > 0:
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
        status = "engine_build_test_failure"
    else:
        status = "success"
    return status, FAILURE_DOMAINS[status]


def _environment_snapshot(lane_id: str) -> dict[str, Any]:
    return {
        "os": platform.system(),
        "os_release": platform.release(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "godot_path": os.environ.get("TRACE2D_BENCH_GODOT_BIN", ""),
        "trace2d_path": os.environ.get("TRACE2D_BENCH_TRACE2D_BIN", ""),
        "godot_ai_python": os.environ.get("TRACE2D_B1_GODOT_AI_PYTHON", "") if lane_id == "godot.agent" else "",
        "engine_version": os.environ.get("TRACE2D_BENCH_ENGINE_VERSION", ""),
    }


def run_trial(args: argparse.Namespace) -> dict[str, Any]:
    repo_root = repository_root()
    policy, suite, profile, schedule = _load_contract(repo_root)
    task = _find_task(suite, args.task)
    lane = _find_lane(suite, args.lane)
    lane_task = task["lanes"][args.lane]

    if args.scored:
        scheduled = [
            item for item in schedule
            if item["task_id"] == args.task and item["lane_id"] == args.lane
        ]
        if not scheduled:
            raise B1HarnessError(f"task/lane is not preregistered for scoring: {args.task}/{args.lane}")

    profile_hash = benchmark_b0.sha256_json(profile)
    if profile_hash != policy["agent_profile_canonical_sha256"]:
        raise B1HarnessError("frozen Agent profile canonical hash mismatch")

    runs_root = Path(args.runs_root).expanduser().resolve()
    runs_root.mkdir(parents=True, exist_ok=True)
    trial_id = args.trial_id or f"{args.task}-{args.lane}-{uuid.uuid4().hex[:12]}"
    trial_root = runs_root / "trials" / trial_id
    if trial_root.exists():
        raise B1HarnessError(f"trial directory already exists: {trial_root}")
    trial_root.mkdir(parents=True)
    workspace = trial_root / "workspace"
    starter = (repo_root / lane_task["starter"]).resolve()
    shutil.copytree(starter, workspace)
    prompt = trial_root / "prompt.md"
    shutil.copy2((repo_root / task["prompt"]).resolve(), prompt)

    agent_result_path = trial_root / "agent-result.json"
    verifier_result_path = trial_root / "verifier-result.json"
    protected = _protected_snapshot(repo_root)
    started_at = utc_now()
    process = _run(
        _agent_command(
            workspace=workspace,
            prompt=prompt,
            lane_id=args.lane,
            result_file=agent_result_path,
        ),
        cwd=workspace,
        timeout=float(task["budget"]["wall_seconds"]),
        env={
            "TRACE2D_BENCH_SUITE_ID": "trace2d-b1",
            "TRACE2D_BENCH_LANE": args.lane,
            "TRACE2D_BENCH_TASK": args.task,
            "TRACE2D_BENCH_WORKSPACE": str(workspace),
            "TRACE2D_BENCH_PROMPT_FILE": str(prompt),
            "TRACE2D_BENCH_AGENT_RESULT": str(agent_result_path),
            "TRACE2D_BENCH_MAX_TOOL_CALLS": str(task["budget"]["max_tool_calls"]),
            "TRACE2D_BENCH_MAX_INPUT_TOKENS": str(task["budget"]["max_input_tokens"]),
            "TRACE2D_BENCH_MAX_OUTPUT_TOKENS": str(task["budget"]["max_output_tokens"]),
        },
    )
    (trial_root / "adapter.stdout.txt").write_text(process["stdout"], encoding="utf-8")
    (trial_root / "adapter.stderr.txt").write_text(process["stderr"], encoding="utf-8")

    agent_result: dict[str, Any] | None = None
    if agent_result_path.is_file():
        try:
            agent_result = _load_json(agent_result_path)
        except B1HarnessError:
            agent_result = None

    identity_ok = False
    if agent_result is not None:
        identity = agent_result.get("model", {})
        identity_ok = (
            identity.get("agent_id") == profile["agent_id"]
            and identity.get("model_id") == profile["model_id"]
            and identity.get("model_revision") == profile["model_revision"]
        )

    integrity_ok = _protected_unchanged(protected)
    verifier: dict[str, Any] | None = None
    verifier_process: dict[str, Any] | None = None
    if integrity_ok and not process["timed_out"]:
        verifier_process = _run(
            _verifier_command(
                repo_root=repo_root,
                workspace=workspace,
                task_id=args.task,
                lane_id=args.lane,
                output=verifier_result_path,
            ),
            cwd=repo_root,
            timeout=180.0,
        )
        (trial_root / "verifier.stdout.txt").write_text(verifier_process["stdout"], encoding="utf-8")
        (trial_root / "verifier.stderr.txt").write_text(verifier_process["stderr"], encoding="utf-8")
        if verifier_result_path.is_file():
            try:
                verifier = _load_json(verifier_result_path)
            except B1HarnessError:
                verifier = None

    status, failure_domain = _classify(
        process=process,
        agent_result=agent_result,
        verifier=verifier,
        integrity_ok=integrity_ok,
        identity_ok=identity_ok,
    )
    metrics = agent_result.get("metrics", {}) if agent_result else {}
    record = {
        "schema_version": SCHEMA_VERSION,
        "suite_id": suite["suite_id"],
        "suite_sha256": _sha256_file(repo_root / "benchmarks/b1/suite.json"),
        "policy_sha256": _sha256_file(repo_root / "benchmarks/b1/scored-cohort-v1.json"),
        "task_id": args.task,
        "task_version": task["version"],
        "lane_id": args.lane,
        "trial_id": trial_id,
        "scored": bool(args.scored),
        "started_at": started_at,
        "finished_at": utc_now(),
        "agent_profile_sha256": profile_hash,
        "agent": {
            "agent_id": profile["agent_id"],
            "model_id": profile["model_id"],
            "model_revision": profile["model_revision"],
            "settings": profile.get("settings", {}),
            "budget": profile["budget"],
            "logical_profile_command": profile.get("command", []),
        },
        "execution_adapter": {
            "module": EXECUTION_ADAPTER_MODULE,
            "role": "b1_environment_bootstrap_only",
            "selected_godot_bridge": lane.get("bridge") if args.lane == "godot.agent" else None,
            "benchmark_only_trace2d_scene_injected": False,
        },
        "lane": {
            "engine": lane["engine"],
            "adapter": lane["adapter"],
            "bridge": lane.get("bridge"),
        },
        "environment": _environment_snapshot(args.lane),
        "result": {
            "status": status,
            "failure_domain": failure_domain,
            "agent_identity_match": identity_ok,
            "agent_return_code": process["return_code"],
            "agent_timed_out": process["timed_out"],
        },
        "metrics": {
            "revisions": int(metrics.get("revisions", 0)),
            "tool_calls": int(metrics.get("tool_calls", 0)),
            "input_tokens": int(metrics.get("input_tokens", 0)),
            "output_tokens": int(metrics.get("output_tokens", 0)),
            "wall_ms": float(process["duration_ms"]),
            "verifier_ms": float((verifier_process or {}).get("duration_ms", 0.0)),
            "human_interventions": int((agent_result or {}).get("human_interventions", 0)),
            "normalized_operations": metrics.get("normalized_operations", {}),
            "engine_native_operations": metrics.get("engine_native_operations", {}),
        },
        "verifier": verifier,
        "workspace_sha256": _stable_tree_hash(workspace),
        "artifacts": {
            "trial_root": str(trial_root),
            "workspace": str(workspace),
            "agent_result": str(agent_result_path),
            "verifier_result": str(verifier_result_path),
            "adapter_stdout": str(trial_root / "adapter.stdout.txt"),
            "adapter_stderr": str(trial_root / "adapter.stderr.txt"),
        },
    }
    return benchmark_b0.append_hash_chained_jsonl(runs_root / "raw.jsonl", record)


def reverify(args: argparse.Namespace) -> dict[str, Any]:
    repo_root = repository_root()
    _load_contract(repo_root)
    records = benchmark_b0.verify_jsonl_chain(Path(args.records).expanduser().resolve())
    original = next((record for record in records if record.get("trial_id") == args.trial_id), None)
    if original is None:
        raise B1HarnessError(f"trial not found: {args.trial_id}")
    workspace = Path(original["artifacts"]["workspace"])
    if not workspace.is_dir():
        raise B1HarnessError(f"trial workspace missing: {workspace}")

    verifier_path = Path(args.replay_records).expanduser().resolve().parent / "reverify" / f"{args.trial_id}.json"
    process = _run(
        _verifier_command(
            repo_root=repo_root,
            workspace=workspace,
            task_id=str(original["task_id"]),
            lane_id=str(original["lane_id"]),
            output=verifier_path,
        ),
        cwd=repo_root,
        timeout=180.0,
    )
    verifier = _load_json(verifier_path) if verifier_path.is_file() else None
    artifact_hash = _stable_tree_hash(workspace)
    original_verdict = (original.get("verifier") or {}).get("verdict", {}).get("status")
    current_verdict = (verifier or {}).get("verdict", {}).get("status")
    replay = {
        "schema_version": SCHEMA_VERSION,
        "kind": "independent_reverify",
        "suite_id": "trace2d-b1",
        "trial_id": args.trial_id,
        "original_record_sha256": original["record_sha256"],
        "lane_id": original["lane_id"],
        "task_id": original["task_id"],
        "workspace_sha256": artifact_hash,
        "workspace_matches_original": artifact_hash == original["workspace_sha256"],
        "verifier": verifier,
        "verifier_process_return_code": process["return_code"],
        "verdict_matches_original": current_verdict == original_verdict,
        "generated_at": utc_now(),
    }
    return benchmark_b0.append_hash_chained_jsonl(
        Path(args.replay_records).expanduser().resolve(), replay
    )


def report(args: argparse.Namespace) -> dict[str, Any]:
    return benchmark_b0.report_records(
        Path(args.records).expanduser().resolve(), bool(args.include_unscored)
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D Benchmark B1 scored harness")
    commands = parser.add_subparsers(dest="command", required=True)

    run = commands.add_parser("run-trial")
    run.add_argument("--task", required=True, choices=benchmark_b1.EXPECTED_TASKS)
    run.add_argument("--lane", required=True, choices=benchmark_b1.EXPECTED_LANES)
    run.add_argument("--runs-root", required=True)
    run.add_argument("--trial-id")
    run.add_argument("--scored", action="store_true")
    run.set_defaults(handler=run_trial)

    replay = commands.add_parser("reverify")
    replay.add_argument("--trial-id", required=True)
    replay.add_argument("--records", required=True)
    replay.add_argument("--replay-records", required=True)
    replay.set_defaults(handler=reverify)

    aggregate = commands.add_parser("report")
    aggregate.add_argument("--records", required=True)
    aggregate.add_argument("--include-unscored", action="store_true")
    aggregate.set_defaults(handler=report)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        result = args.handler(args)
    except (B1HarnessError, benchmark_b1_scored_policy.ScoredPolicyError, OSError, subprocess.SubprocessError) as exc:
        print(f"B1 scored harness error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.command == "reverify":
        ok = result.get("workspace_matches_original") is True and result.get("verdict_matches_original") is True
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
