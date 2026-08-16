#!/usr/bin/env python3
"""Validate Benchmark B2 P1 freeze and explicit scoring-gate evidence.

P1 freezes the selected Godot Agent identity and all nine scored slots. Scoring
may open only after the independent Trace2D and Godot verifier families are
qualified on committed known-good and meaningful known-bad fixtures.
"""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

from scripts import benchmark_b2_preregistration as p0

EXPECTED_STATE = "scoring_open"
EXPECTED_CANDIDATE_STATE = "selected_frozen"
EXPECTED_COHORT_STATE = "frozen_scoring_open"
EXPECTED_SELECTED_ID = "hi-godot/godot-ai"
EXPECTED_SELECTED_PIN = {
    "version": "3.1.5",
    "source_commit": "09a1e3311015153d967710fbe6502ac519585a9b",
    "package_identity": "sha256:51863ba177c66299808aa19ef6cd9069768915b2434d7787b9300e40c3620b04",
}
EXPECTED_QUALIFICATION = {
    "workflow_run_id": 31930609551,
    "artifact_id": 9259154101,
    "artifact_sha256": "7cc3439f506bc28ca064885c41b4c8432260e2c43e7e73f98650612be4d6a811",
}
EXPECTED_TRACE_VERIFIER = {
    "qualification_head": "12ada922f37fdd4e004a39cdb168dc610b8fdd4c",
    "merged_main_commit": "50cf6dff53e700b6b7312f6032deaea84b88655d",
    "workflow_run_id": 31938580902,
    "artifact_id": 9261437919,
    "artifact_sha256": "5c4bf6b0e253b480ddee5ad2a06836c96943d0b77def251d9481ff02f2707635",
}
EXPECTED_GODOT_VERIFIER = {
    "qualification_head": "a11592be1da77867e6e6792f97de6dbc21ea6b23",
    "merged_main_commit": "fd54588f3935d19a5d29ef2e473744b1154e7960",
    "workflow_run_id": 31939889431,
    "artifact_id": 9261721033,
    "artifact_sha256": "a7934b2cf2c55894b969d3a429aeb8afbcdb4cca9fe9033328da5e2ebf83c3cb",
}
EXPECTED_EVIDENCE_PATH = "benchmarks/b2/verifier-qualification-v1.json"
EXPECTED_LANE_ORDERS = [
    ["godot.generic", "godot.agent", "trace2d.agent"],
    ["godot.agent", "trace2d.agent", "godot.generic"],
    ["trace2d.agent", "godot.generic", "godot.agent"],
]


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise p0.PreregistrationError(message)


def _load(path: Path) -> dict[str, Any]:
    return p0._load_json(path)


def _validate_verifier_qualification(evidence: dict[str, Any]) -> None:
    _require(evidence.get("schema_version") == 1, "verifier qualification schema_version must be 1")
    _require(evidence.get("kind") == "trace2d_b2_verifier_qualification", "verifier qualification kind changed")
    _require(evidence.get("benchmark_id") == p0.EXPECTED_BENCHMARK, "verifier qualification benchmark changed")
    _require(evidence.get("state") == "qualified_for_scoring", "verifier qualification is not scoring-ready")
    _require(evidence.get("scored_task_results_observed_before_qualification") is False,
             "scored B2 results were observed before verifier qualification")

    trace = evidence.get("trace2d", {})
    _require(trace.get("lanes") == ["trace2d.agent"], "Trace2D verifier lane ownership changed")
    for key, value in EXPECTED_TRACE_VERIFIER.items():
        _require(trace.get(key) == value, f"Trace2D verifier {key} changed")
    trace_good = trace.get("known_good", {})
    trace_bad = trace.get("known_bad", {})
    _require(trace_good.get("six_fixed_step_cooldown") is True and trace_good.get("accepted") is True,
             "Trace2D known-good verifier qualification changed")
    _require(trace_bad.get("five_fixed_step_cooldown") is True and trace_bad.get("rejected") is True,
             "Trace2D known-bad verifier qualification changed")
    _require(trace_bad.get("expected_failure_checkpoint") == "frame-14 early second attack",
             "Trace2D known-bad checkpoint changed")

    godot = evidence.get("godot", {})
    _require(godot.get("lanes") == ["godot.generic", "godot.agent"], "Godot verifier lane ownership changed")
    _require(godot.get("godot_version") == "4.7.1-stable", "Godot verifier version changed")
    _require(godot.get("godot_engine_identity") == "4.7.1.stable.official.a13da4feb",
             "Godot verifier engine identity changed")
    for key, value in EXPECTED_GODOT_VERIFIER.items():
        _require(godot.get(key) == value, f"Godot verifier {key} changed")
    godot_good = godot.get("known_good", {})
    godot_bad = godot.get("known_bad", {})
    _require(godot_good.get("six_fixed_step_cooldown") is True and godot_good.get("accepted") is True,
             "Godot known-good verifier qualification changed")
    _require(godot_bad.get("five_fixed_step_cooldown") is True and godot_bad.get("rejected") is True,
             "Godot known-bad verifier qualification changed")
    _require(godot_bad.get("expected_failure_checkpoint") == "frame-14 early second attack",
             "Godot known-bad checkpoint changed")

    authority = evidence.get("authority", {})
    _require(authority.get("candidate_owns_verdict") is False, "candidate may not own B2 verdict")
    _require(authority.get("ordinary_semantic_input_path_required") is True, "ordinary semantic input path guard changed")
    _require(authority.get("deterministic_gameplay_pass_fail_owned_by_independent_verifier") is True,
             "independent deterministic verifier authority changed")
    _require(authority.get("presentation_capture_still_required_during_scored_b2") is True,
             "presentation evidence requirement changed")
    _require(authority.get("multimodal_or_human_review_may_override_deterministic_failure") is False,
             "perceptual review may not override deterministic failure")
    _require(evidence.get("scoring_gate_eligible") is True, "verifier evidence does not permit scoring")


def validate_data(
    policy: dict[str, Any],
    candidates: dict[str, Any],
    cohort: dict[str, Any],
    verifier_qualification: dict[str, Any],
    *,
    agent_profile: dict[str, Any],
) -> None:
    _require(policy.get("schema_version") == 1, "policy schema_version must be 1")
    _require(policy.get("kind") == p0.EXPECTED_KIND, "policy kind changed")
    _require(policy.get("benchmark_id") == p0.EXPECTED_BENCHMARK, "benchmark id changed")
    _require(policy.get("state") == EXPECTED_STATE, f"P1 state must be {EXPECTED_STATE}")
    _require(policy.get("preregistration_base_commit") == p0.EXPECTED_BASE, "P0 base commit changed")
    _require(policy.get("budget") == p0.EXPECTED_BUDGET, "B2 budget drifted")
    _require(p0._canonical_sha256(agent_profile) == p0.EXPECTED_AGENT_PROFILE_HASH, "Agent profile drifted")

    b1 = policy.get("historical_b1_anchor", {})
    _require(b1.get("benchmark_tree_git_sha1") == p0.EXPECTED_B1_TREE, "B1 tree anchor changed")
    _require(b1.get("immutable") is True, "B1 must remain immutable")

    task = policy.get("task_freeze", {})
    _require(task.get("state") == "frozen", "B2 task is not frozen")
    _require(task.get("task_ids") == [p0.EXPECTED_TASK], "B2 task membership changed")
    _require(task.get("membership_or_semantic_change_after_this_commit_forbidden") is True, "task mutation guard changed")

    baseline = policy.get("baseline_selection", {})
    _require(baseline.get("selection_state") == "frozen", "selected baseline is not frozen")
    _require(baseline.get("selected_godot_agent_candidate") == EXPECTED_SELECTED_ID, "selected Godot Agent changed")
    selected_pin = baseline.get("selected_pin", {})
    for key, value in EXPECTED_SELECTED_PIN.items():
        _require(selected_pin.get(key) == value, f"selected baseline {key} changed")
    _require(selected_pin.get("qualification_artifact_sha256") == EXPECTED_QUALIFICATION["artifact_sha256"],
             "selected baseline qualification artifact changed")

    _require(candidates.get("schema_version") == 1, "candidate schema_version changed")
    _require(candidates.get("benchmark_id") == p0.EXPECTED_BENCHMARK, "candidate benchmark id changed")
    _require(candidates.get("state") == EXPECTED_CANDIDATE_STATE, "candidate set is not frozen")
    actual_pins: dict[str, tuple[str, str]] = {}
    for item in candidates.get("candidates", []):
        _require(isinstance(item, dict), "candidate entry must be object")
        actual_pins[str(item.get("id", ""))] = (str(item.get("version", "")), str(item.get("source_commit", "")))
    _require(actual_pins == p0.EXPECTED_CANDIDATE_PINS, "candidate identities drifted from P0")

    qualification = candidates.get("qualification", {})
    _require(qualification.get("state") == "passed_selection_frozen", "baseline qualification not frozen")
    _require(qualification.get("scored_task_prompt_visible_to_qualification_agent") is False,
             "qualification exposed scored prompt")
    _require(qualification.get("selected_candidate_id") == EXPECTED_SELECTED_ID, "candidate selection changed")
    candidate_pin = qualification.get("selected_pin", {})
    for key, value in EXPECTED_SELECTED_PIN.items():
        _require(candidate_pin.get(key) == value, f"candidate selected pin {key} changed")
    evidence = qualification.get("evidence", {})
    _require(evidence.get("godot_ai_workflow_run_id") == EXPECTED_QUALIFICATION["workflow_run_id"],
             "Godot AI qualification run changed")
    _require(evidence.get("godot_ai_artifact_id") == EXPECTED_QUALIFICATION["artifact_id"],
             "Godot AI qualification artifact id changed")
    _require(evidence.get("godot_ai_artifact_sha256") == EXPECTED_QUALIFICATION["artifact_sha256"],
             "Godot AI qualification digest changed")

    planned = policy.get("planned_scored_cohort", {})
    _require(planned.get("repetitions_per_task_lane") == 3, "B2 repetitions changed")
    _require(planned.get("lanes") == p0.EXPECTED_LANES, "B2 lanes changed")
    _require(planned.get("planned_total_trials") == 9, "B2 total trials changed")
    _require(planned.get("schedule_file") == "benchmarks/b2/scored-cohort-v1.json", "scored schedule path changed")
    _require(planned.get("schedule_created_after_baseline_selection") is True, "schedule ordering guard missing")

    _require(cohort.get("schema_version") == 1, "cohort schema_version must be 1")
    _require(cohort.get("kind") == "trace2d_b2_scored_cohort_policy", "cohort kind changed")
    _require(cohort.get("state") == EXPECTED_COHORT_STATE, "cohort is not frozen with scoring open")
    _require(cohort.get("task_id") == p0.EXPECTED_TASK, "cohort task changed")
    _require(cohort.get("agent_profile_canonical_sha256") == p0.EXPECTED_AGENT_PROFILE_HASH, "cohort Agent changed")
    _require(cohort.get("lane_order_by_repetition") == EXPECTED_LANE_ORDERS, "lane rotation changed")
    _require(cohort.get("budget") == p0.EXPECTED_BUDGET, "cohort budget changed")

    selected = cohort.get("selected_godot_agent", {})
    _require(selected.get("id") == EXPECTED_SELECTED_ID, "cohort selected agent changed")
    for key, value in EXPECTED_SELECTED_PIN.items():
        _require(selected.get(key) == value, f"cohort selected agent {key} changed")
    _require(selected.get("qualification_workflow_run_id") == EXPECTED_QUALIFICATION["workflow_run_id"],
             "cohort qualification run changed")
    _require(selected.get("qualification_artifact_id") == EXPECTED_QUALIFICATION["artifact_id"],
             "cohort qualification artifact id changed")
    _require(selected.get("qualification_artifact_sha256") == EXPECTED_QUALIFICATION["artifact_sha256"],
             "cohort qualification digest changed")

    slots = cohort.get("slots")
    _require(isinstance(slots, list) and len(slots) == 9, "cohort must contain exactly nine slots")
    expected_slots = []
    slot_id = 1
    for repetition, lane_order in enumerate(EXPECTED_LANE_ORDERS, start=1):
        for lane in lane_order:
            expected_slots.append({
                "slot": slot_id,
                "repetition": repetition,
                "task": p0.EXPECTED_TASK,
                "lane": lane,
            })
            slot_id += 1
    _require(slots == expected_slots, "explicit nine-slot schedule changed")

    _validate_verifier_qualification(verifier_qualification)

    scoring = policy.get("scoring_gate", {})
    _require(scoring.get("allowed") is True, "scoring gate must be open after verifier qualification")
    _require(scoring.get("scored_results_observed") is False, "scored results observed before scoring-gate freeze")
    _require(scoring.get("qualification_evidence") == EXPECTED_EVIDENCE_PATH, "policy verifier evidence path changed")
    _require(scoring.get("remaining_requirements_before_opening") == [], "scoring gate still has unresolved blockers")

    cohort_gate = cohort.get("scoring_gate", {})
    _require(cohort_gate.get("allowed") is True, "cohort scoring gate is not open")
    _require(cohort_gate.get("scored_results_observed") is False, "cohort claims scored results before gate freeze")
    _require(cohort_gate.get("qualification_evidence") == EXPECTED_EVIDENCE_PATH, "cohort verifier evidence path changed")
    _require(cohort_gate.get("remaining_blocker") is None, "cohort still has a scoring blocker")

    candidate_gate = candidates.get("freeze_gate", {})
    _require(candidate_gate.get("scored_suite_allowed") is True, "candidate freeze gate did not open")
    _require(candidate_gate.get("verifier_qualification_evidence") == EXPECTED_EVIDENCE_PATH,
             "candidate verifier evidence path changed")


def validate_repository(repo_root: Path) -> None:
    policy = _load(repo_root / "benchmarks/b2/preregistration-v1.json")
    candidates = _load(repo_root / "benchmarks/b2/baseline-candidates.json")
    cohort = _load(repo_root / "benchmarks/b2/scored-cohort-v1.json")
    verifier_qualification = _load(repo_root / EXPECTED_EVIDENCE_PATH)
    agent_profile = _load(repo_root / "benchmarks/b0/agent-profile.codex-0.144.6.json")
    validate_data(
        policy,
        candidates,
        cohort,
        verifier_qualification,
        agent_profile=agent_profile,
    )
    p0._validate_repo_b1_tree(repo_root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        validate_repository(args.repo_root.resolve())
    except p0.PreregistrationError as exc:
        print(f"B2 P1 freeze invalid: {exc}")
        return 1
    print("B2 P1 freeze valid: baseline, cohort, and independent verifier evidence frozen; scoring gate open.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
