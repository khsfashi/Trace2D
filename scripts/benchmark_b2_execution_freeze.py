#!/usr/bin/env python3
"""Validate the immutable pre-score execution inputs for Benchmark B2.

This layer freezes only neutral lane starters and scored-recording mechanics.
The task, nine-slot schedule, budget, retry policy, selected baseline and
independent verifier authority remain owned by the already-frozen P0/P1 files.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from scripts import benchmark_b2_p1_freeze as p1

EXPECTED_KIND = "trace2d_b2_scored_execution_contract"
EXPECTED_STATE = "frozen_pre_score"
EXPECTED_BASE_COMMIT = "80edeef32d593d02979f7dea86690fc857a115ed"
EXPECTED_TASK = "b2-topdown-combat-v1"
EXPECTED_GODOT_BRIDGE = {
    "id": "hi-godot/godot-ai",
    "version": "3.1.5",
    "source_commit": "09a1e3311015153d967710fbe6502ac519585a9b",
    "package_identity": "sha256:51863ba177c66299808aa19ef6cd9069768915b2434d7787b9300e40c3620b04",
}
EXPECTED_INPUTS = {
    "preregistration": "benchmarks/b2/preregistration-v1.json",
    "scored_cohort": "benchmarks/b2/scored-cohort-v1.json",
    "verifier_qualification": "benchmarks/b2/verifier-qualification-v1.json",
    "task_prompt": "benchmarks/b2/tasks/b2-topdown-combat-v1/PROMPT.md",
    "agent_profile": "benchmarks/b0/agent-profile.codex-0.144.6.json",
}
EXPECTED_SLOTS = [
    {"slot": 1, "repetition": 1, "task": EXPECTED_TASK, "lane": "godot.generic"},
    {"slot": 2, "repetition": 1, "task": EXPECTED_TASK, "lane": "godot.agent"},
    {"slot": 3, "repetition": 1, "task": EXPECTED_TASK, "lane": "trace2d.agent"},
    {"slot": 4, "repetition": 2, "task": EXPECTED_TASK, "lane": "godot.agent"},
    {"slot": 5, "repetition": 2, "task": EXPECTED_TASK, "lane": "trace2d.agent"},
    {"slot": 6, "repetition": 2, "task": EXPECTED_TASK, "lane": "godot.generic"},
    {"slot": 7, "repetition": 3, "task": EXPECTED_TASK, "lane": "trace2d.agent"},
    {"slot": 8, "repetition": 3, "task": EXPECTED_TASK, "lane": "godot.generic"},
    {"slot": 9, "repetition": 3, "task": EXPECTED_TASK, "lane": "godot.agent"},
]
EXPECTED_EXECUTION_POLICY = {
    "isolated_workspace_per_slot": True,
    "append_only_hash_chained_raw_records": True,
    "preserve_every_scheduled_attempt": True,
    "automatic_retries_per_trial": 0,
    "replacement_trials_for_infrastructure_failure": 0,
    "early_stopping": False,
    "best_of_n": False,
    "independent_deterministic_verifier_after_agent": True,
    "presentation_capture_required": True,
    "candidate_owns_verdict": False,
}
EXPECTED_FEEDBACK_POLICY = {
    "initial_deterministic_acceptance_required_before_feedback": True,
    "max_feedback_events": 1,
    "shared_lane_agnostic_feedback_text": True,
    "same_feedback_applied_to_every_eligible_lane": True,
    "max_feedback_revision_cycles": 1,
    "deterministic_reverification_after_revision": True,
}
EXPECTED_FAIRNESS = {
    "starter_contains_task_solution_logic": False,
    "starter_contains_frozen_semantic_actions": False,
    "godot_lanes_share_byte_identical_starter": True,
    "task_schedule_budget_retry_and_verifier_authority_changed": False,
    "slot_execution_before_this_freeze": False,
}
FORBIDDEN_STARTER_TOKENS = (
    "player",
    "enemy",
    "move_left",
    "move_right",
    "move_up",
    "move_down",
    "attack",
    "cooldown",
)


class ExecutionFreezeError(RuntimeError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ExecutionFreezeError(message)


def _load(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ExecutionFreezeError(f"failed to read JSON {path}: {exc}") from exc
    _require(isinstance(value, dict), f"expected JSON object: {path}")
    return value


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _validate_starter(root: Path, entry: dict[str, Any], *, lane: str) -> None:
    starter_root = entry.get("root")
    files = entry.get("files")
    _require(isinstance(starter_root, str) and starter_root, f"{lane} starter root missing")
    _require(isinstance(files, dict) and files, f"{lane} starter files missing")

    directory = (root / starter_root).resolve()
    _require(directory.is_dir(), f"{lane} starter directory missing: {starter_root}")

    actual_files = sorted(
        path.relative_to(directory).as_posix()
        for path in directory.rglob("*")
        if path.is_file()
    )
    expected_files = sorted(str(path) for path in files)
    _require(actual_files == expected_files, f"{lane} starter file set drifted")

    combined = bytearray()
    for relative in expected_files:
        expected_digest = files.get(relative)
        _require(isinstance(expected_digest, str) and len(expected_digest) == 64,
                 f"{lane} starter digest malformed: {relative}")
        path = directory / relative
        _require(_sha256(path) == expected_digest, f"{lane} starter changed: {relative}")
        combined.extend(path.read_bytes())
        combined.extend(b"\n")

    lowered = combined.decode("utf-8", errors="ignore").casefold()
    for token in FORBIDDEN_STARTER_TOKENS:
        _require(token not in lowered, f"{lane} starter leaks frozen task token: {token}")


def validate_execution_data(contract: dict[str, Any], repo_root: Path) -> None:
    _require(contract.get("schema_version") == 1, "execution schema_version must be 1")
    _require(contract.get("kind") == EXPECTED_KIND, "execution contract kind changed")
    _require(contract.get("benchmark_id") == "trace2d-b2", "execution benchmark id changed")
    _require(contract.get("issue") == 104, "execution issue changed")
    _require(contract.get("state") == EXPECTED_STATE, f"execution state must be {EXPECTED_STATE}")
    _require(contract.get("frozen_from_main_commit") == EXPECTED_BASE_COMMIT,
             "execution freeze base commit changed")
    _require(contract.get("scored_results_observed_before_execution_freeze") is False,
             "scored result was observed before execution inputs were frozen")
    _require(contract.get("frozen_inputs") == EXPECTED_INPUTS, "execution frozen-input references changed")
    _require(contract.get("slots") == EXPECTED_SLOTS, "execution slot order changed")
    _require(contract.get("execution_policy") == EXPECTED_EXECUTION_POLICY, "execution policy changed")
    _require(contract.get("human_feedback_policy") == EXPECTED_FEEDBACK_POLICY, "feedback policy changed")
    _require(contract.get("fairness_guards") == EXPECTED_FAIRNESS, "execution fairness guards changed")

    lanes = contract.get("lane_starters")
    _require(isinstance(lanes, dict), "lane starter map missing")
    _require(set(lanes) == {"godot.generic", "godot.agent", "trace2d.agent"}, "lane starter set changed")

    generic = lanes["godot.generic"]
    agent = lanes["godot.agent"]
    trace = lanes["trace2d.agent"]
    _require(generic.get("root") == agent.get("root"), "Godot lanes no longer share one starter root")
    _require(generic.get("files") == agent.get("files"), "Godot lane starter bytes differ")
    _require(generic.get("shares_identical_project_with") == "godot.agent",
             "Godot identical-starter guard changed")
    _require(agent.get("selected_bridge") == EXPECTED_GODOT_BRIDGE, "selected Godot Agent bridge changed")

    _validate_starter(repo_root, generic, lane="godot.generic")
    _validate_starter(repo_root, agent, lane="godot.agent")
    _validate_starter(repo_root, trace, lane="trace2d.agent")


def validate_repository(repo_root: Path) -> None:
    p1.validate_repository(repo_root)
    contract = _load(repo_root / "benchmarks/b2/execution-v1.json")
    validate_execution_data(contract, repo_root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        validate_repository(args.repo_root.resolve())
    except (ExecutionFreezeError, p1.p0.PreregistrationError) as exc:
        print(f"B2 scored execution freeze invalid: {exc}")
        return 1
    print("B2 scored execution inputs are frozen and scoring remains unobserved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
