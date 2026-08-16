#!/usr/bin/env python3
"""Validate the pre-score Benchmark B2 P0 task/policy freeze.

P0 intentionally forbids scored execution. A later P1 change may open scoring
only after non-scored strongest-baseline and verifier qualification evidence is
committed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Any

EXPECTED_KIND = "trace2d_b2_preregistration"
EXPECTED_BENCHMARK = "trace2d-b2"
EXPECTED_STATE = "task_frozen_baseline_pending"
EXPECTED_BASE = "156202991ecd7a27f9205b2f32a5a97c27ec3943"
EXPECTED_B1_TREE = "0a6ecca662ceb3a04794b6f08cbc1fb4d0fb96b4"
EXPECTED_AGENT_PROFILE_HASH = "2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708"
EXPECTED_TASK = "b2-topdown-combat-v1"
EXPECTED_LANES = ["godot.generic", "godot.agent", "trace2d.agent"]
EXPECTED_BUDGET = {
    "wall_seconds": 900,
    "max_tool_calls": 120,
    "max_input_tokens": 100000,
    "max_output_tokens": 20000,
    "max_human_interventions": 1,
    "max_feedback_revision_cycles": 1,
}
EXPECTED_CANDIDATE_PINS = {
    "beremaran/godot-agent-loop": ("3.0.0", "7bc6062f90e2f96a04f997b202f7a24dd152a9fd"),
    "hi-godot/godot-ai": ("3.1.5", "09a1e3311015153d967710fbe6502ac519585a9b"),
    "satelliteoflove/godot-mcp": ("4.1.0", "1b7d40537240fd54300f54bf6fda1ea91f06c878"),
    "Erodenn/godot-mcp-runtime": ("3.2.2", "816b653117bdd6a432cb197e029b27d0a4f09540"),
}


class PreregistrationError(ValueError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise PreregistrationError(message)


def _load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise PreregistrationError(f"failed to read JSON {path}: {exc}") from exc
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


def _validate_repo_b1_tree(repo_root: Path) -> None:
    try:
        actual = subprocess.check_output(
            ["git", "-C", str(repo_root), "rev-parse", "HEAD:benchmarks/b1"],
            text=True,
            stderr=subprocess.STDOUT,
        ).strip()
    except (OSError, subprocess.CalledProcessError) as exc:
        raise PreregistrationError(f"unable to resolve frozen B1 tree identity: {exc}") from exc
    _require(actual == EXPECTED_B1_TREE, "frozen benchmarks/b1 tree changed before B2 scoring")


def validate_data(
    policy: dict[str, Any],
    candidates: dict[str, Any],
    *,
    agent_profile: dict[str, Any],
) -> None:
    _require(policy.get("schema_version") == 1, "schema_version must be 1")
    _require(policy.get("kind") == EXPECTED_KIND, f"kind must be {EXPECTED_KIND}")
    _require(policy.get("benchmark_id") == EXPECTED_BENCHMARK, f"benchmark_id must be {EXPECTED_BENCHMARK}")
    _require(policy.get("state") == EXPECTED_STATE, f"P0 state must remain {EXPECTED_STATE}")
    _require(policy.get("preregistration_base_commit") == EXPECTED_BASE, "P0 base commit changed")

    b1 = policy.get("historical_b1_anchor")
    _require(isinstance(b1, dict), "historical_b1_anchor is required")
    _require(b1.get("benchmark_tree_git_sha1") == EXPECTED_B1_TREE, "B1 tree anchor changed")
    _require(b1.get("immutable") is True, "B1 must remain immutable")
    _require(b1.get("rerun_or_replacement_forbidden") is True, "B1 rerun/replacement must remain forbidden")

    agent = policy.get("agent_identity")
    _require(isinstance(agent, dict), "agent_identity is required")
    _require(agent.get("canonical_sha256") == EXPECTED_AGENT_PROFILE_HASH, "Agent profile hash anchor changed")
    _require(_canonical_sha256(agent_profile) == EXPECTED_AGENT_PROFILE_HASH, "Agent profile content drifted")
    _require(agent.get("same_agent_across_all_lanes") is True, "same Agent must be used across lanes")

    task = policy.get("task_freeze")
    _require(isinstance(task, dict), "task_freeze is required")
    _require(task.get("state") == "frozen", "B2 task must remain frozen")
    _require(task.get("task_ids") == [EXPECTED_TASK], "B2 task membership changed")
    _require(task.get("membership_or_semantic_change_after_this_commit_forbidden") is True, "task mutation must be forbidden")
    _require(task.get("new_held_out_task_not_derived_from_b1_scored_fixtures") is True, "B2 task must remain held out from B1")

    _require(policy.get("budget") == EXPECTED_BUDGET, "B2 budget drifted after P0 freeze")
    rationale = policy.get("budget_rationale")
    _require(isinstance(rationale, dict), "budget_rationale is required")
    _require(rationale.get("token_ceiling_inherited_from_b1") is True, "B2 input/output token ceiling must remain B1-sized")
    _require(rationale.get("larger_token_budget_used_to_hide_b1_context_pressure") is False, "B2 may not hide B1 context pressure with a larger token budget")

    retry = policy.get("retry_policy")
    _require(isinstance(retry, dict), "retry_policy is required")
    _require(retry.get("automatic_retries_per_trial") == 0, "automatic retries must remain zero")
    _require(retry.get("replacement_trials_for_infrastructure_failure") == 0, "replacement trials must remain zero")
    _require(retry.get("early_stopping") is False, "early stopping must remain disabled")
    _require(retry.get("preserve_every_scheduled_attempt") is True, "every scheduled attempt must remain visible")
    _require(retry.get("best_of_n") is False, "best-of-N is forbidden")

    authority = policy.get("verifier_authority")
    _require(isinstance(authority, dict), "verifier_authority is required")
    deterministic = authority.get("deterministic")
    multimodal = authority.get("multimodal")
    human = authority.get("human_review")
    _require(isinstance(deterministic, dict) and deterministic.get("authoritative_for_gameplay_pass_fail") is True,
             "deterministic verifier must own gameplay pass/fail")
    _require(isinstance(multimodal, dict) and multimodal.get("may_override_deterministic_failure") is False,
             "multimodal review may not override deterministic failure")
    _require(isinstance(human, dict), "human_review policy is required")
    _require(human.get("max_feedback_events") == 1, "exactly one bounded human feedback event must remain allowed")
    _require(human.get("shared_lane_agnostic_feedback_text") is True, "feedback must be shared and lane-agnostic")
    _require(human.get("same_feedback_applied_to_every_eligible_lane") is True, "same feedback must reach all lanes")
    _require(human.get("deterministic_reverification_after_revision") is True, "feedback revision must be reverified")
    _require(human.get("may_override_deterministic_failure") is False, "human review may not override deterministic failure")

    planned = policy.get("planned_scored_cohort")
    _require(isinstance(planned, dict), "planned_scored_cohort is required")
    _require(planned.get("repetitions_per_task_lane") == 3, "B2 must retain three repetitions")
    _require(planned.get("lanes") == EXPECTED_LANES, "B2 lane identities changed")
    _require(planned.get("planned_total_trials") == 9, "B2 must plan exactly nine scored slots")
    _require(planned.get("schedule_file") is None, "P0 must not create a scored schedule before baseline qualification")
    _require(planned.get("schedule_creation_forbidden_until_baseline_selected") is True,
             "scored schedule must remain blocked until baseline selection")

    scoring = policy.get("scoring_gate")
    _require(isinstance(scoring, dict), "scoring_gate is required")
    _require(scoring.get("allowed") is False, "P0 must not authorize B2 scoring")
    _require(scoring.get("scored_results_observed") is False, "P0 is invalid after observing scored B2 results")

    _require(candidates.get("schema_version") == 1, "candidate schema_version must be 1")
    _require(candidates.get("benchmark_id") == EXPECTED_BENCHMARK, "candidate benchmark_id changed")
    _require(candidates.get("state") == "qualification_pending", "candidate set must remain qualification_pending")
    selection_rule = candidates.get("selection_rule")
    _require(isinstance(selection_rule, dict), "candidate selection_rule is required")
    _require(selection_rule.get("strongest_credible_normal_external_user_lane") is True,
             "strongest-baseline rule must remain enabled")
    _require(selection_rule.get("non_scored_fixture_required") is True, "non-scored qualification must remain required")
    _require(selection_rule.get("readme_claims_alone_are_not_qualification") is True,
             "README claims alone may not select the B2 baseline")

    actual_candidates = candidates.get("candidates")
    _require(isinstance(actual_candidates, list), "candidates must be an array")
    actual_pins: dict[str, tuple[str, str]] = {}
    for item in actual_candidates:
        _require(isinstance(item, dict), "candidate entries must be objects")
        candidate_id = str(item.get("id", ""))
        _require(candidate_id not in actual_pins, f"duplicate candidate id: {candidate_id}")
        actual_pins[candidate_id] = (str(item.get("version", "")), str(item.get("source_commit", "")))
    _require(actual_pins == EXPECTED_CANDIDATE_PINS, "B2 candidate identities drifted after P0 freeze")

    qualification = candidates.get("qualification")
    _require(isinstance(qualification, dict), "qualification block is required")
    _require(qualification.get("state") == "not_run", "P0 must precede strongest-baseline qualification")
    _require(qualification.get("selected_candidate_id") is None, "P0 must not preselect a candidate")
    _require(qualification.get("selected_pin") is None, "P0 must not preselect a pin")
    _require(qualification.get("scored_task_prompt_visible_to_qualification_agent") is False,
             "qualification must not expose the scored task prompt")

    freeze_gate = candidates.get("freeze_gate")
    _require(isinstance(freeze_gate, dict) and freeze_gate.get("scored_suite_allowed") is False,
             "candidate qualification gate must block scoring")


def validate_repository(repo_root: Path) -> None:
    policy = _load_json(repo_root / "benchmarks/b2/preregistration-v1.json")
    candidates = _load_json(repo_root / "benchmarks/b2/baseline-candidates.json")
    agent_profile = _load_json(repo_root / "benchmarks/b0/agent-profile.codex-0.144.6.json")
    prompt = repo_root / "benchmarks/b2/tasks/b2-topdown-combat-v1/PROMPT.md"
    _require(prompt.is_file() and prompt.stat().st_size > 0, "frozen B2 task prompt is missing")
    validate_data(policy, candidates, agent_profile=agent_profile)
    _validate_repo_b1_tree(repo_root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        validate_repository(args.repo_root.resolve())
    except PreregistrationError as exc:
        print(f"B2 preregistration invalid: {exc}")
        return 1
    print("B2 preregistration valid: task/policy frozen; scoring remains blocked pending baseline qualification.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
