#!/usr/bin/env python3
"""Non-scored post-B2 acceptance loop for Trace2D issue #104.

This harness is intentionally separate from the immutable nine-slot B2 scored
cohort. It freezes a new held-out presentation variant before execution, runs a
small matched Trace2D-only initial cohort, records an advisory perceptual review,
then applies exactly one real human-feedback revision to the first
deterministically accepted candidate and re-verifies it.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
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
import benchmark_b0_stable_harness  # noqa: E402
import benchmark_b2_execution_freeze  # noqa: E402
import benchmark_b2_scored_harness as scored  # noqa: E402

CONTRACT_PATH = Path("benchmarks/b2/acceptance/contract-v1.json")
TASK_PROMPT_PATH = Path("benchmarks/b2/acceptance/tasks/ember-hall-v1/PROMPT.md")
EXECUTION_PATH = Path("benchmarks/b2/execution-v1.json")
PROFILE_PATH = Path("benchmarks/b0/agent-profile.codex-0.144.6.json")
LANE = "trace2d.agent"
SCORING_TASK_ID = "b2-topdown-combat-v1"
ACCEPTANCE_TASK_ID = "b2-acceptance-ember-hall-v1"
PRESENTATION_ROOT = Path(".trace2d-b2-evidence/presentation")
FORBIDDEN_SCORED_ROOT_TOKEN = "benchmark-b2-scored-v1"
INITIAL_SUMMARY = "initial-summary.json"
PERCEPTUAL_REVIEW = "perceptual-review.json"
REVISION_RECORD = "revision-record.json"


class AcceptanceError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AcceptanceError(f"failed to read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise AcceptanceError(f"expected JSON object: {path}")
    return value


def sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def sha256_file(path: Path) -> str:
    return benchmark_b0.sha256_file(path)


def write_json(path: Path, value: dict[str, Any]) -> None:
    scored.write_json_atomic(path, value)


def validate_contract() -> dict[str, Any]:
    contract = load_json(REPO_ROOT / CONTRACT_PATH)
    if contract.get("kind") != "trace2d_b2_nonscored_acceptance_contract":
        raise AcceptanceError("unexpected acceptance contract kind")
    if contract.get("state") != "frozen_pre_acceptance":
        raise AcceptanceError("acceptance contract is not frozen_pre_acceptance")
    if contract.get("scored") is not False:
        raise AcceptanceError("acceptance contract must be explicitly non-scored")
    if contract.get("lane") != LANE:
        raise AcceptanceError("acceptance lane must remain trace2d.agent")
    if int(contract.get("initial_runs", 0)) != 2:
        raise AcceptanceError("acceptance matched initial cohort must contain exactly two runs")
    if contract.get("task_id") != ACCEPTANCE_TASK_ID:
        raise AcceptanceError("acceptance task id drifted")
    if contract.get("deterministic_contract_task_id") != SCORING_TASK_ID:
        raise AcceptanceError("deterministic verifier contract id drifted")
    prompt_path = REPO_ROOT / TASK_PROMPT_PATH
    if contract.get("task_prompt") != TASK_PROMPT_PATH.as_posix():
        raise AcceptanceError("acceptance prompt path drifted")
    if sha256_file(prompt_path) != contract.get("task_prompt_sha256"):
        raise AcceptanceError("acceptance prompt SHA-256 drifted")
    policy = contract.get("isolation", {})
    if policy.get("scored_record_write_forbidden") is not True:
        raise AcceptanceError("scored record write guard must stay enabled")
    if policy.get("durable_root_name") != "benchmark-b2-acceptance-v1":
        raise AcceptanceError("acceptance durable root identity drifted")
    if policy.get("scored_durable_root_name") != FORBIDDEN_SCORED_ROOT_TOKEN:
        raise AcceptanceError("scored durable root guard identity drifted")
    benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    return contract


def require_acceptance_root(path_text: str) -> Path:
    root = Path(path_text).expanduser().resolve()
    lowered = str(root).casefold()
    if FORBIDDEN_SCORED_ROOT_TOKEN.casefold() in lowered:
        raise AcceptanceError("refusing to use the immutable B2 scored durable root")
    try:
        root.relative_to(REPO_ROOT.resolve())
    except ValueError:
        pass
    else:
        raise AcceptanceError("acceptance durable state must live outside the repository")
    if (root / "raw.jsonl").exists():
        raise AcceptanceError("refusing a root containing scored-style raw.jsonl")
    return root


def load_profile() -> dict[str, Any]:
    return load_json(REPO_ROOT / PROFILE_PATH)


def agent_identity_ok(result: dict[str, Any] | None, profile: dict[str, Any]) -> bool:
    if not isinstance(result, dict):
        return False
    identity = result.get("model")
    if not isinstance(identity, dict):
        return False
    return (
        identity.get("agent_id") == profile.get("agent_id")
        and identity.get("model_id") == profile.get("model_id")
        and identity.get("model_revision") == profile.get("model_revision")
    )


def budget_summary(agent_result: dict[str, Any] | None, contract: dict[str, Any]) -> dict[str, Any]:
    limits = contract["budget"]
    metrics = agent_result.get("metrics", {}) if isinstance(agent_result, dict) else {}
    observed = {
        "tool_calls": int(metrics.get("tool_calls", 0)),
        "input_tokens": int(metrics.get("input_tokens", 0)),
        "output_tokens": int(metrics.get("output_tokens", 0)),
    }
    return {
        "limits": limits,
        "observed": observed,
        "exceeded": {
            "tool_calls": observed["tool_calls"] > int(limits["max_tool_calls"]),
            "input_tokens": observed["input_tokens"] > int(limits["max_input_tokens"]),
            "output_tokens": observed["output_tokens"] > int(limits["max_output_tokens"]),
        },
        "pass_fail_authority": False,
        "note": "Acceptance records complexity pressure; deterministic verifier remains gameplay authority.",
    }


def metrics_from(agent_result: dict[str, Any] | None, process: dict[str, Any], verifier_process: dict[str, Any]) -> dict[str, Any]:
    metrics = agent_result.get("metrics", {}) if isinstance(agent_result, dict) else {}
    return {
        "revisions": int(metrics.get("revisions", 0)),
        "tool_calls": int(metrics.get("tool_calls", 0)),
        "input_tokens": int(metrics.get("input_tokens", 0)),
        "cached_input_tokens": int(metrics.get("cached_input_tokens", 0)),
        "output_tokens": int(metrics.get("output_tokens", 0)),
        "reasoning_output_tokens": int(metrics.get("reasoning_output_tokens", 0)),
        "visual_feedback_calls": int(metrics.get("visual_feedback_calls", 0)),
        "wrapper_human_interventions": int((agent_result or {}).get("human_interventions", 0)),
        "wall_ms": float(process.get("duration_ms", 0.0)),
        "verifier_ms": float(verifier_process.get("duration_ms", 0.0)),
        "normalized_operations": metrics.get("normalized_operations", {}),
        "engine_native_operations": metrics.get("engine_native_operations", {}),
    }


def load_optional_result(path: Path) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    try:
        return load_json(path)
    except AcceptanceError:
        return None


def deterministic_pass(verifier: dict[str, Any] | None) -> bool:
    return isinstance(verifier, dict) and verifier.get("verdict", {}).get("status") == "pass"


def initial_status(process: dict[str, Any], identity_ok: bool, verifier: dict[str, Any] | None, captures: list[dict[str, Any]]) -> str:
    if process.get("timed_out"):
        return "agent_timeout"
    if not identity_ok:
        return "agent_identity_or_result_failure"
    if not deterministic_pass(verifier):
        return "deterministic_failure"
    if not captures:
        return "presentation_missing"
    return "accepted_for_review"


def run_agent(*, workspace: Path, prompt: Path, result_path: Path, contract: dict[str, Any], task_env_id: str) -> dict[str, Any]:
    budget = contract["budget"]
    return scored.run_process(
        scored.agent_command(workspace, prompt, LANE, result_path),
        cwd=workspace,
        timeout=float(budget["wall_seconds"]),
        env={
            "TRACE2D_BENCH_SUITE_ID": "trace2d-b2-acceptance",
            "TRACE2D_BENCH_LANE": LANE,
            "TRACE2D_BENCH_TASK": task_env_id,
            "TRACE2D_BENCH_WORKSPACE": str(workspace),
            "TRACE2D_BENCH_PROMPT_FILE": str(prompt),
            "TRACE2D_BENCH_AGENT_RESULT": str(result_path),
            "TRACE2D_BENCH_PRESENTATION_ROOT": str(PRESENTATION_ROOT),
            "TRACE2D_BENCH_MAX_TOOL_CALLS": str(budget["max_tool_calls"]),
            "TRACE2D_BENCH_MAX_INPUT_TOKENS": str(budget["max_input_tokens"]),
            "TRACE2D_BENCH_MAX_OUTPUT_TOKENS": str(budget["max_output_tokens"]),
            "TRACE2D_BENCH_WRAPPER_TIMEOUT": str(max(1, int(budget["wall_seconds"]) - 15)),
        },
    )


def run_verifier(workspace: Path, result_path: Path, build_slot: int) -> tuple[dict[str, Any], dict[str, Any] | None]:
    process = scored.run_process(
        scored.verifier_command(workspace, LANE, result_path, build_slot),
        cwd=REPO_ROOT,
        timeout=300.0,
    )
    return process, load_optional_result(result_path)


def initial_record(*, root: Path, run_index: int, contract: dict[str, Any], execution: dict[str, Any], profile: dict[str, Any]) -> dict[str, Any]:
    trial_id = f"accept-initial-{run_index:02d}-trace2d-agent"
    trial_root = root / "initial" / trial_id
    if trial_root.exists():
        raise AcceptanceError(f"initial acceptance trial already exists; no rerun allowed: {trial_id}")
    trial_root.mkdir(parents=True)

    workspace = trial_root / "workspace"
    starter = REPO_ROOT / execution["lane_starters"][LANE]["root"]
    shutil.copytree(starter, workspace)
    scored.verify_starter(workspace, LANE, execution)

    prompt = trial_root / "frozen-acceptance-prompt.md"
    shutil.copy2(REPO_ROOT / TASK_PROMPT_PATH, prompt)
    if sha256_file(prompt) != contract["task_prompt_sha256"]:
        raise AcceptanceError("copied acceptance task prompt drifted")

    benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    agent_result_path = trial_root / "agent-result.json"
    verifier_result_path = trial_root / "verifier-result.json"
    started_at = utc_now()
    process = run_agent(workspace=workspace, prompt=prompt, result_path=agent_result_path, contract=contract, task_env_id=ACCEPTANCE_TASK_ID)
    (trial_root / "adapter.stdout.txt").write_text(str(process.get("stdout", "")), encoding="utf-8")
    (trial_root / "adapter.stderr.txt").write_text(str(process.get("stderr", "")), encoding="utf-8")
    agent_result = load_optional_result(agent_result_path)
    identity_ok = agent_identity_ok(agent_result, profile)

    freeze_valid = True
    try:
        benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    except Exception:
        freeze_valid = False

    verifier_process, verifier = run_verifier(workspace, verifier_result_path, 100 + run_index)
    (trial_root / "verifier.stdout.txt").write_text(str(verifier_process.get("stdout", "")), encoding="utf-8")
    (trial_root / "verifier.stderr.txt").write_text(str(verifier_process.get("stderr", "")), encoding="utf-8")
    captures = scored.presentation_files(workspace)
    status = initial_status(process, identity_ok, verifier, captures)
    record = {
        "schema_version": 1,
        "kind": "trace2d_b2_nonscored_acceptance_initial",
        "benchmark_id": "trace2d-b2",
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "scored": False,
        "phase": "initial",
        "run_index": run_index,
        "trial_id": trial_id,
        "lane_id": LANE,
        "task_id": ACCEPTANCE_TASK_ID,
        "deterministic_contract_task_id": SCORING_TASK_ID,
        "started_at": started_at,
        "finished_at": utc_now(),
        "status": status,
        "eligible_for_perceptual_review": status == "accepted_for_review",
        "agent_identity_ok": identity_ok,
        "agent_result": agent_result,
        "metrics": metrics_from(agent_result, process, verifier_process),
        "budget": budget_summary(agent_result, contract),
        "deterministic_verifier": verifier,
        "presentation": {"required": True, "authoritative_for_gameplay": False, "captures": captures},
        "workspace_sha256": benchmark_b0_stable_harness.stable_tree_hash(workspace),
        "workspace_hash_policy": benchmark_b0_stable_harness.WORKSPACE_HASH_POLICY,
        "integrity": {
            "scored": False,
            "scored_record_write_forbidden": True,
            "scored_freeze_valid_before": True,
            "scored_freeze_valid_after": freeze_valid,
            "automatic_retries": 0,
            "replacement_trials": 0,
        },
        "environment": {"os": platform.system(), "os_release": platform.release(), "machine": platform.machine(), "python": platform.python_version()},
        "artifacts": {"trial_root": str(trial_root), "workspace": str(workspace), "agent_result": str(agent_result_path), "verifier_result": str(verifier_result_path)},
    }
    write_json(trial_root / "record.json", record)
    return record


def start(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if root.exists() and any(root.iterdir()):
        raise AcceptanceError("acceptance v1 durable root is already consumed; refusing rerun-until-win")
    root.mkdir(parents=True, exist_ok=True)

    preflight = scored.preflight_environment(LANE)
    execution = load_json(REPO_ROOT / EXECUTION_PATH)
    profile = load_profile()
    records = [initial_record(root=root, run_index=index, contract=contract, execution=execution, profile=profile) for index in range(1, int(contract["initial_runs"]) + 1)]
    eligible = [
        {"run_index": record["run_index"], "trial_id": record["trial_id"], "workspace": record["artifacts"]["workspace"], "workspace_sha256": record["workspace_sha256"], "captures": record["presentation"]["captures"]}
        for record in records if record["eligible_for_perceptual_review"]
    ]
    review_target = eligible[0] if eligible else None
    summary = {
        "schema_version": 1,
        "kind": "trace2d_b2_nonscored_acceptance_initial_summary",
        "benchmark_id": "trace2d-b2",
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "scored": False,
        "contract_sha256": sha256_file(REPO_ROOT / CONTRACT_PATH),
        "task_prompt_sha256": contract["task_prompt_sha256"],
        "matched_initial_runs": len(records),
        "records": [
            {"run_index": record["run_index"], "trial_id": record["trial_id"], "status": record["status"], "deterministic_pass": deterministic_pass(record["deterministic_verifier"]), "capture_count": len(record["presentation"]["captures"]), "agent_identity_ok": record["agent_identity_ok"], "budget": record["budget"], "metrics": record["metrics"]}
            for record in records
        ],
        "eligible_trials": eligible,
        "review_target": review_target,
        "preflight": preflight,
        "next_required_phase": "perceptual_review" if review_target else "acceptance_failed_no_deterministic_candidate",
        "scored_cohort_unchanged": True,
    }
    write_json(root / INITIAL_SUMMARY, summary)
    return summary


def validate_review_payload(payload: dict[str, Any], summary: dict[str, Any]) -> dict[str, Any]:
    target = summary.get("review_target")
    if not isinstance(target, dict):
        raise AcceptanceError("no deterministic acceptance candidate exists for perceptual review")
    if payload.get("reviewer_agent") != "ChatGPT":
        raise AcceptanceError("perceptual reviewer must be ChatGPT")
    if payload.get("model") != "GPT-5.6 Sol":
        raise AcceptanceError("perceptual review model identity drifted")
    if payload.get("target_trial_id") != target.get("trial_id"):
        raise AcceptanceError("perceptual review target differs from selected deterministic candidate")
    captures = target.get("captures")
    if not isinstance(captures, list) or not captures:
        raise AcceptanceError("selected review target has no presentation capture")
    if payload.get("capture_sha256") != captures[0].get("sha256"):
        raise AcceptanceError("perceptual review capture SHA-256 mismatch")
    findings = payload.get("findings")
    if not isinstance(findings, list) or not findings or not all(isinstance(item, str) and item.strip() for item in findings):
        raise AcceptanceError("perceptual review requires non-empty findings")
    recommendation = payload.get("recommendation")
    if not isinstance(recommendation, str) or not recommendation.strip():
        raise AcceptanceError("perceptual review requires a recommendation")
    return payload


def record_review(args: argparse.Namespace) -> dict[str, Any]:
    validate_contract()
    root = require_acceptance_root(args.runs_root)
    summary = load_json(root / INITIAL_SUMMARY)
    review_path = root / PERCEPTUAL_REVIEW
    if review_path.exists():
        raise AcceptanceError("perceptual review already recorded; refusing replacement")
    payload = validate_review_payload(load_json(Path(args.review_file).expanduser().resolve()), summary)
    review = {"schema_version": 1, "kind": "trace2d_b2_nonscored_perceptual_review", "scored": False, "recorded_at": utc_now(), **payload, "advisory_only": True, "may_override_deterministic_failure": False, "candidate_modified": False}
    write_json(review_path, review)
    return review


def build_revision_prompt(feedback: str, perceptual_review: dict[str, Any]) -> str:
    return f"""# B2 non-scored acceptance: human-feedback revision

The existing Trace2D candidate in this workspace already passed the independent
deterministic gameplay verifier. This is exactly one post-review revision cycle.

Preserve every deterministic gameplay semantic and normal public-API path.
Do not change benchmark/verifier/harness files and do not add acceptance-only
shortcuts.

## Advisory perceptual review

{perceptual_review.get('recommendation', '')}

## Human feedback (authoritative for this revision request)

--- BEGIN HUMAN FEEDBACK ---
{feedback}
--- END HUMAN FEEDBACK ---

Apply the human feedback to the game's presentation/usability while preserving
the deterministic contract. Rebuild/re-run what is needed, and retain a fresh
final presentation capture under `.trace2d-b2-evidence/presentation/`.
"""


def feedback(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if (root / REVISION_RECORD).exists():
        raise AcceptanceError("human-feedback revision already consumed; refusing a second cycle")
    summary = load_json(root / INITIAL_SUMMARY)
    target = summary.get("review_target")
    if not isinstance(target, dict):
        raise AcceptanceError("no deterministically accepted initial candidate is available")
    perceptual_review = load_json(root / PERCEPTUAL_REVIEW)

    feedback_text = Path(args.feedback_file).expanduser().resolve().read_text(encoding="utf-8").strip()
    if not feedback_text:
        raise AcceptanceError("human feedback must be non-empty")
    if len(feedback_text) > 4000:
        raise AcceptanceError("human feedback exceeds the 4000-character acceptance limit")

    workspace = Path(str(target["workspace"])).resolve()
    if not workspace.is_dir():
        raise AcceptanceError("retained review-target workspace is missing")
    before_hash = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    if before_hash != target.get("workspace_sha256"):
        raise AcceptanceError("retained review-target workspace changed before human feedback")

    revision_root = root / "revision"
    revision_root.mkdir(parents=True, exist_ok=False)
    feedback_copy = revision_root / "human-feedback.txt"
    feedback_copy.write_text(feedback_text + "\n", encoding="utf-8")
    prompt = revision_root / "human-feedback-revision-prompt.md"
    prompt.write_text(build_revision_prompt(feedback_text, perceptual_review), encoding="utf-8")

    agent_result_path = revision_root / "agent-result.json"
    verifier_result_path = revision_root / "verifier-result.json"
    started_at = utc_now()
    process = run_agent(workspace=workspace, prompt=prompt, result_path=agent_result_path, contract=contract, task_env_id=f"{ACCEPTANCE_TASK_ID}-human-revision-v1")
    (revision_root / "adapter.stdout.txt").write_text(str(process.get("stdout", "")), encoding="utf-8")
    (revision_root / "adapter.stderr.txt").write_text(str(process.get("stderr", "")), encoding="utf-8")
    agent_result = load_optional_result(agent_result_path)
    identity_ok = agent_identity_ok(agent_result, load_profile())

    verifier_process, verifier = run_verifier(workspace, verifier_result_path, 201)
    (revision_root / "verifier.stdout.txt").write_text(str(verifier_process.get("stdout", "")), encoding="utf-8")
    (revision_root / "verifier.stderr.txt").write_text(str(verifier_process.get("stderr", "")), encoding="utf-8")
    captures = scored.presentation_files(workspace)
    after_hash = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    changed = after_hash != before_hash
    freeze_valid = True
    try:
        benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    except Exception:
        freeze_valid = False

    success = identity_ok and deterministic_pass(verifier) and bool(captures) and changed and freeze_valid
    record = {
        "schema_version": 1,
        "kind": "trace2d_b2_nonscored_acceptance_revision",
        "benchmark_id": "trace2d-b2",
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "scored": False,
        "phase": "human_feedback_revision",
        "target_trial_id": target["trial_id"],
        "started_at": started_at,
        "finished_at": utc_now(),
        "status": "accepted" if success else "revision_failed_acceptance",
        "agent_identity_ok": identity_ok,
        "human_feedback": {"events": 1, "revision_cycles": 1, "text": feedback_text, "sha256": sha256_text(feedback_text), "source": "owner feedback relayed from ChatGPT conversation"},
        "perceptual_review_sha256": sha256_file(root / PERCEPTUAL_REVIEW),
        "agent_result": agent_result,
        "metrics": metrics_from(agent_result, process, verifier_process),
        "budget": budget_summary(agent_result, contract),
        "deterministic_verifier": verifier,
        "presentation": {"required": True, "authoritative_for_gameplay": False, "captures": captures},
        "workspace_before_sha256": before_hash,
        "workspace_after_sha256": after_hash,
        "workspace_changed_by_revision": changed,
        "integrity": {"scored": False, "scored_record_write_forbidden": True, "scored_freeze_valid_after_revision": freeze_valid, "automatic_retries": 0, "feedback_revision_cycles": 1},
        "acceptance": {"deterministic_reverification_passed": deterministic_pass(verifier), "presentation_retained": bool(captures), "perceptual_review_recorded": True, "human_feedback_recorded": True, "agent_revision_observed": changed, "full_loop_passed": success},
    }
    write_json(root / REVISION_RECORD, record)
    return record


def status(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    summary = load_json(root / INITIAL_SUMMARY) if (root / INITIAL_SUMMARY).is_file() else None
    review = load_json(root / PERCEPTUAL_REVIEW) if (root / PERCEPTUAL_REVIEW).is_file() else None
    revision = load_json(root / REVISION_RECORD) if (root / REVISION_RECORD).is_file() else None
    return {"acceptance_id": ACCEPTANCE_TASK_ID, "scored": False, "contract_sha256": sha256_file(REPO_ROOT / CONTRACT_PATH), "task_prompt_sha256": contract["task_prompt_sha256"], "initial": summary, "perceptual_review": review, "revision": revision, "full_loop_passed": bool(revision and revision.get("acceptance", {}).get("full_loop_passed") is True)}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D B2 non-scored acceptance harness")
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate-contract")
    validate.set_defaults(handler=lambda _: validate_contract())
    preflight = commands.add_parser("preflight")
    preflight.add_argument("--runs-root", required=True)
    preflight.set_defaults(handler=lambda args: {"contract": validate_contract(), "root": str(require_acceptance_root(args.runs_root)), "environment": scored.preflight_environment(LANE)})
    start_cmd = commands.add_parser("start")
    start_cmd.add_argument("--runs-root", required=True)
    start_cmd.set_defaults(handler=start)
    review = commands.add_parser("record-review")
    review.add_argument("--runs-root", required=True)
    review.add_argument("--review-file", required=True)
    review.set_defaults(handler=record_review)
    feedback_cmd = commands.add_parser("feedback")
    feedback_cmd.add_argument("--runs-root", required=True)
    feedback_cmd.add_argument("--feedback-file", required=True)
    feedback_cmd.set_defaults(handler=feedback)
    state = commands.add_parser("status")
    state.add_argument("--runs-root", required=True)
    state.set_defaults(handler=status)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        result = args.handler(args)
    except (AcceptanceError, scored.B2HarnessError, OSError, subprocess.SubprocessError, ValueError) as exc:
        print(f"B2 acceptance harness error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
