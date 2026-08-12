#!/usr/bin/env python3
"""Validate Benchmark B1 baseline selection and frozen scored-suite contract."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
from typing import Any

REQUIRED_TASK_CLASSES = (
    "sprite_import_or_normalize",
    "trim_pivot_alignment_repair",
    "deterministic_animation_exact_event",
    "particle_structural_performance_budget",
    "exact_frame_headless_and_presentation_evidence",
    "seeded_content_defect_diagnosis_and_repair",
)
REQUIRED_CANDIDATES = (
    "satelliteoflove/godot-mcp",
    "hi-godot/godot-ai",
    "Erodenn/godot-mcp-runtime",
)
EXPECTED_LANES = ("godot.generic", "godot.agent", "trace2d.agent")
EXPECTED_TASKS = (
    "b1-sprite-normalize-repair",
    "b1-animation-exact-event",
    "b1-particle-budget-repair",
)
EXPECTED_TASK_CLASS_MAP = {
    "b1-sprite-normalize-repair": ("sprite_import_or_normalize", "trim_pivot_alignment_repair"),
    "b1-animation-exact-event": ("deterministic_animation_exact_event", "exact_frame_headless_and_presentation_evidence"),
    "b1-particle-budget-repair": ("particle_structural_performance_budget", "seeded_content_defect_diagnosis_and_repair"),
}
EXPECTED_VERIFIERS = (
    "godot-b1-sprite-normalize-repair-v1",
    "trace2d-b1-sprite-normalize-repair-v1",
    "godot-b1-animation-exact-event-v1",
    "trace2d-b1-animation-exact-event-v1",
    "godot-b1-particle-budget-repair-v1",
    "trace2d-b1-particle-budget-repair-v1",
)
EVIDENCE_LAYERS = (
    "deterministic_assertions",
    "presentation_artifacts",
    "multimodal_advisory_review",
    "human_final_judgment",
)
SELECTED_ID = "hi-godot/godot-ai"
SELECTED_PIN = "v3.0.6@f3d99dfbd38c9e095edf1467f85bee507ace2c3a"
QUALIFICATION_RUN = 31622618958
FROZEN_TRACE2D_COMMIT = "31712ca419efb232d292680661caea51d8a318e4"


class ContractError(ValueError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def _load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ContractError(f"failed to read JSON {path}: {exc}") from exc
    _require(isinstance(value, dict), f"expected JSON object: {path}")
    return value


def _repo_path(repo_root: Path, value: Any, field: str) -> Path:
    _require(isinstance(value, str) and value, f"{field} must be a repository path")
    path = (repo_root / value).resolve()
    root = repo_root.resolve()
    _require(path == root or root in path.parents, f"{field} must stay inside repository root")
    return path


def validate_contract(data: dict[str, Any], repo_root: Path) -> None:
    _require(data.get("schema_version") == 1, "schema_version must be 1")
    _require(data.get("benchmark_id") == "trace2d-b1", "benchmark_id must be trace2d-b1")
    _require(data.get("state") == "baseline_selected", "B1 strongest baseline must be selected before scored task freeze")

    inherits = data.get("inherits")
    _require(isinstance(inherits, dict), "inherits must be an object")
    _require(inherits.get("godot_version") == "4.7.1-stable", "B1 must reuse frozen Godot 4.7.1-stable")
    for key in ("b0_suite", "agent_profile"):
        path = _repo_path(repo_root, inherits.get(key), f"inherits.{key}")
        _require(path.is_file(), f"inherited file does not exist: {inherits.get(key)}")

    _require(
        data.get("required_task_classes") == list(REQUIRED_TASK_CLASSES),
        "required_task_classes must preserve the #103 preregistered order and membership",
    )

    selection_rule = data.get("selection_rule")
    _require(isinstance(selection_rule, dict), "selection_rule must be an object")
    for key in (
        "must_precede_scored_task_freeze",
        "non_scored_fixture_required",
        "ordinary_external_user_workflow_only",
        "task_specific_solution_logic_forbidden",
    ):
        _require(selection_rule.get(key) is True, f"selection_rule.{key} must be true")

    candidates = data.get("candidates")
    _require(isinstance(candidates, list), "candidates must be an array")
    ids = [candidate.get("id") for candidate in candidates if isinstance(candidate, dict)]
    _require(tuple(ids) == REQUIRED_CANDIDATES, "candidate membership/order must match the reviewed B1 baseline set")
    _require(len(set(ids)) == len(ids), "candidate ids must be unique")
    for candidate in candidates:
        _require(isinstance(candidate, dict), "candidate entry must be an object")
        _require(isinstance(candidate.get("pin"), str) and candidate["pin"], f"{candidate.get('id')} must have an exact pin")
        source = candidate.get("source")
        _require(isinstance(source, str) and source.startswith("https://github.com/"), f"{candidate.get('id')} must record a primary GitHub source")
        notes = candidate.get("capability_notes")
        _require(isinstance(notes, list) and notes, f"{candidate.get('id')} must record reviewed capability notes")

    satellite = candidates[0]
    satellite_result = satellite.get("qualification_result")
    _require(isinstance(satellite_result, dict), "satellite qualification result must be recorded")
    _require(satellite_result.get("qualified") is False, "satellite must retain the observed B1 qualification failure")
    _require(satellite_result.get("workflow_run_id") == QUALIFICATION_RUN, "satellite qualification run must remain pinned")
    _require(
        satellite_result.get("reason_code") == "godot_4_7_method_track_api_incompatible",
        "satellite failure reason must remain explicit",
    )

    hi_godot = candidates[1]
    _require(hi_godot.get("pin") == SELECTED_PIN, "selected hi-godot pin changed")
    hi_result = hi_godot.get("qualification_result")
    _require(isinstance(hi_result, dict), "selected candidate qualification result must be recorded")
    _require(hi_result.get("qualified") is True, "selected candidate must have passed qualification")
    _require(hi_result.get("workflow_run_id") == QUALIFICATION_RUN, "selected qualification run must remain pinned")
    for key in ("known_good_accepted", "known_bad_rejected", "presentation_capture_handoff"):
        _require(hi_result.get(key) is True, f"selected candidate evidence missing {key}")

    qualification = data.get("qualification")
    _require(isinstance(qualification, dict), "qualification must be an object")
    _require(qualification.get("status") == "passed", "qualification must be passed before scored freeze")
    _require(qualification.get("workflow_run_id") == QUALIFICATION_RUN, "qualification workflow run must remain pinned")
    _require(qualification.get("selected_candidate_id") == SELECTED_ID, "selected B1 Godot Agent changed")
    _require(qualification.get("selected_pin") == SELECTED_PIN, "selected B1 Godot Agent pin changed")
    selection_document = _repo_path(repo_root, qualification.get("selection_document"), "qualification.selection_document")
    _require(selection_document.is_file(), "selection document does not exist")
    evidence = qualification.get("required_evidence")
    _require(isinstance(evidence, list) and len(evidence) >= 8, "qualification.required_evidence is incomplete")

    freeze_gate = data.get("freeze_gate")
    _require(isinstance(freeze_gate, dict), "freeze_gate must be an object")
    _require(freeze_gate.get("scored_suite_allowed") is True, "scored suite may open only after selected qualification evidence")


def validate_verifier_registry(data: dict[str, Any]) -> None:
    _require(data.get("schema_version") == 1, "verifier registry schema_version must be 1")
    _require(data.get("benchmark_id") == "trace2d-b1", "verifier registry benchmark_id mismatch")
    _require(data.get("state") == "frozen", "verifier registry must remain frozen")
    _require(data.get("qualification_evidence") == "benchmarks/b1/fixture-qualification.json", "verifier qualification evidence path changed")
    entries = data.get("verifiers")
    _require(isinstance(entries, list), "verifiers must be an array")
    ids = tuple(entry.get("id") for entry in entries if isinstance(entry, dict))
    _require(ids == EXPECTED_VERIFIERS, "verifier membership/order changed after freeze")
    _require(len(set(ids)) == len(ids), "verifier ids must be unique")
    for entry in entries:
        _require(isinstance(entry, dict), "verifier entry must be an object")
        _require(entry.get("engine") in ("godot", "trace2d"), f"invalid verifier engine for {entry.get('id')}")
        _require(entry.get("qualification_required") is True, f"{entry.get('id')} must require independent fixture qualification")
        checks = entry.get("deterministic_checks")
        _require(isinstance(checks, list) and checks, f"{entry.get('id')} must freeze deterministic checks")
        dispatch = entry.get("dispatch")
        _require(isinstance(dispatch, dict), f"{entry.get('id')} must freeze dispatch metadata")
        _require(isinstance(dispatch.get("kind"), str) and dispatch["kind"], f"{entry.get('id')} dispatch.kind is required")


def validate_freeze_manifest(data: dict[str, Any], repo_root: Path) -> None:
    _require(data.get("schema_version") == 1, "freeze manifest schema_version must be 1")
    _require(data.get("benchmark_id") == "trace2d-b1", "freeze manifest benchmark_id mismatch")
    _require(data.get("state") == "frozen", "freeze manifest state must remain frozen")
    _require(data.get("algorithm") == "sha256", "freeze manifest algorithm must be sha256")
    entries = data.get("files")
    _require(isinstance(entries, list) and entries, "freeze manifest files must be a non-empty array")

    core = [
        "benchmarks/b0/agent-profile.codex-0.144.6.json",
        "benchmarks/b0/suite.json",
        "benchmarks/b1/baseline-qualification.json",
        "benchmarks/b1/suite.json",
        "benchmarks/b1/verifiers.json",
    ]
    task_root = repo_root / "benchmarks/b1/tasks"
    task_files = sorted(
        path.relative_to(repo_root).as_posix()
        for path in task_root.rglob("*")
        if path.is_file()
    )
    expected_paths = core + task_files
    observed_paths = [entry.get("path") for entry in entries if isinstance(entry, dict)]
    _require(observed_paths == expected_paths, "freeze manifest file membership/order changed or omitted a frozen task file")

    for entry in entries:
        _require(isinstance(entry, dict), "freeze manifest entry must be an object")
        path = _repo_path(repo_root, entry.get("path"), "freeze_manifest.files.path")
        _require(path.is_file(), f"frozen file missing: {entry.get('path')}")
        digest = entry.get("sha256")
        _require(isinstance(digest, str) and len(digest) == 64, f"invalid sha256 for {entry.get('path')}")
        observed = hashlib.sha256(path.read_bytes()).hexdigest()
        _require(observed == digest, f"frozen file digest changed: {entry.get('path')}")


def _load_agent_budget(repo_root: Path, path_value: Any) -> dict[str, Any]:
    path = _repo_path(repo_root, path_value, "frozen_source.agent_profile")
    _require(path.is_file(), f"agent profile missing: {path_value}")
    profile = _load_json(path)
    budget = profile.get("budget")
    _require(isinstance(budget, dict), "agent profile budget must be an object")
    return copy.deepcopy(budget)


def validate_suite(data: dict[str, Any], baseline: dict[str, Any], verifiers: dict[str, Any], repo_root: Path) -> None:
    validate_contract(baseline, repo_root)
    validate_verifier_registry(verifiers)

    _require(data.get("schema_version") == 1, "suite.schema_version must be 1")
    _require(data.get("suite_id") == "trace2d-b1", "suite_id must be trace2d-b1")
    _require(data.get("state") == "frozen", "B1 suite state must remain frozen")

    frozen = data.get("frozen_source")
    _require(isinstance(frozen, dict), "frozen_source must be an object")
    _require(frozen.get("trace2d_commit") == FROZEN_TRACE2D_COMMIT, "Trace2D production source moved after B1 task freeze")
    _require(frozen.get("godot_version") == "4.7.1-stable", "Godot version must remain 4.7.1-stable")
    selected = frozen.get("godot_agent")
    _require(isinstance(selected, dict), "frozen_source.godot_agent must be an object")
    _require(selected.get("id") == SELECTED_ID and selected.get("pin") == SELECTED_PIN, "frozen Godot Agent identity/pin changed")
    baseline_path = _repo_path(repo_root, frozen.get("baseline_contract"), "frozen_source.baseline_contract")
    _require(baseline_path.is_file(), "frozen baseline contract path does not exist")
    budget = _load_agent_budget(repo_root, frozen.get("agent_profile"))

    methodology = data.get("methodology")
    _require(isinstance(methodology, dict), "methodology must be an object")
    inherited_suite = _repo_path(repo_root, methodology.get("inherits_suite"), "methodology.inherits_suite")
    _require(inherited_suite.is_file(), "B0 methodology suite does not exist")
    for key in ("trial_isolation_reused", "append_only_trace_reused", "retry_exclusion_policy_reused", "raw_metric_vocabulary_reused"):
        _require(methodology.get(key) is True, f"methodology.{key} must be true")

    scoring_gate = data.get("scoring_gate")
    _require(isinstance(scoring_gate, dict), "scoring_gate must be an object")
    _require(scoring_gate.get("requires_fixture_qualification") is True, "fixture qualification must gate scoring")
    _require(scoring_gate.get("qualification_evidence") == "benchmarks/b1/fixture-qualification.json", "fixture qualification evidence path changed")
    _require(scoring_gate.get("suite_mutation_after_freeze_forbidden") is True, "suite must remain immutable after freeze")
    _require(data.get("freeze_manifest") == "benchmarks/b1/freeze-manifest.json", "freeze manifest path changed")

    lanes = data.get("lanes")
    _require(isinstance(lanes, list), "lanes must be an array")
    lane_ids = tuple(lane.get("id") for lane in lanes if isinstance(lane, dict))
    _require(lane_ids == EXPECTED_LANES, "B1 lane membership/order must match B0")
    for lane in lanes:
        qualification = lane.get("qualification", {})
        _require(qualification.get("required") is True, f"{lane.get('id')} must require fixture qualification")
        _require(qualification.get("evidence") == "benchmarks/b1/fixture-qualification.json", f"{lane.get('id')} qualification evidence path changed")
    godot_agent = lanes[1]
    bridge = godot_agent.get("bridge")
    _require(isinstance(bridge, dict), "godot.agent bridge is required")
    _require(bridge.get("id") == SELECTED_ID and bridge.get("pin") == SELECTED_PIN, "godot.agent bridge drifted from selected baseline")

    tasks = data.get("tasks")
    _require(isinstance(tasks, list), "tasks must be an array")
    ids = tuple(task.get("id") for task in tasks if isinstance(task, dict))
    _require(ids == EXPECTED_TASKS, "B1 task membership/order changed after freeze")

    covered: list[str] = []
    registry_ids = {entry["id"] for entry in verifiers["verifiers"]}
    for task in tasks:
        task_id = task.get("id", "<missing>")
        _require(task.get("version") == 1, f"{task_id} version must be 1")
        _require(task.get("state") == "frozen", f"{task_id} must remain frozen")
        task_classes = task.get("task_classes")
        _require(tuple(task_classes or []) == EXPECTED_TASK_CLASS_MAP[task_id], f"{task_id} task-class binding changed after freeze")
        covered.extend(task_classes)
        prompt = _repo_path(repo_root, task.get("prompt"), f"{task_id}.prompt")
        _require(prompt.is_file(), f"{task_id} prompt does not exist")
        _require(task.get("budget") == budget, f"{task_id} budget must exactly match the frozen B0 agent profile")
        _require(task.get("evidence_layers") == list(EVIDENCE_LAYERS), f"{task_id} evidence-layer ordering changed")
        expected = task.get("expected")
        _require(isinstance(expected, dict) and expected, f"{task_id} expected semantic contract is required")

        lane_map = task.get("lanes")
        _require(isinstance(lane_map, dict) and tuple(lane_map.keys()) == EXPECTED_LANES, f"{task_id} must define all three lanes in frozen order")
        generic = lane_map["godot.generic"]
        agent = lane_map["godot.agent"]
        for key in ("starter", "known_good", "known_bad", "verifier"):
            _require(generic.get(key) == agent.get(key), f"{task_id} Godot generic/agent must use identical {key}")

        for lane_id, lane_task in lane_map.items():
            _require(isinstance(lane_task, dict), f"{task_id}/{lane_id} mapping must be an object")
            for key in ("starter", "known_good"):
                path = _repo_path(repo_root, lane_task.get(key), f"{task_id}/{lane_id}.{key}")
                _require(path.is_dir(), f"{task_id}/{lane_id} missing {key} fixture: {path}")
            bad = lane_task.get("known_bad")
            _require(isinstance(bad, list) and bad, f"{task_id}/{lane_id} requires at least one known_bad fixture")
            for value in bad:
                path = _repo_path(repo_root, value, f"{task_id}/{lane_id}.known_bad")
                _require(path.is_dir(), f"{task_id}/{lane_id} missing known_bad fixture: {path}")
            verifier_id = lane_task.get("verifier")
            _require(verifier_id in registry_ids, f"{task_id}/{lane_id} references unknown verifier: {verifier_id}")
            if lane_id.startswith("godot."):
                _require(verifier_id.startswith("godot-"), f"{task_id}/{lane_id} must dispatch to Godot verifier")
            else:
                _require(verifier_id.startswith("trace2d-"), f"{task_id}/{lane_id} must dispatch to Trace2D verifier")

    _require(len(covered) == len(REQUIRED_TASK_CLASSES) and set(covered) == set(REQUIRED_TASK_CLASSES) and len(set(covered)) == len(covered), "task class coverage must contain every preregistered #103 class exactly once")


def load_and_validate_contract(path: Path, repo_root: Path) -> dict[str, Any]:
    data = _load_json(path)
    validate_contract(data, repo_root)
    return data


def load_and_validate_suite(path: Path, repo_root: Path) -> dict[str, Any]:
    baseline = _load_json(repo_root / "benchmarks/b1/baseline-qualification.json")
    verifiers = _load_json(repo_root / "benchmarks/b1/verifiers.json")
    suite = _load_json(path)
    validate_suite(suite, baseline, verifiers, repo_root)
    manifest_path = _repo_path(repo_root, suite.get("freeze_manifest"), "freeze_manifest")
    manifest = _load_json(manifest_path)
    validate_freeze_manifest(manifest, repo_root)
    return suite


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate_contract_parser = subparsers.add_parser("validate-contract")
    validate_contract_parser.add_argument("--contract", default="benchmarks/b1/baseline-qualification.json")
    validate_suite_parser = subparsers.add_parser("validate-suite")
    validate_suite_parser.add_argument("--suite", default="benchmarks/b1/suite.json")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    if args.command == "validate-contract":
        load_and_validate_contract(repo_root / args.contract, repo_root)
        print("Benchmark B1 baseline selection contract: OK")
        return 0
    if args.command == "validate-suite":
        load_and_validate_suite(repo_root / args.suite, repo_root)
        print("Benchmark B1 frozen scored-suite contract: OK")
        return 0
    raise AssertionError("unreachable")


if __name__ == "__main__":
    raise SystemExit(main())
