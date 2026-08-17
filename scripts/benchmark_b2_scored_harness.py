#!/usr/bin/env python3
"""Owner-local initial-scoring harness for frozen Benchmark B2.

Runs exactly one preregistered slot at a time. The harness rejects slot skips and
reruns, performs only non-scored environment preflight before a slot is consumed,
uses the already-qualified independent verifier, retains presentation evidence
separately, and appends every started attempt to B0's hash-chained raw record.
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

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
for _path in (REPO_ROOT, SCRIPT_DIR):
    _value = str(_path)
    if _value not in sys.path:
        sys.path.insert(0, _value)

import benchmark_b0  # noqa: E402
import benchmark_b0_codex_wrapper as codex_core  # noqa: E402
import benchmark_b0_stable_harness  # noqa: E402
import benchmark_b2_execution_freeze  # noqa: E402

BENCHMARK_ID = "trace2d-b2"
TASK_ID = "b2-topdown-combat-v1"
ADAPTER_PATH = SCRIPT_DIR / "benchmark_b2_codex_windows_acl_wrapper.py"
VERIFIER_PATH = SCRIPT_DIR / "verify_benchmark_b2_candidate.py"
EXECUTION_PATH = Path("benchmarks/b2/execution-v1.json")
PREREG_PATH = Path("benchmarks/b2/preregistration-v1.json")
PROMPT_PATH = Path("benchmarks/b2/tasks/b2-topdown-combat-v1/PROMPT.md")
PROFILE_PATH = Path("benchmarks/b0/agent-profile.codex-0.144.6.json")
PRESENTATION_ROOT = Path(".trace2d-b2-evidence/presentation")
PRESENTATION_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp", ".bmp"}
ATTEMPT_START_CHECKPOINT = "attempt-start.json"
AGENT_PROCESS_CHECKPOINT = "agent-process.json"
GODOT_ENGINE_ID = "4.7.1.stable.official.a13da4feb"
GODOT_AI_VERSION = "3.1.5"
GODOT_AI_COMMIT = "09a1e3311015153d967710fbe6502ac519585a9b"
GODOT_AI_PACKAGE = "sha256:51863ba177c66299808aa19ef6cd9069768915b2434d7787b9300e40c3620b04"
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


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise B2HarnessError(f"failed to read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise B2HarnessError(f"expected JSON object: {path}")
    return value


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    encoded = json.dumps(value, indent=2, ensure_ascii=False) + "\n"
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


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


def load_contract() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    try:
        benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    except Exception as exc:
        raise B2HarnessError(f"B2 execution freeze validation failed: {exc}") from exc
    execution = load_json(REPO_ROOT / EXECUTION_PATH)
    prereg = load_json(REPO_ROOT / PREREG_PATH)
    profile = load_json(REPO_ROOT / PROFILE_PATH)
    if execution.get("state") != "frozen_pre_score":
        raise B2HarnessError("B2 execution freeze is not frozen_pre_score")
    gate = prereg.get("scoring_gate", {})
    if gate.get("allowed") is not True or gate.get("scored_results_observed") is not False:
        raise B2HarnessError("committed B2 scoring gate is not the frozen open pre-score state")
    profile_hash = benchmark_b0.sha256_json(profile)
    if profile_hash != prereg.get("agent_identity", {}).get("canonical_sha256"):
        raise B2HarnessError("frozen B2 Agent profile canonical SHA-256 mismatch")
    slots = execution.get("slots")
    if not isinstance(slots, list) or len(slots) != 9:
        raise B2HarnessError("frozen B2 schedule must contain exactly nine slots")
    return execution, prereg, profile


def existing_records(raw_path: Path, execution: dict[str, Any]) -> list[dict[str, Any]]:
    if not raw_path.exists():
        return []
    records = benchmark_b0.verify_jsonl_chain(raw_path)
    slots = execution["slots"]
    if len(records) > len(slots):
        raise B2HarnessError("raw record contains more attempts than the frozen schedule")
    for index, record in enumerate(records):
        expected = slots[index]
        if record.get("benchmark_id") != BENCHMARK_ID or record.get("phase") != "initial":
            raise B2HarnessError("raw record contains a foreign/non-initial record")
        if record.get("slot") != expected["slot"]:
            raise B2HarnessError("raw record slot order differs from frozen schedule")
        if record.get("task_id") != expected["task"] or record.get("lane_id") != expected["lane"]:
            raise B2HarnessError("raw record task/lane differs from frozen schedule")
        if record.get("repetition") != expected["repetition"]:
            raise B2HarnessError("raw record repetition differs from frozen schedule")
    return records


def next_frozen_slot(runs_root: Path, execution: dict[str, Any]) -> tuple[list[dict[str, Any]], dict[str, Any] | None]:
    records = existing_records(runs_root / "raw.jsonl", execution)
    if len(records) == len(execution["slots"]):
        return records, None
    return records, execution["slots"][len(records)]


def require_file_from_env(names: tuple[str, ...], label: str) -> Path:
    raw = next((os.environ.get(name, "").strip() for name in names if os.environ.get(name, "").strip()), "")
    if not raw:
        raise B2HarnessError(f"{label} requires one of: {', '.join(names)}")
    path = Path(raw).expanduser().resolve()
    if not path.is_file():
        raise B2HarnessError(f"{label} not found: {path}")
    return path


def preflight_environment(lane: str) -> dict[str, Any]:
    if os.name != "nt":
        raise B2HarnessError("scored B2 slots require the qualified native Windows ACL host")

    codex = codex_core.resolve_codex(os.environ.get("TRACE2D_BENCH_CODEX_BIN", "codex"))
    actual_codex = codex_core.verify_codex_version(codex, REPO_ROOT)
    auth_raw = os.environ.get("TRACE2D_BENCH_CODEX_AUTH_FILE", "").strip()
    auth = Path(auth_raw).expanduser() if auth_raw else Path.home() / ".codex" / "auth.json"
    if not auth.is_file():
        raise B2HarnessError(f"Codex auth file not found: {auth}")

    evidence: dict[str, Any] = {
        "lane": lane,
        "ready": True,
        "codex": {"path": str(Path(codex).resolve()), "version": actual_codex, "auth_file_present": True},
    }
    if lane.startswith("godot."):
        godot = require_file_from_env(
            ("TRACE2D_B2_GODOT_BIN", "TRACE2D_BENCH_GODOT_BIN"), "pinned Godot executable"
        )
        version = run_process([str(godot), "--version"], cwd=godot.parent, timeout=30.0)
        observed = str(version.get("stdout", "")).strip()
        if version["timed_out"] or version["return_code"] != 0 or observed != GODOT_ENGINE_ID:
            raise B2HarnessError(f"Godot identity mismatch: expected {GODOT_ENGINE_ID!r}, got {observed!r}")
        evidence["godot"] = {"path": str(godot), "version": observed}

    if lane == "godot.agent":
        python = require_file_from_env(("TRACE2D_B2_GODOT_AI_PYTHON",), "B2 Godot Agent Python")
        addon_raw = os.environ.get("TRACE2D_B2_GODOT_AI_ADDON_DIR", "").strip()
        if not addon_raw:
            raise B2HarnessError("godot.agent requires TRACE2D_B2_GODOT_AI_ADDON_DIR")
        addon = Path(addon_raw).expanduser().resolve()
        if not (addon / "plugin.cfg").is_file():
            raise B2HarnessError(f"B2 Godot Agent addon plugin.cfg not found: {addon}")
        version = run_process(
            [str(python), "-c", "import importlib.metadata as m; print(m.version('godot-ai'))"],
            cwd=python.parent,
            timeout=30.0,
        )
        observed = str(version.get("stdout", "")).strip()
        if version["timed_out"] or version["return_code"] != 0 or observed != GODOT_AI_VERSION:
            raise B2HarnessError(
                f"Godot Agent package version mismatch: expected {GODOT_AI_VERSION!r}, got {observed!r}"
            )
        evidence["godot_agent"] = {
            "python": str(python),
            "addon": str(addon),
            "version": observed,
            "frozen_source_commit": GODOT_AI_COMMIT,
            "frozen_package_identity": GODOT_AI_PACKAGE,
        }

    if lane == "trace2d.agent":
        trace2d = require_file_from_env(("TRACE2D_BENCH_TRACE2D_BIN",), "Trace2D public CLI")
        evidence["trace2d"] = {"path": str(trace2d)}
    return evidence


def verify_starter(workspace: Path, lane: str, execution: dict[str, Any]) -> None:
    files = execution["lane_starters"][lane]["files"]
    observed = sorted(path.relative_to(workspace).as_posix() for path in workspace.rglob("*") if path.is_file())
    if observed != sorted(files):
        raise B2HarnessError(f"copied starter file set drifted for {lane}")
    for relative, expected in files.items():
        if sha256_file(workspace / relative) != expected:
            raise B2HarnessError(f"copied starter SHA-256 mismatch for {lane}/{relative}")


def agent_command(workspace: Path, prompt: Path, lane: str, result: Path) -> list[str]:
    return [
        sys.executable,
        str(ADAPTER_PATH),
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


def verifier_command(workspace: Path, lane: str, output: Path, slot: int) -> list[str]:
    build_root_raw = os.environ.get("TRACE2D_B2_VERIFY_BUILD_ROOT", "").strip()
    build_root = Path(build_root_raw).expanduser().resolve() if build_root_raw else REPO_ROOT / ".trace2d-benchmark-b2/candidate-verifier"
    command = [
        sys.executable,
        str(VERIFIER_PATH),
        "--task",
        TASK_ID,
        "--lane",
        lane,
        "--workspace",
        str(workspace),
        "--repo-root",
        str(REPO_ROOT),
        "--trace2d-build-dir",
        str(build_root / f"slot-{slot:02d}"),
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
        if path.suffix.casefold() in PRESENTATION_SUFFIXES:
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


def _valid_checkpoint(
    path: Path, *, kind: str, trial_id: str, slot: int, lane: str
) -> dict[str, Any] | None:
    if not path.is_file() or path.is_symlink():
        return None
    try:
        data = load_json(path)
    except B2HarnessError:
        return None
    if (
        data.get("kind") != kind
        or data.get("trial_id") != trial_id
        or data.get("slot") != slot
        or data.get("lane_id") != lane
    ):
        return None
    return data


def _retained_artifact_fingerprints(trial_root: Path) -> list[dict[str, Any]]:
    names = (
        ATTEMPT_START_CHECKPOINT,
        AGENT_PROCESS_CHECKPOINT,
        "agent-result.json",
        "verifier-result.json",
        "adapter.stdout.txt",
        "adapter.stderr.txt",
        "verifier.stdout.txt",
        "verifier.stderr.txt",
        "codex-events.jsonl",
        "codex.stderr.txt",
        "effective-agent-prompt.md",
        "godot-ai-server.log",
        "godot-editor.log",
    )
    result: list[dict[str, Any]] = []
    for name in names:
        path = trial_root / name
        if not path.is_file() or path.is_symlink():
            continue
        try:
            result.append({"path": name, "sha256": sha256_file(path), "bytes": path.stat().st_size})
        except OSError as exc:
            result.append({"path": name, "error": f"{type(exc).__name__}: {exc}"})
    return result


def _safe_workspace_hash(workspace: Path) -> tuple[str | None, str | None]:
    if not workspace.is_dir():
        return None, "workspace directory missing"
    try:
        return benchmark_b0_stable_harness.stable_tree_hash(workspace), None
    except (OSError, RuntimeError) as exc:
        return None, f"{type(exc).__name__}: {exc}"


def seal_interrupted_trial(
    *,
    runs_root: Path,
    records: list[dict[str, Any]],
    expected: dict[str, Any],
    profile: dict[str, Any],
    trial_root: Path,
) -> dict[str, Any]:
    slot = int(expected["slot"])
    lane = str(expected["lane"])
    trial_id = f"slot-{slot:02d}-{lane.replace('.', '-')}-r{expected['repetition']}"
    start_checkpoint = _valid_checkpoint(
        trial_root / ATTEMPT_START_CHECKPOINT,
        kind="trace2d_b2_attempt_start",
        trial_id=trial_id,
        slot=slot,
        lane=lane,
    )
    process_checkpoint = _valid_checkpoint(
        trial_root / AGENT_PROCESS_CHECKPOINT,
        kind="trace2d_b2_agent_process",
        trial_id=trial_id,
        slot=slot,
        lane=lane,
    )
    legacy_process_evidence = all(
        (trial_root / name).is_file() for name in ("adapter.stdout.txt", "adapter.stderr.txt")
    )
    if start_checkpoint is None and not legacy_process_evidence:
        raise B2HarnessError(
            "slot trial directory exists without proof that the Agent attempt started; "
            f"refusing both rerun and automatic recovery: {trial_root}"
        )

    workspace = trial_root / "workspace"
    workspace_hash, workspace_hash_error = _safe_workspace_hash(workspace)
    wall_ms: float | None = None
    if process_checkpoint is not None and isinstance(process_checkpoint.get("duration_ms"), (int, float)):
        wall_ms = float(process_checkpoint["duration_ms"])
    started_at = start_checkpoint.get("started_at") if start_checkpoint is not None else None
    agent_finished_at = process_checkpoint.get("finished_at") if process_checkpoint is not None else None
    recovered_at = utc_now()
    retained = _retained_artifact_fingerprints(trial_root)

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
        "finished_at": recovered_at,
        "execution_contract_sha256": sha256_file(REPO_ROOT / EXECUTION_PATH),
        "preregistration_sha256": sha256_file(REPO_ROOT / PREREG_PATH),
        "task_prompt_sha256": sha256_file(REPO_ROOT / PROMPT_PATH),
        "agent_profile_sha256": benchmark_b0.sha256_json(profile),
        "status": "benchmark_integrity_failure",
        "failure_domain": FAILURE_DOMAINS["benchmark_integrity_failure"],
        "agent_identity_ok": None,
        "agent_result": None,
        "metrics": {
            "available": False,
            "revisions": None,
            "tool_calls": None,
            "input_tokens": None,
            "output_tokens": None,
            "visual_feedback_calls": None,
            "human_interventions": None,
            "wall_ms": wall_ms,
            "verifier_ms": None,
            "normalized_operations": None,
            "engine_native_operations": None,
        },
        "deterministic_verifier": None,
        "presentation": {
            "required": True,
            "authoritative_for_gameplay": False,
            "capture_root": PRESENTATION_ROOT.as_posix(),
            "captures": [],
            "retained_candidate_outputs_not_reinterpreted": True,
        },
        "workspace_sha256": workspace_hash,
        "workspace_hash_policy": benchmark_b0_stable_harness.WORKSPACE_HASH_POLICY,
        "environment": {
            "os": platform.system(),
            "os_release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "recovery_only": True,
        },
        "integrity": {
            "schedule_prefix_length_before": len(records),
            "schedule_prefix_valid": True,
            "repo_freeze_unchanged_after_agent": None,
            "automatic_retries": 0,
            "replacement_trials": 0,
            "interrupted_attempt_recovered": True,
            "agent_reexecuted": False,
            "verifier_reexecuted": False,
        },
        "recovery": {
            "reason": "existing_consumed_trial_missing_raw_record",
            "recovered_at": recovered_at,
            "agent_finished_at": agent_finished_at,
            "start_checkpoint_valid": start_checkpoint is not None,
            "process_checkpoint_valid": process_checkpoint is not None,
            "legacy_process_evidence": legacy_process_evidence,
            "workspace_hash_error": workspace_hash_error,
            "retained_artifacts": retained,
            "candidate_result_reused": False,
            "candidate_verdict_reused": False,
        },
        "artifacts": {
            "trial_root": str(trial_root),
            "workspace": str(workspace),
            "attempt_start": str(trial_root / ATTEMPT_START_CHECKPOINT),
            "agent_process": str(trial_root / AGENT_PROCESS_CHECKPOINT),
            "agent_result": str(trial_root / "agent-result.json"),
            "verifier_result": str(trial_root / "verifier-result.json"),
            "adapter_stdout": str(trial_root / "adapter.stdout.txt"),
            "adapter_stderr": str(trial_root / "adapter.stderr.txt"),
            "verifier_stdout": str(trial_root / "verifier.stdout.txt"),
            "verifier_stderr": str(trial_root / "verifier.stderr.txt"),
        },
    }
    return benchmark_b0.append_hash_chained_jsonl(runs_root / "raw.jsonl", record)


def run_slot(args: argparse.Namespace) -> dict[str, Any]:
    execution, prereg, profile = load_contract()
    runs_root = Path(args.runs_root).expanduser().resolve()
    runs_root.mkdir(parents=True, exist_ok=True)
    records, expected = next_frozen_slot(runs_root, execution)
    if expected is None:
        raise B2HarnessError("all nine frozen initial B2 slots are already recorded")
    if args.slot != expected["slot"]:
        raise B2HarnessError(
            f"next frozen B2 slot is {expected['slot']} ({expected['lane']}), not requested slot {args.slot}"
        )

    slot = int(expected["slot"])
    lane = str(expected["lane"])
    trial_id = f"slot-{slot:02d}-{lane.replace('.', '-')}-r{expected['repetition']}"
    trial_root = runs_root / "trials" / trial_id
    if trial_root.exists():
        return seal_interrupted_trial(
            runs_root=runs_root,
            records=records,
            expected=expected,
            profile=profile,
            trial_root=trial_root,
        )

    environment_preflight = preflight_environment(lane)
    trial_root.mkdir(parents=True)

    workspace = trial_root / "workspace"
    starter = REPO_ROOT / execution["lane_starters"][lane]["root"]
    shutil.copytree(starter, workspace)
    verify_starter(workspace, lane, execution)
    prompt = trial_root / "frozen-task-prompt.md"
    shutil.copy2(REPO_ROOT / PROMPT_PATH, prompt)
    if sha256_file(prompt) != sha256_file(REPO_ROOT / PROMPT_PATH):
        raise B2HarnessError("frozen B2 task prompt copy drifted")

    agent_result_path = trial_root / "agent-result.json"
    verifier_result_path = trial_root / "verifier-result.json"
    budget = prereg["budget"]
    started_at = utc_now()
    write_json_atomic(
        trial_root / ATTEMPT_START_CHECKPOINT,
        {
            "schema_version": 1,
            "kind": "trace2d_b2_attempt_start",
            "benchmark_id": BENCHMARK_ID,
            "trial_id": trial_id,
            "slot": slot,
            "repetition": int(expected["repetition"]),
            "task_id": TASK_ID,
            "lane_id": lane,
            "started_at": started_at,
            "execution_contract_sha256": sha256_file(REPO_ROOT / EXECUTION_PATH),
            "preregistration_sha256": sha256_file(REPO_ROOT / PREREG_PATH),
            "task_prompt_sha256": sha256_file(REPO_ROOT / PROMPT_PATH),
            "agent_profile_sha256": benchmark_b0.sha256_json(profile),
        },
    )
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
    agent_finished_at = utc_now()
    write_json_atomic(
        trial_root / AGENT_PROCESS_CHECKPOINT,
        {
            "schema_version": 1,
            "kind": "trace2d_b2_agent_process",
            "benchmark_id": BENCHMARK_ID,
            "trial_id": trial_id,
            "slot": slot,
            "repetition": int(expected["repetition"]),
            "task_id": TASK_ID,
            "lane_id": lane,
            "started_at": started_at,
            "finished_at": agent_finished_at,
            "return_code": process["return_code"],
            "timed_out": process["timed_out"],
            "duration_ms": float(process["duration_ms"]),
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
        benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    except Exception:
        integrity_ok = False

    verifier: dict[str, Any] | None = None
    verifier_process: dict[str, Any] | None = None
    if integrity_ok and not process["timed_out"]:
        verifier_process = run_process(
            verifier_command(workspace, lane, verifier_result_path, slot),
            cwd=REPO_ROOT,
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
    status, failure_domain = classify(process, agent_result, identity_ok, verifier, captures, integrity_ok)
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
        "execution_contract_sha256": sha256_file(REPO_ROOT / EXECUTION_PATH),
        "preregistration_sha256": sha256_file(REPO_ROOT / PREREG_PATH),
        "task_prompt_sha256": sha256_file(REPO_ROOT / PROMPT_PATH),
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
        "workspace_hash_policy": benchmark_b0_stable_harness.WORKSPACE_HASH_POLICY,
        "environment": {
            "preflight": environment_preflight,
            "os": platform.system(),
            "os_release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "integrity": {
            "schedule_prefix_length_before": len(records),
            "schedule_prefix_valid": True,
            "repo_freeze_unchanged_after_agent": integrity_ok,
            "automatic_retries": 0,
            "replacement_trials": 0,
        },
        "artifacts": {
            "trial_root": str(trial_root),
            "workspace": str(workspace),
            "attempt_start": str(trial_root / ATTEMPT_START_CHECKPOINT),
            "agent_process": str(trial_root / AGENT_PROCESS_CHECKPOINT),
            "agent_result": str(agent_result_path),
            "verifier_result": str(verifier_result_path),
            "adapter_stdout": str(trial_root / "adapter.stdout.txt"),
            "adapter_stderr": str(trial_root / "adapter.stderr.txt"),
            "verifier_stdout": str(trial_root / "verifier.stdout.txt"),
            "verifier_stderr": str(trial_root / "verifier.stderr.txt"),
        },
    }
    return benchmark_b0.append_hash_chained_jsonl(runs_root / "raw.jsonl", record)


def validate(_: argparse.Namespace) -> dict[str, Any]:
    execution, prereg, profile = load_contract()
    return {
        "benchmark_id": BENCHMARK_ID,
        "scoring_gate_open": prereg["scoring_gate"]["allowed"],
        "slot_count": len(execution["slots"]),
        "agent_profile_sha256": benchmark_b0.sha256_json(profile),
        "execution_contract_sha256": sha256_file(REPO_ROOT / EXECUTION_PATH),
        "valid": True,
    }


def next_slot(args: argparse.Namespace) -> dict[str, Any]:
    execution, _, _ = load_contract()
    runs_root = Path(args.runs_root).expanduser().resolve()
    records, slot = next_frozen_slot(runs_root, execution)
    return {
        "benchmark_id": BENCHMARK_ID,
        "initial_scoring_complete": slot is None,
        "completed_slots": len(records),
        "next_slot": slot,
    }


def preflight_slot(args: argparse.Namespace) -> dict[str, Any]:
    execution, _, _ = load_contract()
    runs_root = Path(args.runs_root).expanduser().resolve()
    records, slot = next_frozen_slot(runs_root, execution)
    if slot is None:
        return {
            "benchmark_id": BENCHMARK_ID,
            "initial_scoring_complete": True,
            "completed_slots": len(records),
            "next_slot": None,
        }
    return {
        "benchmark_id": BENCHMARK_ID,
        "initial_scoring_complete": False,
        "completed_slots": len(records),
        "next_slot": slot,
        "environment": preflight_environment(str(slot["lane"])),
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D Benchmark B2 owner-local scored harness")
    commands = parser.add_subparsers(dest="command", required=True)
    check = commands.add_parser("validate")
    check.set_defaults(handler=validate)
    upcoming = commands.add_parser("next-slot")
    upcoming.add_argument("--runs-root", required=True)
    upcoming.set_defaults(handler=next_slot)
    preflight = commands.add_parser("preflight-slot")
    preflight.add_argument("--runs-root", required=True)
    preflight.set_defaults(handler=preflight_slot)
    run = commands.add_parser("run-slot")
    run.add_argument("--runs-root", required=True)
    run.add_argument("--slot", type=int, required=True, choices=range(1, 10))
    run.set_defaults(handler=run_slot)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        result = args.handler(args)
    except (B2HarnessError, codex_core.WrapperError, OSError, subprocess.SubprocessError, ValueError) as exc:
        print(f"B2 scored harness error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
