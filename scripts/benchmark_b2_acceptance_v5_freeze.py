#!/usr/bin/env python3
"""Validate append-only Benchmark B2 acceptance-v5 readiness provenance."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from scripts import benchmark_b2_acceptance_v4_freeze
from scripts import benchmark_b2_agent_outcome

REPO_ROOT = Path(__file__).resolve().parents[1]
V4_CONTRACT_PATH = Path("benchmarks/b2/acceptance/contract-v4.json")
V5_CONTRACT_PATH = Path("benchmarks/b2/acceptance/contract-v5.json")
EXPECTED_V4_GIT_BLOB_SHA = "a35192fd2dca210ab4cf00b127645067fe21b8e4"
EXPECTED_ROOT = "benchmark-b2-acceptance-v5"
EXPECTED_HISTORICAL_ROOTS = [
    "benchmark-b2-acceptance-v1",
    "benchmark-b2-acceptance-v2",
    "benchmark-b2-acceptance-v3",
    "benchmark-b2-acceptance-v4",
]
EXPECTED_SCORED_ROOT = "benchmark-b2-scored-v1"
EXPECTED_READINESS_COMMIT = "3f20c2e39ccfc3a55048c005ee4c7c98c4f501c9"
EXPECTED_READINESS_RUN = 32105655474
EXPECTED_READINESS_WORKFLOW_BLOB = "331e188c23d7647ddcb278064d7be73dca2fe791"
EXPECTED_B2_WRAPPER_BLOB = "68031349f4313cd998353dd9204cd227e5486b02"
EXPECTED_OUTCOME_BLOB = "b586c19f40ad139aba79cc43c184eaeab1f0564e"
COPY_EXACT_KEYS = (
    "task_id",
    "task_prompt",
    "task_prompt_sha256",
    "lane",
    "initial_runs",
    "deterministic_contract_task_id",
    "deterministic_verifier",
    "selection_policy",
    "remediation",
    "presentation_gate",
    "perceptual_review",
    "feedback_policy",
    "budget",
)


class AcceptanceV5FreezeError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AcceptanceV5FreezeError(message)


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AcceptanceV5FreezeError(f"failed to read JSON {path}: {exc}") from exc
    require(isinstance(value, dict), f"expected JSON object: {path}")
    return value


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_blob_sha(path: Path) -> str:
    payload = path.read_bytes()
    header = f"blob {len(payload)}\0".encode("ascii")
    return hashlib.sha1(header + payload).hexdigest()


def validate_contract_data(v5: dict[str, Any], v4: dict[str, Any], repo_root: Path = REPO_ROOT) -> None:
    require(v5.get("schema_version") == 5, "acceptance-v5 schema_version must be 5")
    require(v5.get("kind") == "trace2d_b2_nonscored_acceptance_contract", "acceptance-v5 kind drifted")
    require(v5.get("benchmark_id") == "trace2d-b2" and v5.get("issue") == 104, "acceptance-v5 identity drifted")
    require(v5.get("state") == "frozen_pre_acceptance" and v5.get("scored") is False, "acceptance-v5 must stay frozen and non-scored")
    require(v5.get("acceptance_version") == 5 and v5.get("supersedes_acceptance_version") == 4, "acceptance-v5 version chain drifted")
    require(
        v5.get("does_not_reinterpret")
        == [
            "benchmark-b2-scored-v1",
            "benchmark-b2-acceptance-v1",
            "benchmark-b2-acceptance-v2",
            "benchmark-b2-acceptance-v3",
            "benchmark-b2-acceptance-v4",
        ],
        "acceptance-v5 historical evidence boundary drifted",
    )

    for key in COPY_EXACT_KEYS:
        require(v5.get(key) == v4.get(key), f"acceptance-v5 relaxed or changed v4 field: {key}")

    prompt = repo_root / str(v5["task_prompt"])
    require(prompt.is_file(), "acceptance-v5 reused task prompt is missing")
    require(sha256_file(prompt) == v5["task_prompt_sha256"], "acceptance-v5 task prompt bytes drifted")

    held_out = v5.get("held_out_policy", {})
    for key in (
        "post_score_validation_only",
        "acceptance_v4_candidate_output_observed_before_v5_freeze",
        "no_v5_candidate_output_observed_before_freeze",
        "v4_failure_was_upstream_agent_transport_not_candidate_gameplay",
        "task_prompt_reused_byte_identical_from_v4",
        "presentation_thresholds_reused_unchanged_from_v4",
        "budget_reused_unchanged_from_v4",
        "scored_and_acceptance_v1_through_v4_evidence_remain_historical_only",
    ):
        require(held_out.get(key) is True, f"acceptance-v5 held-out guard disabled: {key}")

    qualification = v5.get("qualification_policy", {})
    require(qualification.get("v5_candidate_output_observed") is False, "v5 candidate output must not exist before freeze")
    for key in (
        "reuse_v4_synthetic_presentation_gate_qualification",
        "deterministic_verifier_unchanged_from_v4",
        "task_threshold_budget_rubric_must_match_v4",
        "readiness_provenance_must_lock_before_first_v5_execution",
        "agent_transport_failure_must_precede_deterministic_verifier_classification",
    ):
        require(qualification.get(key) is True, f"acceptance-v5 qualification guard disabled: {key}")

    readiness = v5.get("agent_readiness", {})
    require(readiness.get("source_pull_requests") == [304, 305, 306], "acceptance-v5 readiness PR chain drifted")
    require(readiness.get("required_main_commit") == EXPECTED_READINESS_COMMIT, "acceptance-v5 readiness commit drifted")
    require(readiness.get("workflow_run_id") == EXPECTED_READINESS_RUN, "acceptance-v5 readiness run drifted")
    require(readiness.get("codex_cli_version") == "0.144.6", "acceptance-v5 Codex version drifted")
    require(readiness.get("model_id") == "gpt-5.5" and readiness.get("reasoning_effort") == "high", "acceptance-v5 model identity drifted")
    require(readiness.get("permission_profile") == ":workspace+windows_ntfs_acl_v1_elevated", "acceptance-v5 isolation profile drifted")
    for key in (
        "candidate_free",
        "passed",
        "workspace_write_proved",
        "external_read_attempt_observed",
        "external_read_denied",
        "turn_completed",
    ):
        require(readiness.get(key) is True, f"acceptance-v5 readiness proof disabled: {key}")
    for key in ("benchmark_task_prompt_used", "acceptance_candidate_created", "canary_secret_leaked"):
        require(readiness.get(key) is False, f"acceptance-v5 readiness safety guard violated: {key}")
    require(readiness.get("codex_return_code") == 0, "acceptance-v5 readiness Codex return code drifted")
    require(readiness.get("readiness_workflow_git_blob_sha") == EXPECTED_READINESS_WORKFLOW_BLOB, "readiness workflow provenance drifted")
    require(readiness.get("b2_wrapper_git_blob_sha") == EXPECTED_B2_WRAPPER_BLOB, "B2 wrapper provenance drifted")
    require(readiness.get("agent_outcome_classifier_git_blob_sha") == EXPECTED_OUTCOME_BLOB, "Agent outcome classifier provenance drifted")

    readiness_workflow = repo_root / ".github/workflows/benchmark-b2-owner-agent-readiness.yml"
    wrapper = repo_root / "scripts/benchmark_b2_codex_windows_acl_wrapper.py"
    classifier = repo_root / "scripts/benchmark_b2_agent_outcome.py"
    require(git_blob_sha(readiness_workflow) == EXPECTED_READINESS_WORKFLOW_BLOB, "readiness workflow bytes changed after proven run")
    require(git_blob_sha(wrapper) == EXPECTED_B2_WRAPPER_BLOB, "B2 wrapper bytes changed after proven run")
    require(git_blob_sha(classifier) == EXPECTED_OUTCOME_BLOB, "Agent outcome classifier bytes changed after proven run")

    workflow_text = readiness_workflow.read_text(encoding="utf-8")
    for required in (
        "$protectedRoot = Join-Path $root 'protected'",
        "$canary = Join-Path $protectedRoot 'outside-workspace-canary.txt'",
        "$env:TRACE2D_BENCH_ACL_EXTRA_PROTECTED_ROOTS = $protectedRoot",
        "external_read_denied",
        "canary_secret_leaked",
    ):
        require(required in workflow_text, f"readiness fail-closed workflow guard missing: {required}")

    wrapper_text = wrapper.read_text(encoding="utf-8")
    require("def _probe_tool_roots(" in wrapper_text, "candidate-free probe resolver missing")
    require('configure_b2(probe_only=args.command == "probe-isolation")' in wrapper_text, "probe-only dispatch guard missing")

    classification = benchmark_b2_agent_outcome.classify_agent_execution(
        agent_result={
            "status": "tool_transport_failure",
            "wrapper": {"process_return_code": 1, "turn_completed": False, "budget_ok": True},
        },
        agent_identity_ok=True,
    )
    require(classification.get("status") == benchmark_b2_agent_outcome.TRANSPORT_FAILURE, "transport failure classification drifted")
    require(classification.get("deterministic_verifier_authoritative") is False, "transport failure must precede deterministic verifier")

    isolation = v5.get("isolation", {})
    require(isolation.get("durable_root_name") == EXPECTED_ROOT, "acceptance-v5 durable root drifted")
    require(isolation.get("historical_acceptance_root_names") == EXPECTED_HISTORICAL_ROOTS, "acceptance-v5 historical root guard drifted")
    require(isolation.get("scored_durable_root_name") == EXPECTED_SCORED_ROOT, "acceptance-v5 scored root guard drifted")
    for key in (
        "scored_record_write_forbidden",
        "scored_raw_jsonl_forbidden",
        "historical_acceptance_write_forbidden",
        "repository_worktree_durable_state_forbidden",
    ):
        require(isolation.get(key) is True, f"acceptance-v5 isolation guard disabled: {key}")

    acceptance = v5.get("acceptance", {})
    for key in (
        "scored_cohort_must_remain_immutable",
        "acceptance_v1_must_remain_immutable",
        "acceptance_v2_must_remain_immutable",
        "acceptance_v3_must_remain_immutable",
        "acceptance_v4_must_remain_immutable",
    ):
        require(acceptance.get(key) is True, f"acceptance-v5 historical immutability guard disabled: {key}")


def validate_repository(repo_root: Path = REPO_ROOT) -> None:
    benchmark_b2_acceptance_v4_freeze.validate_repository(repo_root)
    v4_path = repo_root / V4_CONTRACT_PATH
    require(git_blob_sha(v4_path) == EXPECTED_V4_GIT_BLOB_SHA, "consumed acceptance-v4 contract bytes changed")
    v4 = load_json(v4_path)
    v5 = load_json(repo_root / V5_CONTRACT_PATH)
    validate_contract_data(v5, v4, repo_root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    args = parser.parse_args()
    try:
        validate_repository(args.repo_root.resolve())
    except Exception as exc:
        print(f"B2 acceptance-v5 freeze invalid: {exc}")
        return 1
    print("B2 acceptance-v5 is frozen append-only with v4 task/gates/budget unchanged and readiness provenance locked.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
