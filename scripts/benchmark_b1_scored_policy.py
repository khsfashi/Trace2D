#!/usr/bin/env python3
"""Validate the preregistered Benchmark B1 scored-cohort execution policy.

The B1 suite, task fixtures, verifier registry, and freeze manifest are already
frozen. This module validates only the execution schedule and exact selected
Agent environment layered on top of that frozen content. It deliberately does
not mutate or regenerate frozen inputs.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
from pathlib import Path
from typing import Any

import benchmark_b1

EXPECTED_KIND = "trace2d_b1_scored_cohort_policy"
EXPECTED_COHORT = "trace2d-b1-content-authoring-scored-v1"
EXPECTED_REPETITIONS = 3
EXPECTED_TOTAL = 27
EXPECTED_GODOT_AI_COMMIT = "f3d99dfbd38c9e095edf1467f85bee507ace2c3a"
EXPECTED_GODOT_AI_QUALIFICATION_RUN = 31622618958
EXPECTED_GODOT_AI_QUALIFICATION_ARTIFACT = 9151863240
EXPECTED_GODOT_AI_QUALIFICATION_ARTIFACT_SHA256 = "799fb557100c3c39b4421b8fe4abc85dbe79693826083089780a607c333b6cb2"
EXPECTED_GODOT_AI_FREEZE_SHA256 = "75ae3394599cefe2426e3921403b6d2448463e7aa3ec3272b15313bccf6eb73e"


class ScoredPolicyError(ValueError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ScoredPolicyError(message)


def _load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ScoredPolicyError(f"failed to read JSON {path}: {exc}") from exc
    _require(isinstance(value, dict), f"expected JSON object: {path}")
    return value


def _canonical_sha256(value: Any) -> str:
    encoded = json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def expand_schedule(policy: dict[str, Any]) -> list[dict[str, Any]]:
    task_orders = policy.get("task_order_by_repetition")
    lane_orders = policy.get("lane_order_by_repetition")
    _require(isinstance(task_orders, list), "task_order_by_repetition must be an array")
    _require(isinstance(lane_orders, list), "lane_order_by_repetition must be an array")
    _require(
        len(task_orders) == EXPECTED_REPETITIONS,
        f"task order must contain exactly {EXPECTED_REPETITIONS} repetitions",
    )
    _require(
        len(lane_orders) == EXPECTED_REPETITIONS,
        f"lane order must contain exactly {EXPECTED_REPETITIONS} repetitions",
    )

    expected_tasks = tuple(benchmark_b1.EXPECTED_TASKS)
    expected_lanes = tuple(benchmark_b1.EXPECTED_LANES)
    schedule: list[dict[str, Any]] = []
    slot = 0
    for repetition in range(1, EXPECTED_REPETITIONS + 1):
        task_order = task_orders[repetition - 1]
        lane_order = lane_orders[repetition - 1]
        _require(
            isinstance(task_order, list)
            and len(task_order) == len(expected_tasks)
            and set(task_order) == set(expected_tasks),
            f"repetition {repetition} task order must contain every frozen B1 task exactly once",
        )
        _require(
            isinstance(lane_order, list)
            and len(lane_order) == len(expected_lanes)
            and set(lane_order) == set(expected_lanes),
            f"repetition {repetition} lane order must contain every B1 lane exactly once",
        )
        for task_id in task_order:
            for lane_id in lane_order:
                slot += 1
                schedule.append(
                    {
                        "slot": slot,
                        "repetition": repetition,
                        "task_id": str(task_id),
                        "lane_id": str(lane_id),
                    }
                )

    _require(len(schedule) == EXPECTED_TOTAL, f"expanded schedule must contain {EXPECTED_TOTAL} slots")
    return schedule


def validate_policy_data(
    policy: dict[str, Any],
    *,
    suite: dict[str, Any],
    qualification: dict[str, Any],
    profile: dict[str, Any],
    b0_policy: dict[str, Any],
) -> list[dict[str, Any]]:
    _require(policy.get("schema_version") == 1, "policy schema_version must be 1")
    _require(policy.get("kind") == EXPECTED_KIND, f"policy kind must be {EXPECTED_KIND}")
    _require(policy.get("cohort_id") == EXPECTED_COHORT, f"cohort_id must be {EXPECTED_COHORT}")
    _require(policy.get("state") == "ready", "scored cohort policy must remain ready")
    _require(suite.get("suite_id") == "trace2d-b1", "policy must target trace2d-b1")
    _require(suite.get("state") == "frozen", "B1 suite must remain frozen before scoring")

    _require(
        qualification.get("benchmark_id") == "trace2d-b1"
        and qualification.get("qualification_state") == "passed",
        "B1 fixture qualification must be passed",
    )
    _require(qualification.get("scoring_eligible") is True, "fixture qualification must authorize scoring")
    _require(
        qualification.get("scored_runs_observed") is False,
        "preregistered policy validation is only valid before scored B1 results are observed",
    )
    dispatch = qualification.get("dispatch")
    _require(isinstance(dispatch, dict), "fixture qualification dispatch is missing")
    for engine in ("godot", "trace2d"):
        _require(
            isinstance(dispatch.get(engine), dict) and dispatch[engine].get("qualified") is True,
            f"fixture qualification must retain positive {engine} dispatch evidence",
        )

    expected_profile_hash = str(policy.get("agent_profile_canonical_sha256", ""))
    _require(len(expected_profile_hash) == 64, "agent profile canonical SHA-256 must be recorded")
    _require(
        _canonical_sha256(profile) == expected_profile_hash,
        "agent profile canonical SHA-256 changed after preregistration",
    )

    godot_python = policy.get("godot_agent_python_environment")
    _require(isinstance(godot_python, dict), "selected Godot Agent Python environment must be frozen")
    _require(
        godot_python.get("qualification_workflow_run_id") == EXPECTED_GODOT_AI_QUALIFICATION_RUN,
        "selected Godot Agent qualification workflow changed",
    )
    _require(
        godot_python.get("qualification_artifact_id") == EXPECTED_GODOT_AI_QUALIFICATION_ARTIFACT,
        "selected Godot Agent qualification artifact changed",
    )
    _require(
        godot_python.get("qualification_artifact_sha256") == EXPECTED_GODOT_AI_QUALIFICATION_ARTIFACT_SHA256,
        "selected Godot Agent qualification artifact digest changed",
    )
    _require(
        godot_python.get("freeze_sha256") == EXPECTED_GODOT_AI_FREEZE_SHA256,
        "selected Godot Agent Python freeze digest changed",
    )
    _require(
        godot_python.get("selected_source_commit") == EXPECTED_GODOT_AI_COMMIT,
        "selected Godot Agent source commit changed",
    )

    budget = policy.get("budget")
    _require(isinstance(budget, dict), "policy budget must be an object")
    _require(profile.get("budget") == budget, "B1 policy budget must exactly match the frozen Agent profile")
    for task in suite.get("tasks", []):
        _require(
            isinstance(task, dict) and task.get("budget") == budget,
            f"task {task.get('id') if isinstance(task, dict) else '<invalid>'} budget drifted from the scored policy",
        )

    _require(
        int(policy.get("repetitions_per_task_lane", -1)) == EXPECTED_REPETITIONS,
        f"repetitions_per_task_lane must remain {EXPECTED_REPETITIONS}",
    )
    _require(
        int(policy.get("total_planned_trials", -1)) == EXPECTED_TOTAL,
        f"total_planned_trials must remain {EXPECTED_TOTAL}",
    )

    retry = policy.get("retry_policy")
    _require(isinstance(retry, dict), "retry_policy must be an object")
    _require(retry.get("automatic_retries_per_trial") == 0, "automatic retries must remain zero")
    _require(
        retry.get("replacement_trials_for_infrastructure_failure") == 0,
        "replacement trials must remain zero",
    )
    _require(retry.get("early_stopping") is False, "early stopping must remain disabled")
    _require(retry.get("preserve_every_scheduled_attempt") is True, "every scheduled attempt must be preserved")

    reporting = policy.get("reporting_policy")
    _require(isinstance(reporting, dict), "reporting_policy must be an object")
    for key in (
        "publish_raw_sample_count",
        "publish_status_distribution",
        "publish_resource_distributions",
        "publish_per_task_per_lane_results",
        "separate_infrastructure_from_implementation",
        "separate_budget_exceeded_from_transport_failure",
        "presentation_review_separate_from_deterministic_pass_fail",
    ):
        _require(reporting.get(key) is True, f"reporting_policy.{key} must remain true")
    _require(reporting.get("best_of_n") is False, "best-of-N reporting is forbidden")
    _require(reporting.get("weighted_composite_score") is False, "weighted composite scoring is forbidden")
    _require(reporting.get("broad_superiority_claim_allowed") is False, "broad superiority claims are forbidden")

    _require(b0_policy.get("state") == "ready", "inherited B0 scored policy must remain ready")
    _require(
        b0_policy.get("repetitions_per_lane") == EXPECTED_REPETITIONS,
        "B1 must inherit B0's three-repetition policy",
    )
    _require(b0_policy.get("budget") == budget, "B1 budget must remain identical to B0")
    _require(
        policy.get("lane_order_by_repetition") == b0_policy.get("lane_order_by_repetition"),
        "B1 lane rotation must exactly reuse the preregistered B0 lane rotation",
    )
    inherited_retry = b0_policy.get("retry_policy", {})
    for key in (
        "automatic_retries_per_trial",
        "replacement_trials_for_infrastructure_failure",
        "early_stopping",
    ):
        _require(retry.get(key) == inherited_retry.get(key), f"B1 retry policy drifted from B0 at {key}")

    schedule = expand_schedule(policy)
    pair_counts = collections.Counter((item["task_id"], item["lane_id"]) for item in schedule)
    expected_pairs = {
        (task_id, lane_id)
        for task_id in benchmark_b1.EXPECTED_TASKS
        for lane_id in benchmark_b1.EXPECTED_LANES
    }
    _require(set(pair_counts) == expected_pairs, "expanded schedule must cover every frozen task/lane pair")
    _require(
        all(count == EXPECTED_REPETITIONS for count in pair_counts.values()),
        "every frozen task/lane pair must receive exactly three scheduled attempts",
    )
    return schedule


def validate_selected_agent_freeze(policy: dict[str, Any], repo_root: Path) -> Path:
    environment = policy.get("godot_agent_python_environment")
    _require(isinstance(environment, dict), "selected Godot Agent Python environment must be frozen")
    relative = environment.get("freeze")
    _require(isinstance(relative, str) and relative, "Godot Agent Python freeze path is required")
    path = (repo_root / relative).resolve()
    root = repo_root.resolve()
    _require(path == root or root in path.parents, "Godot Agent Python freeze must stay inside repository root")
    _require(path.is_file(), f"Godot Agent Python freeze does not exist: {relative}")
    observed = _file_sha256(path)
    expected = str(environment.get("freeze_sha256", ""))
    _require(observed == expected, f"Godot Agent Python freeze SHA-256 mismatch: expected {expected}, got {observed}")
    text = path.read_text(encoding="utf-8")
    _require("godot-ai @ file://" in text, "qualification freeze must retain the original local selected-source install record")
    _require("fastmcp==3.4.7" in text and "mcp==1.29.0" in text, "qualification freeze lost selected bridge transport dependencies")
    return path


def load_and_validate_policy(policy_path: Path, repo_root: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    policy = _load_json(policy_path)

    def repo_path(value: Any, field: str) -> Path:
        _require(isinstance(value, str) and value, f"{field} must be a repository path")
        path = (repo_root / value).resolve()
        root = repo_root.resolve()
        _require(path == root or root in path.parents, f"{field} must stay inside repository root")
        _require(path.is_file(), f"{field} does not exist: {value}")
        return path

    suite_path = repo_path(policy.get("suite"), "suite")
    qualification_path = repo_path(policy.get("fixture_qualification"), "fixture_qualification")
    profile_path = repo_path(policy.get("agent_profile"), "agent_profile")
    inherited_path = repo_path(policy.get("preregistration", {}).get("inherits"), "preregistration.inherits")

    suite = benchmark_b1.load_and_validate_suite(suite_path, repo_root)
    qualification = _load_json(qualification_path)
    profile = _load_json(profile_path)
    b0_policy = _load_json(inherited_path)
    schedule = validate_policy_data(
        policy,
        suite=suite,
        qualification=qualification,
        profile=profile,
        b0_policy=b0_policy,
    )
    validate_selected_agent_freeze(policy, repo_root)
    return policy, schedule


def main() -> int:
    parser = argparse.ArgumentParser(description="Trace2D Benchmark B1 scored-cohort policy")
    parser.add_argument(
        "--policy",
        default="benchmarks/b1/scored-cohort-v1.json",
        help="repository-relative or absolute scored policy path",
    )
    parser.add_argument("--print-schedule", action="store_true")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    policy_path = Path(args.policy)
    if not policy_path.is_absolute():
        policy_path = repo_root / policy_path
    policy, schedule = load_and_validate_policy(policy_path.resolve(), repo_root)
    print(
        f"Benchmark B1 scored cohort policy: OK ({policy['cohort_id']}, {len(schedule)} preregistered slots)"
    )
    if args.print_schedule:
        print(json.dumps(schedule, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
