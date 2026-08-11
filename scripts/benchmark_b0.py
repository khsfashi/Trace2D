#!/usr/bin/env python3
"""Trace2D Benchmark B0 matched harness.

Stdlib-only orchestration for isolated trials, independent verification,
append-only raw records, and replay/re-verification. It deliberately does not
invoke a model provider directly; a frozen external Agent wrapper is supplied
through an agent profile so the same Agent/model can be reused across lanes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

SCHEMA_VERSION = 1
EXPECTED_LANES = ("godot.generic", "godot.agent", "trace2d.agent")
FAILURE_DOMAINS = {
    "success": "success",
    "environment_failure": "infrastructure",
    "harness_setup_failure": "infrastructure",
    "agent_setup_failure": "infrastructure",
    "tool_transport_failure": "infrastructure",
    "timeout": "implementation",
    "engine_build_test_failure": "implementation",
    "verifier_failure": "infrastructure",
    "capability_not_eligible": "eligibility",
    "human_intervention": "human",
    "benchmark_integrity_failure": "integrity",
}


class HarnessError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise HarnessError(f"failed to read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise HarnessError(f"expected JSON object: {path}")
    return value


def canonical_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def sha256_json(value: Any) -> str:
    return sha256_bytes(canonical_json(value).encode("utf-8"))


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def resolve_repo_path(value: str) -> Path:
    return (repository_root() / value).resolve()


def find_lane(suite: dict[str, Any], lane_id: str) -> dict[str, Any]:
    for lane in suite.get("lanes", []):
        if lane.get("id") == lane_id:
            return lane
    raise HarnessError(f"unknown lane: {lane_id}")


def find_task(suite: dict[str, Any], task_id: str) -> dict[str, Any]:
    for task in suite.get("tasks", []):
        if task.get("id") == task_id:
            return task
    raise HarnessError(f"unknown task: {task_id}")


def validate_suite(suite_path: Path) -> dict[str, Any]:
    suite = load_json(suite_path)
    errors: list[str] = []
    if suite.get("schema_version") != SCHEMA_VERSION:
        errors.append("suite.schema_version must be 1")
    if not isinstance(suite.get("suite_id"), str) or not suite["suite_id"]:
        errors.append("suite_id must be a non-empty string")

    lane_ids = tuple(lane.get("id") for lane in suite.get("lanes", []))
    if set(lane_ids) != set(EXPECTED_LANES) or len(lane_ids) != len(EXPECTED_LANES):
        errors.append(f"lanes must be exactly {EXPECTED_LANES}")

    for lane in suite.get("lanes", []):
        qualification = lane.get("qualification")
        if not isinstance(qualification, dict) or qualification.get("required") is not True:
            errors.append(f"lane {lane.get('id')} must require qualification evidence")
        bridge = lane.get("bridge")
        if isinstance(bridge, dict) and bridge.get("package"):
            version = str(bridge.get("version", ""))
            if not version or version.lower() == "latest":
                errors.append(f"lane {lane.get('id')} bridge package must pin an exact version")

    tasks = suite.get("tasks")
    if not isinstance(tasks, list) or not tasks:
        errors.append("suite must contain at least one task")
    else:
        for task in tasks:
            task_id = task.get("id", "<missing>")
            if not isinstance(task.get("prompt"), str):
                errors.append(f"task {task_id} requires prompt path")
            else:
                prompt = resolve_repo_path(task["prompt"])
                if not prompt.is_file():
                    errors.append(f"task {task_id} prompt does not exist: {prompt}")
            lane_map = task.get("lanes")
            if not isinstance(lane_map, dict) or set(lane_map.keys()) != set(EXPECTED_LANES):
                errors.append(f"task {task_id} must define all three lanes")
                continue
            for lane_id, lane_task in lane_map.items():
                for key in ("starter", "known_good"):
                    path = resolve_repo_path(str(lane_task.get(key, "")))
                    if not path.is_dir():
                        errors.append(f"task {task_id}/{lane_id} missing {key}: {path}")
                known_bad = lane_task.get("known_bad")
                if not isinstance(known_bad, list) or not known_bad:
                    errors.append(f"task {task_id}/{lane_id} requires at least one known_bad fixture")
                else:
                    for item in known_bad:
                        path = resolve_repo_path(str(item))
                        if not path.is_dir():
                            errors.append(f"task {task_id}/{lane_id} missing known_bad fixture: {path}")
                if not lane_task.get("verifier"):
                    errors.append(f"task {task_id}/{lane_id} requires verifier identity")

    if errors:
        raise HarnessError("suite validation failed:\n  - " + "\n  - ".join(errors))
    return suite


def qualification_path(lane: dict[str, Any]) -> Path:
    return resolve_repo_path(str(lane["qualification"]["evidence"]))


def validate_qualification(suite: dict[str, Any], lane_id: str) -> dict[str, Any]:
    lane = find_lane(suite, lane_id)
    path = qualification_path(lane)
    if not path.is_file():
        raise HarnessError(f"qualification evidence missing for {lane_id}: {path}")
    evidence = load_json(path)
    if evidence.get("schema_version") != SCHEMA_VERSION:
        raise HarnessError(f"qualification evidence schema mismatch: {path}")
    if evidence.get("lane_id") != lane_id or evidence.get("qualified") is not True:
        raise HarnessError(f"qualification evidence is not a positive result for {lane_id}: {path}")
    if lane_id == "godot.agent":
        bridge = lane.get("bridge", {})
        recorded = evidence.get("bridge", {})
        if recorded.get("id") != bridge.get("id") or recorded.get("version") != bridge.get("version"):
            raise HarnessError("godot.agent qualification does not match the pinned bridge identity/version")
        checks = evidence.get("checks", {})
        for required in ("authoring", "runtime_inspection", "timed_input", "deterministic_step"):
            if checks.get(required) is not True:
                raise HarnessError(f"godot.agent qualification missing required check: {required}")
    return evidence


def executable_from_environment(lane_id: str) -> tuple[str, Path]:
    if lane_id.startswith("godot."):
        key = "TRACE2D_BENCH_GODOT_BIN"
    else:
        key = "TRACE2D_BENCH_TRACE2D_BIN"
    raw = os.environ.get(key, "")
    if not raw:
        raise HarnessError(f"{key} is required for lane {lane_id}")
    path = Path(raw).expanduser().resolve()
    if not path.is_file():
        raise HarnessError(f"{key} does not point to a file: {path}")
    return key, path


def subprocess_capture(
    argv: list[str],
    *,
    cwd: Path,
    timeout_seconds: float,
    extra_env: dict[str, str] | None = None,
) -> dict[str, Any]:
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)
    started = time.perf_counter_ns()
    try:
        completed = subprocess.run(
            argv,
            cwd=cwd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_seconds,
            check=False,
        )
        timed_out = False
        return_code: int | None = completed.returncode
        stdout = completed.stdout
        stderr = completed.stderr
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        return_code = None
        stdout = exc.stdout if isinstance(exc.stdout, str) else ""
        stderr = exc.stderr if isinstance(exc.stderr, str) else ""
    elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000.0
    return {
        "argv": argv,
        "return_code": return_code,
        "timed_out": timed_out,
        "stdout": stdout,
        "stderr": stderr,
        "duration_ms": elapsed_ms,
    }


def last_json_object(text: str) -> dict[str, Any] | None:
    for line in reversed(text.splitlines()):
        stripped = line.strip()
        if not stripped.startswith("{"):
            continue
        try:
            value = json.loads(stripped)
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict):
            return value
    return None


def verify_trace2d(workspace: Path, expected: dict[str, Any], timeout_seconds: float) -> dict[str, Any]:
    _, engine = executable_from_environment("trace2d.agent")
    scene = workspace / "scene.trace2d.toml"
    result = subprocess_capture(
        [
            str(engine),
            "inspect",
            "--scene",
            str(scene),
            "--frames",
            "0",
            "--seed",
            "42",
            "--json",
        ],
        cwd=workspace,
        timeout_seconds=timeout_seconds,
    )
    if result["timed_out"]:
        return {"status": "error", "code": "verifier_timeout", "process": result}
    payload = last_json_object(result["stdout"])
    if result["return_code"] != 0 or payload is None or payload.get("status") != "ok":
        return {"status": "fail", "code": "scene_load_or_inspect_failed", "process": result, "payload": payload}

    entities = payload.get("scene", {}).get("entities", [])
    matches = [entity for entity in entities if entity.get("id") == expected["entity_id"]]
    if len(matches) != 1:
        return {"status": "fail", "code": "semantic_identity_mismatch", "observed_count": len(matches), "process": result}
    entity = matches[0]
    position = entity.get("transform", {}).get("position", {})
    checks = {
        "name": entity.get("name") == expected["name"],
        "position_x": position.get("x") == expected["position_x"],
        "position_y": position.get("y") == expected["position_y"],
    }
    if not all(checks.values()):
        return {"status": "fail", "code": "observable_mismatch", "checks": checks, "entity": entity, "process": result}
    return {
        "status": "pass",
        "verifier": "trace2d-semantic-scene-v1",
        "observed": {
            "entity_id": entity.get("id"),
            "name": entity.get("name"),
            "position_x": position.get("x"),
            "position_y": position.get("y"),
        },
        "process": result,
    }


def verify_godot(workspace: Path, timeout_seconds: float) -> dict[str, Any]:
    _, engine = executable_from_environment("godot.generic")
    verifier = resolve_repo_path("benchmarks/b0/verifiers/godot_semantic_scene.gd")
    result = subprocess_capture(
        [str(engine), "--headless", "--path", str(workspace), "--script", str(verifier)],
        cwd=workspace,
        timeout_seconds=timeout_seconds,
    )
    if result["timed_out"]:
        return {"status": "error", "code": "verifier_timeout", "process": result}
    payload = last_json_object(result["stdout"])
    if payload is None:
        return {"status": "error", "code": "verifier_protocol_error", "process": result}
    if payload.get("status") == "pass" and result["return_code"] == 0:
        return {"status": "pass", "verifier": "godot-semantic-scene-v1", "observed": payload, "process": result}
    if payload.get("status") == "fail":
        return {"status": "fail", "code": payload.get("code", "rejected"), "observed": payload, "process": result}
    return {"status": "error", "code": "verifier_process_error", "observed": payload, "process": result}


def run_verifier(suite: dict[str, Any], task: dict[str, Any], lane_id: str, workspace: Path) -> dict[str, Any]:
    timeout = min(float(task["budget"]["wall_seconds"]), 60.0)
    if lane_id.startswith("godot."):
        return verify_godot(workspace, timeout)
    return verify_trace2d(workspace, task["expected"], timeout)


def fixture_qualification(suite: dict[str, Any], task_id: str, lane_id: str) -> dict[str, Any]:
    task = find_task(suite, task_id)
    lane_task = task["lanes"][lane_id]
    good_path = resolve_repo_path(lane_task["known_good"])
    good = run_verifier(suite, task, lane_id, good_path)
    bad_results: list[dict[str, Any]] = []
    for value in lane_task["known_bad"]:
        path = resolve_repo_path(value)
        verdict = run_verifier(suite, task, lane_id, path)
        bad_results.append({"fixture": value, "verdict": verdict})
    qualified = good.get("status") == "pass" and all(item["verdict"].get("status") == "fail" for item in bad_results)
    return {
        "schema_version": SCHEMA_VERSION,
        "suite_id": suite["suite_id"],
        "task_id": task_id,
        "lane_id": lane_id,
        "qualified": qualified,
        "known_good": good,
        "known_bad": bad_results,
        "generated_at": utc_now(),
    }


def validate_agent_profile(path: Path, task: dict[str, Any]) -> dict[str, Any]:
    profile = load_json(path)
    errors: list[str] = []
    if profile.get("schema_version") != SCHEMA_VERSION:
        errors.append("agent profile schema_version must be 1")
    for key in ("agent_id", "model_id", "model_revision"):
        if not isinstance(profile.get(key), str) or not profile[key]:
            errors.append(f"agent profile requires non-empty {key}")
    command = profile.get("command")
    if not isinstance(command, list) or not command or not all(isinstance(item, str) and item for item in command):
        errors.append("agent profile command must be a non-empty argv string array")
    budget = profile.get("budget")
    if not isinstance(budget, dict) or budget != task.get("budget"):
        errors.append("agent profile budget must exactly equal the task budget")
    if errors:
        raise HarnessError("agent profile validation failed:\n  - " + "\n  - ".join(errors))
    return profile


def tree_hash(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted((p for p in root.rglob("*") if p.is_file()), key=lambda p: p.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(sha256_file(path).encode("ascii"))
        digest.update(b"\0")
    return digest.hexdigest()


def protected_files(suite_path: Path, task: dict[str, Any]) -> dict[str, str]:
    paths = [
        suite_path.resolve(),
        resolve_repo_path(task["prompt"]),
        resolve_repo_path("benchmarks/b0/verifiers/godot_semantic_scene.gd"),
        Path(__file__).resolve(),
    ]
    return {str(path): sha256_file(path) for path in paths}


def verify_protected_files(before: dict[str, str]) -> bool:
    for raw, expected in before.items():
        path = Path(raw)
        if not path.is_file() or sha256_file(path) != expected:
            return False
    return True


def append_hash_chained_jsonl(path: Path, record: dict[str, Any]) -> dict[str, Any]:
    path.parent.mkdir(parents=True, exist_ok=True)
    previous_hash: str | None = None
    if path.exists():
        last_line = ""
        with path.open("r", encoding="utf-8") as stream:
            for line in stream:
                if line.strip():
                    last_line = line
        if last_line:
            try:
                previous = json.loads(last_line)
                previous_hash = previous.get("record_sha256")
            except json.JSONDecodeError as exc:
                raise HarnessError(f"raw JSONL tail is malformed; refusing to append: {path}") from exc
    payload = dict(record)
    payload["previous_record_sha256"] = previous_hash
    payload["record_sha256"] = sha256_json(payload)
    encoded = canonical_json(payload) + "\n"
    with path.open("a", encoding="utf-8", newline="\n") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    return payload


def verify_jsonl_chain(path: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    previous_hash: str | None = None
    with path.open("r", encoding="utf-8") as stream:
        for number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                raise HarnessError(f"malformed JSONL at {path}:{number}") from exc
            if record.get("previous_record_sha256") != previous_hash:
                raise HarnessError(f"JSONL hash chain break at {path}:{number}")
            claimed = record.get("record_sha256")
            without_hash = dict(record)
            without_hash.pop("record_sha256", None)
            actual = sha256_json(without_hash)
            if claimed != actual:
                raise HarnessError(f"JSONL record hash mismatch at {path}:{number}")
            previous_hash = claimed
            records.append(record)
    return records


def substitute_command(command: Iterable[str], values: dict[str, str]) -> list[str]:
    result: list[str] = []
    for item in command:
        rendered = item
        for key, value in values.items():
            rendered = rendered.replace("{" + key + "}", value)
        if "{" in rendered or "}" in rendered:
            raise HarnessError(f"unresolved command placeholder: {rendered}")
        result.append(rendered)
    return result


def classify_agent_result(
    *,
    process: dict[str, Any],
    agent_result: dict[str, Any] | None,
    verifier: dict[str, Any] | None,
    integrity_ok: bool,
) -> tuple[str, str]:
    if not integrity_ok:
        status = "benchmark_integrity_failure"
    elif process["timed_out"]:
        status = "timeout"
    elif agent_result is None:
        status = "agent_setup_failure"
    elif int(agent_result.get("human_interventions", 0)) > 0:
        status = "human_intervention"
    elif agent_result.get("status") == "tool_transport_failure":
        status = "tool_transport_failure"
    elif process["return_code"] != 0 or agent_result.get("status") != "completed":
        status = "agent_setup_failure"
    elif verifier is None or verifier.get("status") == "error":
        status = "verifier_failure"
    elif verifier.get("status") != "pass":
        status = "engine_build_test_failure"
    else:
        status = "success"
    return status, FAILURE_DOMAINS[status]


def environment_snapshot(lane_id: str) -> dict[str, Any]:
    engine_key = "TRACE2D_BENCH_GODOT_BIN" if lane_id.startswith("godot.") else "TRACE2D_BENCH_TRACE2D_BIN"
    return {
        "os": platform.system(),
        "os_release": platform.release(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "engine_path": os.environ.get(engine_key, ""),
        "engine_version": os.environ.get("TRACE2D_BENCH_ENGINE_VERSION", ""),
        "node_version": os.environ.get("TRACE2D_BENCH_NODE_VERSION", ""),
    }


def run_trial(args: argparse.Namespace) -> dict[str, Any]:
    suite_path = Path(args.suite).resolve()
    suite = validate_suite(suite_path)
    task = find_task(suite, args.task)
    lane = find_lane(suite, args.lane)
    lane_task = task["lanes"][args.lane]
    scored = bool(args.scored)

    if scored:
        if suite.get("state") != "eligible":
            raise HarnessError("scored run blocked: suite state is not eligible")
        if task.get("state") != "eligible":
            raise HarnessError(f"scored run blocked: task {args.task} state is not eligible")
        validate_qualification(suite, args.lane)

    executable_from_environment(args.lane)
    profile_path = Path(args.agent_profile).resolve()
    profile = validate_agent_profile(profile_path, task)
    profile_hash = sha256_json(profile)
    suite_hash = sha256_file(suite_path)

    runs_root = Path(args.runs_root).resolve()
    runs_root.mkdir(parents=True, exist_ok=True)
    trial_id = args.trial_id or f"{args.task}-{args.lane}-{uuid.uuid4().hex[:12]}"
    trial_root = runs_root / "trials" / trial_id
    if trial_root.exists():
        raise HarnessError(f"trial directory already exists: {trial_root}")
    workspace = trial_root / "workspace"
    starter = resolve_repo_path(lane_task["starter"])
    shutil.copytree(starter, workspace)
    prompt_src = resolve_repo_path(task["prompt"])
    prompt_dst = trial_root / "prompt.md"
    trial_root.mkdir(parents=True, exist_ok=True)
    shutil.copy2(prompt_src, prompt_dst)

    agent_result_path = trial_root / "agent-result.json"
    command = substitute_command(
        profile["command"],
        {
            "workspace": str(workspace),
            "prompt": str(prompt_dst),
            "agent_result": str(agent_result_path),
            "lane": args.lane,
            "task": args.task,
        },
    )
    protected = protected_files(suite_path, task)
    started_at = utc_now()
    process = subprocess_capture(
        command,
        cwd=workspace,
        timeout_seconds=float(task["budget"]["wall_seconds"]),
        extra_env={
            "TRACE2D_BENCH_LANE": args.lane,
            "TRACE2D_BENCH_TASK": args.task,
            "TRACE2D_BENCH_WORKSPACE": str(workspace),
            "TRACE2D_BENCH_PROMPT_FILE": str(prompt_dst),
            "TRACE2D_BENCH_AGENT_RESULT": str(agent_result_path),
            "TRACE2D_BENCH_MAX_TOOL_CALLS": str(task["budget"]["max_tool_calls"]),
            "TRACE2D_BENCH_MAX_INPUT_TOKENS": str(task["budget"]["max_input_tokens"]),
            "TRACE2D_BENCH_MAX_OUTPUT_TOKENS": str(task["budget"]["max_output_tokens"]),
        },
    )
    (trial_root / "agent.stdout.txt").write_text(process["stdout"], encoding="utf-8")
    (trial_root / "agent.stderr.txt").write_text(process["stderr"], encoding="utf-8")

    agent_result: dict[str, Any] | None = None
    if agent_result_path.is_file():
        try:
            agent_result = load_json(agent_result_path)
        except HarnessError:
            agent_result = None
    identity_ok = True
    if agent_result is not None:
        identity = agent_result.get("model", {})
        identity_ok = (
            identity.get("agent_id") == profile["agent_id"]
            and identity.get("model_id") == profile["model_id"]
            and identity.get("model_revision") == profile["model_revision"]
        )
        if not identity_ok:
            agent_result = None

    integrity_ok = verify_protected_files(protected)
    verifier: dict[str, Any] | None = None
    if integrity_ok and not process["timed_out"]:
        verifier = run_verifier(suite, task, args.lane, workspace)

    status, failure_domain = classify_agent_result(
        process=process,
        agent_result=agent_result,
        verifier=verifier,
        integrity_ok=integrity_ok,
    )
    metrics = agent_result.get("metrics", {}) if agent_result else {}
    record = {
        "schema_version": SCHEMA_VERSION,
        "suite_id": suite["suite_id"],
        "suite_sha256": suite_hash,
        "task_id": args.task,
        "task_version": task["version"],
        "lane_id": args.lane,
        "trial_id": trial_id,
        "scored": scored,
        "started_at": started_at,
        "finished_at": utc_now(),
        "agent_profile_sha256": profile_hash,
        "agent": {
            "agent_id": profile["agent_id"],
            "model_id": profile["model_id"],
            "model_revision": profile["model_revision"],
            "settings": profile.get("settings", {}),
            "budget": profile["budget"],
        },
        "lane": {
            "engine": lane["engine"],
            "adapter": lane["adapter"],
            "bridge": lane.get("bridge"),
        },
        "environment": environment_snapshot(args.lane),
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
            "wall_ms": process["duration_ms"],
            "verifier_ms": float((verifier or {}).get("process", {}).get("duration_ms", 0.0)),
            "human_interventions": int((agent_result or {}).get("human_interventions", 0)),
            "normalized_operations": metrics.get("normalized_operations", {}),
            "engine_native_operations": metrics.get("engine_native_operations", {}),
        },
        "verifier": verifier,
        "workspace_sha256": tree_hash(workspace),
        "artifacts": {
            "trial_root": str(trial_root),
            "workspace": str(workspace),
            "agent_stdout": str(trial_root / "agent.stdout.txt"),
            "agent_stderr": str(trial_root / "agent.stderr.txt"),
        },
    }
    raw_path = runs_root / "raw.jsonl"
    return append_hash_chained_jsonl(raw_path, record)


def reverify_trial(args: argparse.Namespace) -> dict[str, Any]:
    suite = validate_suite(Path(args.suite).resolve())
    raw_path = Path(args.records).resolve()
    records = verify_jsonl_chain(raw_path)
    original = next((record for record in records if record.get("trial_id") == args.trial_id), None)
    if original is None:
        raise HarnessError(f"trial not found: {args.trial_id}")
    task = find_task(suite, original["task_id"])
    workspace = Path(original["artifacts"]["workspace"])
    if not workspace.is_dir():
        raise HarnessError(f"trial workspace missing: {workspace}")
    executable_from_environment(original["lane_id"])
    artifact_hash = tree_hash(workspace)
    verifier = run_verifier(suite, task, original["lane_id"], workspace)
    replay = {
        "schema_version": SCHEMA_VERSION,
        "kind": "independent_reverify",
        "suite_id": suite["suite_id"],
        "trial_id": args.trial_id,
        "original_record_sha256": original["record_sha256"],
        "lane_id": original["lane_id"],
        "task_id": original["task_id"],
        "workspace_sha256": artifact_hash,
        "workspace_matches_original": artifact_hash == original["workspace_sha256"],
        "verifier": verifier,
        "verdict_matches_original": (
            verifier.get("status") == (original.get("verifier") or {}).get("status")
        ),
        "generated_at": utc_now(),
    }
    return append_hash_chained_jsonl(Path(args.replay_records).resolve(), replay)


def report_records(path: Path, include_unscored: bool) -> dict[str, Any]:
    records = verify_jsonl_chain(path)
    if not include_unscored:
        records = [record for record in records if record.get("scored") is True]
    groups: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for record in records:
        groups.setdefault((record["task_id"], record["lane_id"]), []).append(record)

    profile_hashes_by_task: dict[str, set[str]] = {}
    for record in records:
        profile_hashes_by_task.setdefault(record["task_id"], set()).add(record["agent_profile_sha256"])

    output_groups: list[dict[str, Any]] = []
    for (task_id, lane_id), values in sorted(groups.items()):
        statuses: dict[str, int] = {}
        for record in values:
            status = record["result"]["status"]
            statuses[status] = statuses.get(status, 0) + 1
        numeric_fields = (
            "revisions",
            "tool_calls",
            "input_tokens",
            "output_tokens",
            "wall_ms",
            "verifier_ms",
            "human_interventions",
        )
        medians: dict[str, float] = {}
        ranges: dict[str, dict[str, float]] = {}
        for field in numeric_fields:
            samples = [float(record["metrics"][field]) for record in values]
            medians[field] = statistics.median(samples)
            ranges[field] = {"min": min(samples), "max": max(samples)}
        success_count = statuses.get("success", 0)
        output_groups.append(
            {
                "task_id": task_id,
                "lane_id": lane_id,
                "raw_count": len(values),
                "success_count": success_count,
                "success_rate": success_count / len(values) if values else 0.0,
                "status_counts": statuses,
                "medians": medians,
                "ranges": ranges,
            }
        )

    mixed_profiles = {
        task_id: sorted(values)
        for task_id, values in profile_hashes_by_task.items()
        if len(values) > 1
    }
    return {
        "schema_version": SCHEMA_VERSION,
        "record_count": len(records),
        "groups": output_groups,
        "integrity": {
            "same_agent_profile_per_task": not mixed_profiles,
            "mixed_agent_profile_hashes": mixed_profiles,
        },
    }


def command_validate(args: argparse.Namespace) -> int:
    suite = validate_suite(Path(args.suite).resolve())
    print(canonical_json({"status": "ok", "suite_id": suite["suite_id"], "suite_sha256": sha256_file(Path(args.suite).resolve())}))
    return 0


def command_preflight(args: argparse.Namespace) -> int:
    suite = validate_suite(Path(args.suite).resolve())
    task = find_task(suite, args.task)
    find_lane(suite, args.lane)
    _, engine = executable_from_environment(args.lane)
    result: dict[str, Any] = {
        "status": "ok",
        "suite_id": suite["suite_id"],
        "task_id": args.task,
        "lane_id": args.lane,
        "engine": str(engine),
        "scored": bool(args.scored),
    }
    if args.scored:
        if suite.get("state") != "eligible" or task.get("state") != "eligible":
            raise HarnessError("scored preflight blocked until suite/task state is eligible")
        result["qualification"] = validate_qualification(suite, args.lane)
    print(canonical_json(result))
    return 0


def command_qualify_fixtures(args: argparse.Namespace) -> int:
    suite = validate_suite(Path(args.suite).resolve())
    find_lane(suite, args.lane)
    executable_from_environment(args.lane)
    evidence = fixture_qualification(suite, args.task, args.lane)
    print(json.dumps(evidence, indent=2, ensure_ascii=False))
    return 0 if evidence["qualified"] else 1


def command_run(args: argparse.Namespace) -> int:
    record = run_trial(args)
    print(json.dumps(record, indent=2, ensure_ascii=False))
    return 0 if record["result"]["status"] == "success" else 1


def command_reverify(args: argparse.Namespace) -> int:
    record = reverify_trial(args)
    print(json.dumps(record, indent=2, ensure_ascii=False))
    return 0 if record["workspace_matches_original"] and record["verdict_matches_original"] else 1


def command_report(args: argparse.Namespace) -> int:
    report = report_records(Path(args.records).resolve(), args.include_unscored)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report["integrity"]["same_agent_profile_per_task"] else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D Benchmark B0 matched harness")
    sub = parser.add_subparsers(dest="command", required=True)

    validate = sub.add_parser("validate-suite")
    validate.add_argument("--suite", default="benchmarks/b0/suite.json")
    validate.set_defaults(func=command_validate)

    preflight = sub.add_parser("preflight")
    preflight.add_argument("--suite", default="benchmarks/b0/suite.json")
    preflight.add_argument("--task", required=True)
    preflight.add_argument("--lane", choices=EXPECTED_LANES, required=True)
    preflight.add_argument("--scored", action="store_true")
    preflight.set_defaults(func=command_preflight)

    qualify = sub.add_parser("qualify-fixtures")
    qualify.add_argument("--suite", default="benchmarks/b0/suite.json")
    qualify.add_argument("--task", required=True)
    qualify.add_argument("--lane", choices=EXPECTED_LANES, required=True)
    qualify.set_defaults(func=command_qualify_fixtures)

    run = sub.add_parser("run-trial")
    run.add_argument("--suite", default="benchmarks/b0/suite.json")
    run.add_argument("--task", required=True)
    run.add_argument("--lane", choices=EXPECTED_LANES, required=True)
    run.add_argument("--agent-profile", required=True)
    run.add_argument("--runs-root", default="benchmark-runs/b0")
    run.add_argument("--trial-id")
    run.add_argument("--scored", action="store_true")
    run.set_defaults(func=command_run)

    replay = sub.add_parser("reverify")
    replay.add_argument("--suite", default="benchmarks/b0/suite.json")
    replay.add_argument("--records", default="benchmark-runs/b0/raw.jsonl")
    replay.add_argument("--replay-records", default="benchmark-runs/b0/replay.jsonl")
    replay.add_argument("--trial-id", required=True)
    replay.set_defaults(func=command_reverify)

    report = sub.add_parser("report")
    report.add_argument("--records", default="benchmark-runs/b0/raw.jsonl")
    report.add_argument("--include-unscored", action="store_true")
    report.set_defaults(func=command_report)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return int(args.func(args))
    except HarnessError as exc:
        print(canonical_json({"status": "error", "message": str(exc)}), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
