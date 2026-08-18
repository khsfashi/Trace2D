#!/usr/bin/env python3
"""Validate the append-only Benchmark B2 acceptance-v4 freeze.

V4 is a post-score, non-scored successor to consumed acceptance-v3. It may test
PR #299's general public C++ discovery remediation, but it must not relax or
reinterpret the immutable scored, v1, v2, or v3 evidence layers.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from scripts import benchmark_b2_acceptance_v3_freeze
from scripts import benchmark_b2_postscore_remediation as remediation

REPO_ROOT = Path(__file__).resolve().parents[1]
V3_CONTRACT_PATH = Path("benchmarks/b2/acceptance/contract-v3.json")
V4_CONTRACT_PATH = Path("benchmarks/b2/acceptance/contract-v4.json")
EXPECTED_V3_GIT_BLOB_SHA = "df364ee58ac4194e8eb989a1c42aaec5769f3218"
EXPECTED_REMEDIATION_COMMIT = "4095bdaf118594f8dd5af1445a5e16fbeaf5a13e"
EXPECTED_REMEDIATION_PR = 299
EXPECTED_ROOT = "benchmark-b2-acceptance-v4"
EXPECTED_HISTORICAL_ROOTS = [
    "benchmark-b2-acceptance-v1",
    "benchmark-b2-acceptance-v2",
    "benchmark-b2-acceptance-v3",
]
EXPECTED_SCORED_ROOT = "benchmark-b2-scored-v1"
EXPECTED_PUBLIC_SYMBOLS = [
    "trace2d::application::Game",
    "trace2d::application::Game::OnFixedUpdate",
    "trace2d::scene::ComponentRegistry",
    "trace2d::application::Application::SetPresentationCallback",
    "trace2d::platform::Platform",
    "trace2d::render::Renderer",
    "trace2d::render::OrthographicCamera",
    "trace2d::render::SpriteRenderData",
]
COPY_EXACT_KEYS = (
    "task_id",
    "task_prompt",
    "task_prompt_sha256",
    "lane",
    "initial_runs",
    "deterministic_contract_task_id",
    "deterministic_verifier",
    "selection_policy",
    "presentation_gate",
    "perceptual_review",
    "feedback_policy",
    "budget",
)


class AcceptanceV4FreezeError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AcceptanceV4FreezeError(message)


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AcceptanceV4FreezeError(f"failed to read JSON {path}: {exc}") from exc
    require(isinstance(value, dict), f"expected JSON object: {path}")
    return value


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_blob_sha(path: Path) -> str:
    payload = path.read_bytes()
    header = f"blob {len(payload)}\0".encode("ascii")
    return hashlib.sha1(header + payload).hexdigest()


def validate_contract_data(v4: dict[str, Any], v3: dict[str, Any], repo_root: Path = REPO_ROOT) -> None:
    require(v4.get("schema_version") == 4, "acceptance-v4 schema_version must be 4")
    require(v4.get("kind") == "trace2d_b2_nonscored_acceptance_contract", "acceptance-v4 kind drifted")
    require(v4.get("benchmark_id") == "trace2d-b2" and v4.get("issue") == 104, "acceptance-v4 identity drifted")
    require(
        v4.get("state") == "frozen_pre_acceptance" and v4.get("scored") is False,
        "acceptance-v4 must stay frozen and non-scored",
    )
    require(
        v4.get("acceptance_version") == 4 and v4.get("supersedes_acceptance_version") == 3,
        "acceptance-v4 version chain drifted",
    )
    require(
        v4.get("does_not_reinterpret")
        == [
            "benchmark-b2-scored-v1",
            "benchmark-b2-acceptance-v1",
            "benchmark-b2-acceptance-v2",
            "benchmark-b2-acceptance-v3",
        ],
        "acceptance-v4 historical evidence boundary drifted",
    )

    for key in COPY_EXACT_KEYS:
        require(v4.get(key) == v3.get(key), f"acceptance-v4 relaxed or changed v3 field: {key}")

    prompt = repo_root / str(v4["task_prompt"])
    require(prompt.is_file(), "acceptance-v4 reused task prompt is missing")
    require(sha256_file(prompt) == v4["task_prompt_sha256"], "acceptance-v4 task prompt bytes drifted")

    held_out = v4.get("held_out_policy", {})
    for key in (
        "post_score_validation_only",
        "acceptance_v3_candidate_output_observed_before_v4_freeze",
        "no_v4_candidate_output_observed_before_freeze",
        "remediation_derived_from_acceptance_v3_failure_mode",
        "task_prompt_reused_byte_identical_from_v3",
        "presentation_thresholds_reused_unchanged_from_v3",
        "budget_reused_unchanged_from_v3",
        "acceptance_v1_v2_and_v3_evidence_remain_historical_only",
    ):
        require(held_out.get(key) is True, f"acceptance-v4 held-out guard disabled: {key}")

    qualification = v4.get("qualification_policy", {})
    require(
        qualification.get("v4_candidate_output_observed") is False,
        "v4 candidate output must not be observed before freeze",
    )
    for key in (
        "reuse_v3_synthetic_presentation_gate_qualification",
        "deterministic_verifier_unchanged_from_v3",
        "task_threshold_budget_rubric_must_match_v3",
        "remediation_commit_must_lock_before_first_v4_execution",
    ):
        require(qualification.get(key) is True, f"acceptance-v4 qualification guard disabled: {key}")

    repair = v4.get("remediation", {})
    require(repair.get("source_pull_request") == EXPECTED_REMEDIATION_PR, "acceptance-v4 remediation PR drifted")
    require(
        repair.get("required_main_commit") == EXPECTED_REMEDIATION_COMMIT,
        "acceptance-v4 remediation commit drifted",
    )
    require(repair.get("bridge_handoff_file") == "B2Candidate.cpp", "acceptance-v4 bridge filename drifted")
    require(
        repair.get("required_public_api_symbols") == EXPECTED_PUBLIC_SYMBOLS,
        "acceptance-v4 public API discovery surface drifted",
    )
    for key in (
        "gameplay_solution_injected",
        "presentation_solution_injected",
        "task_relaxed",
        "deterministic_verifier_relaxed",
        "presentation_gate_relaxed",
        "budget_relaxed",
    ):
        require(repair.get(key) is False, f"acceptance-v4 remediation must not relax/inject: {key}")

    for qualified_name in EXPECTED_PUBLIC_SYMBOLS:
        symbol = remediation.public_api_symbol(qualified_name, repo_root)
        source = repo_root / str(symbol.get("source", ""))
        example = repo_root / str(symbol.get("canonical_example", ""))
        require(source.is_file(), f"indexed public API source missing: {qualified_name}")
        require(example.is_file(), f"indexed public API example missing: {qualified_name}")

    game_update = remediation.public_api_symbol("trace2d::application::Game::OnFixedUpdate", repo_root)
    require(
        game_update.get("include") == "trace2d/application/Application.hpp",
        "Game::OnFixedUpdate canonical include drifted",
    )
    game_example = (repo_root / str(game_update["canonical_example"])).read_text(encoding="utf-8")
    require("void OnFixedUpdate(" in game_example, "canonical Game example no longer declares OnFixedUpdate")
    require(
        "const trace2d::application::FixedUpdate& update) override" in game_example,
        "canonical Game example no longer overrides the required fixed-step callback",
    )

    registry = remediation.public_api_symbol("trace2d::scene::ComponentRegistry", repo_root)
    require(registry.get("include") == "trace2d/scene/Components.hpp", "ComponentRegistry canonical include drifted")
    registry_example = (repo_root / str(registry["canonical_example"])).read_text(encoding="utf-8")
    require(
        "#include <trace2d/scene/Components.hpp>" in registry_example,
        "canonical ComponentRegistry example relies on a transitive include",
    )
    require(
        "trace2d/scene/ComponentRegistry.hpp" not in registry_example,
        "canonical ComponentRegistry example invented a per-type header",
    )

    wrapper = (repo_root / "scripts/benchmark_b2_codex_windows_acl_wrapper.py").read_text(encoding="utf-8")
    require("workspace-root file `B2Candidate.cpp`" in wrapper, "Trace2D B2 bridge filename is not exposed to the Agent")
    require("trace2d::benchmark::b2::CreateCandidate" in wrapper, "Trace2D B2 bridge factory handoff is missing")
    require("execution plumbing only" in wrapper, "Trace2D B2 bridge must remain mechanical-only")

    isolation = v4.get("isolation", {})
    require(isolation.get("durable_root_name") == EXPECTED_ROOT, "acceptance-v4 durable root drifted")
    require(
        isolation.get("historical_acceptance_root_names") == EXPECTED_HISTORICAL_ROOTS,
        "acceptance-v4 historical root guard drifted",
    )
    require(isolation.get("scored_durable_root_name") == EXPECTED_SCORED_ROOT, "acceptance-v4 scored root guard drifted")
    for key in (
        "scored_record_write_forbidden",
        "scored_raw_jsonl_forbidden",
        "historical_acceptance_write_forbidden",
        "repository_worktree_durable_state_forbidden",
    ):
        require(isolation.get(key) is True, f"acceptance-v4 isolation guard disabled: {key}")

    acceptance = v4.get("acceptance", {})
    require(acceptance.get("scored_cohort_must_remain_immutable") is True, "scored immutability guard disabled")
    require(acceptance.get("acceptance_v1_must_remain_immutable") is True, "acceptance-v1 immutability guard disabled")
    require(acceptance.get("acceptance_v2_must_remain_immutable") is True, "acceptance-v2 immutability guard disabled")
    require(acceptance.get("acceptance_v3_must_remain_immutable") is True, "acceptance-v3 immutability guard disabled")


def validate_repository(repo_root: Path = REPO_ROOT) -> None:
    benchmark_b2_acceptance_v3_freeze.validate_repository(repo_root)
    v3_path = repo_root / V3_CONTRACT_PATH
    require(git_blob_sha(v3_path) == EXPECTED_V3_GIT_BLOB_SHA, "consumed acceptance-v3 contract bytes changed")
    v3 = load_json(v3_path)
    v4 = load_json(repo_root / V4_CONTRACT_PATH)
    validate_contract_data(v4, v3, repo_root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    args = parser.parse_args()
    try:
        validate_repository(args.repo_root.resolve())
    except Exception as exc:
        print(f"B2 acceptance-v4 freeze invalid: {exc}")
        return 1
    print("B2 acceptance-v4 is frozen append-only with v3 task/gates/budget unchanged.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
