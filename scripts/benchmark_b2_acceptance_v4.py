#!/usr/bin/env python3
"""Append-only non-scored B2 acceptance-v4 harness.

V4 evaluates only the general public C++ discovery remediation frozen by
contract-v4. It reuses acceptance-v3's task, gates, rubric and budget exactly,
but owns a new durable root and never reads or writes consumed v1/v2/v3 state.
"""
from __future__ import annotations

import argparse
import json
import platform
import shutil
import subprocess
import sys
import zlib
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
for _path in (REPO_ROOT, SCRIPT_DIR):
    value = str(_path)
    if value not in sys.path:
        sys.path.insert(0, value)

import benchmark_b0_stable_harness  # noqa: E402
import benchmark_b2_acceptance as v1  # noqa: E402
import benchmark_b2_acceptance_v2 as v2  # noqa: E402
import benchmark_b2_acceptance_v4_freeze as freeze  # noqa: E402
import benchmark_b2_execution_freeze  # noqa: E402
import benchmark_b2_scored_harness as scored  # noqa: E402

CONTRACT_PATH = Path("benchmarks/b2/acceptance/contract-v4.json")
EXECUTION_PATH = Path("benchmarks/b2/execution-v1.json")
LANE = "trace2d.agent"
SCORING_TASK_ID = "b2-topdown-combat-v1"
ACCEPTANCE_TASK_ID = "b2-acceptance-ember-hall-playable-v2"
ROOT_TOKEN = "benchmark-b2-acceptance-v4"
FORBIDDEN_ROOT_TOKENS = (
    "benchmark-b2-scored-v1",
    "benchmark-b2-acceptance-v1",
    "benchmark-b2-acceptance-v2",
    "benchmark-b2-acceptance-v3",
)
INITIAL_SUMMARY = "initial-summary.json"
PERCEPTUAL_REVIEW = "perceptual-review.json"
REVISION_RECORD = "revision-record.json"
FINAL_REVIEW_TARGET = "final-review-target.json"
FINAL_REVIEW = "final-perceptual-review.json"


class AcceptanceV4Error(RuntimeError):
    pass


def load_json(path: Path) -> dict[str, Any]:
    return v1.load_json(path)


def write_json(path: Path, value: dict[str, Any]) -> None:
    v1.write_json(path, value)


def validate_contract() -> dict[str, Any]:
    freeze.validate_repository(REPO_ROOT)
    contract = load_json(REPO_ROOT / CONTRACT_PATH)
    if contract.get("acceptance_version") != 4 or contract.get("state") != "frozen_pre_acceptance":
        raise AcceptanceV4Error("acceptance-v4 contract is not frozen")
    if contract.get("scored") is not False or contract.get("lane") != LANE:
        raise AcceptanceV4Error("acceptance-v4 scoring/lane identity drifted")
    if contract.get("initial_runs") != 2:
        raise AcceptanceV4Error("acceptance-v4 must retain exactly two initial attempts")
    if contract.get("task_id") != ACCEPTANCE_TASK_ID:
        raise AcceptanceV4Error("acceptance-v4 task identity drifted")
    isolation = contract.get("isolation", {})
    if isolation.get("durable_root_name") != ROOT_TOKEN:
        raise AcceptanceV4Error("acceptance-v4 durable root drifted")
    if isolation.get("historical_acceptance_root_names") != [
        "benchmark-b2-acceptance-v1",
        "benchmark-b2-acceptance-v2",
        "benchmark-b2-acceptance-v3",
    ]:
        raise AcceptanceV4Error("acceptance-v4 historical root guard drifted")
    return contract


def require_acceptance_root(path_text: str) -> Path:
    root = Path(path_text).expanduser().resolve()
    lowered = str(root).casefold()
    if ROOT_TOKEN.casefold() not in lowered:
        raise AcceptanceV4Error("acceptance-v4 root identity is required")
    for forbidden in FORBIDDEN_ROOT_TOKENS:
        if forbidden.casefold() in lowered:
            raise AcceptanceV4Error(f"refusing historical/scored root: {forbidden}")
    try:
        root.relative_to(REPO_ROOT.resolve())
    except ValueError:
        pass
    else:
        raise AcceptanceV4Error("acceptance-v4 durable state must live outside repository")
    if (root / "raw.jsonl").exists():
        raise AcceptanceV4Error("refusing scored-style raw.jsonl")
    return root


def _freeze_valid() -> bool:
    try:
        freeze.validate_repository(REPO_ROOT)
        return True
    except Exception:
        return False


def _capture_refs(gate: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        role: {
            "path": result["path"],
            "sha256": result["sha256"],
            "byte_count": result["byte_count"],
            "width": result["width"],
            "height": result["height"],
        }
        for role, result in gate["captures"].items()
    }


def _initial_status(
    process: dict[str, Any],
    identity_ok: bool,
    verifier: dict[str, Any] | None,
    gate: dict[str, Any],
    freeze_valid: bool,
) -> str:
    if process.get("timed_out"):
        return "agent_timeout"
    if not identity_ok:
        return "agent_identity_or_result_failure"
    if not v1.deterministic_pass(verifier):
        return "deterministic_failure"
    if not gate.get("passed"):
        return "presentation_gate_failure"
    if not freeze_valid:
        return "integrity_failure"
    return "accepted_for_perceptual_review"


def initial_record(
    *,
    root: Path,
    run_index: int,
    contract: dict[str, Any],
    execution: dict[str, Any],
    profile: dict[str, Any],
) -> dict[str, Any]:
    trial_id = f"accept-v4-initial-{run_index:02d}-trace2d-agent"
    trial_root = root / "initial" / trial_id
    if trial_root.exists():
        raise AcceptanceV4Error(f"acceptance-v4 trial already exists: {trial_id}")
    trial_root.mkdir(parents=True)

    workspace = trial_root / "workspace"
    starter = REPO_ROOT / execution["lane_starters"][LANE]["root"]
    shutil.copytree(starter, workspace)
    scored.verify_starter(workspace, LANE, execution)

    prompt = trial_root / "frozen-acceptance-v4-prompt.md"
    source_prompt = REPO_ROOT / str(contract["task_prompt"])
    shutil.copy2(source_prompt, prompt)
    if v1.sha256_file(prompt) != contract["task_prompt_sha256"]:
        raise AcceptanceV4Error("copied acceptance-v4 prompt drifted")

    freeze.validate_repository(REPO_ROOT)
    agent_result_path = trial_root / "agent-result.json"
    verifier_result_path = trial_root / "verifier-result.json"
    started_at = v1.utc_now()
    process = v1.run_agent(
        workspace=workspace,
        prompt=prompt,
        result_path=agent_result_path,
        contract=contract,
        task_env_id="b2-acceptance-v4-ember-hall",
    )
    (trial_root / "adapter.stdout.txt").write_text(str(process.get("stdout", "")), encoding="utf-8")
    (trial_root / "adapter.stderr.txt").write_text(str(process.get("stderr", "")), encoding="utf-8")
    agent_result = v1.load_optional_result(agent_result_path)
    identity_ok = v1.agent_identity_ok(agent_result, profile)

    verifier_process, verifier = v1.run_verifier(workspace, verifier_result_path, 700 + run_index)
    (trial_root / "verifier.stdout.txt").write_text(str(verifier_process.get("stdout", "")), encoding="utf-8")
    (trial_root / "verifier.stderr.txt").write_text(str(verifier_process.get("stderr", "")), encoding="utf-8")
    gate = v2.presentation_gate(workspace, contract)
    freeze_valid = _freeze_valid()
    status = _initial_status(process, identity_ok, verifier, gate, freeze_valid)

    record = {
        "schema_version": 4,
        "kind": "trace2d_b2_nonscored_acceptance_v4_initial",
        "benchmark_id": "trace2d-b2",
        "acceptance_version": 4,
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "scored": False,
        "phase": "initial",
        "run_index": run_index,
        "trial_id": trial_id,
        "lane_id": LANE,
        "task_id": ACCEPTANCE_TASK_ID,
        "deterministic_contract_task_id": SCORING_TASK_ID,
        "started_at": started_at,
        "finished_at": v1.utc_now(),
        "status": status,
        "eligible_for_perceptual_review": status == "accepted_for_perceptual_review",
        "agent_identity_ok": identity_ok,
        "agent_result": agent_result,
        "metrics": v1.metrics_from(agent_result, process, verifier_process),
        "budget": v1.budget_summary(agent_result, contract),
        "deterministic_verifier": verifier,
        "presentation_gate": gate,
        "workspace_sha256": benchmark_b0_stable_harness.stable_tree_hash(workspace),
        "workspace_hash_policy": benchmark_b0_stable_harness.WORKSPACE_HASH_POLICY,
        "integrity": {
            "scored": False,
            "scored_record_write_forbidden": True,
            "acceptance_v1_write_forbidden": True,
            "acceptance_v2_write_forbidden": True,
            "acceptance_v3_write_forbidden": True,
            "v4_freeze_valid_after_agent": freeze_valid,
            "automatic_retries": 0,
            "replacement_trials": 0,
        },
        "environment": {
            "os": platform.system(),
            "os_release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "artifacts": {
            "trial_root": str(trial_root),
            "workspace": str(workspace),
            "agent_result": str(agent_result_path),
            "verifier_result": str(verifier_result_path),
        },
    }
    write_json(trial_root / "record.json", record)
    return record


def start(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if root.exists() and any(root.iterdir()):
        raise AcceptanceV4Error("acceptance-v4 root is already consumed; refusing rerun-until-win")
    root.mkdir(parents=True, exist_ok=True)

    preflight = scored.preflight_environment(LANE)
    execution = load_json(REPO_ROOT / EXECUTION_PATH)
    profile = v1.load_profile()
    records = [
        initial_record(root=root, run_index=index, contract=contract, execution=execution, profile=profile)
        for index in range(1, 3)
    ]

    eligible: list[dict[str, Any]] = []
    for record in records:
        if record["eligible_for_perceptual_review"]:
            eligible.append({
                "run_index": record["run_index"],
                "trial_id": record["trial_id"],
                "workspace": record["artifacts"]["workspace"],
                "workspace_sha256": record["workspace_sha256"],
                "captures": _capture_refs(record["presentation_gate"]),
            })
    review_target = eligible[0] if eligible else None
    summary = {
        "schema_version": 4,
        "kind": "trace2d_b2_nonscored_acceptance_v4_initial_summary",
        "benchmark_id": "trace2d-b2",
        "acceptance_version": 4,
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "scored": False,
        "contract_sha256": v1.sha256_file(REPO_ROOT / CONTRACT_PATH),
        "task_prompt_sha256": contract["task_prompt_sha256"],
        "matched_initial_runs": len(records),
        "records": [
            {
                "run_index": record["run_index"],
                "trial_id": record["trial_id"],
                "status": record["status"],
                "deterministic_pass": v1.deterministic_pass(record["deterministic_verifier"]),
                "presentation_gate_pass": record["presentation_gate"]["passed"],
                "presentation_gate_failures": record["presentation_gate"]["failures"],
                "agent_identity_ok": record["agent_identity_ok"],
                "budget": record["budget"],
                "metrics": record["metrics"],
            }
            for record in records
        ],
        "eligible_trials": eligible,
        "review_target": review_target,
        "preflight": preflight,
        "next_required_phase": "perceptual_review" if review_target else "acceptance_v4_failed_no_reviewable_candidate",
        "scored_cohort_unchanged": True,
        "acceptance_v1_unchanged": True,
        "acceptance_v2_unchanged": True,
        "acceptance_v3_unchanged": True,
    }
    write_json(root / INITIAL_SUMMARY, summary)
    return summary


def record_review(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    review_path = root / PERCEPTUAL_REVIEW
    if review_path.exists():
        raise AcceptanceV4Error("initial perceptual review already recorded")
    summary = load_json(root / INITIAL_SUMMARY)
    payload = v2.validate_review_payload(
        load_json(Path(args.review_file).expanduser().resolve()), summary, contract
    )
    review = {
        "schema_version": 4,
        "kind": "trace2d_b2_nonscored_acceptance_v4_perceptual_review",
        "acceptance_version": 4,
        "scored": False,
        "recorded_at": v1.utc_now(),
        **payload,
        "may_override_deterministic_failure": False,
        "candidate_modified": False,
    }
    write_json(review_path, review)
    return review


def build_revision_prompt(feedback_text: str, review: dict[str, Any]) -> str:
    return f"""# B2 non-scored acceptance-v4: one human-feedback revision

This retained Trace2D candidate already passed the independent deterministic
gameplay verifier and the unchanged acceptance-v3/v4 machine presentation gate.

Preserve every deterministic gameplay semantic and every presentation
requirement. Do not change benchmark/verifier/harness files and do not add
acceptance-only shortcuts.

## Perceptual review recommendation

{review.get('recommendation', '')}

## Human feedback

--- BEGIN HUMAN FEEDBACK ---
{feedback_text}
--- END HUMAN FEEDBACK ---

Apply exactly this presentation/usability feedback while preserving gameplay.
Regenerate all four required captures: `ember-hall-overview.png`,
`ember-hall-attack.png`, `ember-hall-hit.png`, and `ember-hall-death.png`.
"""


def feedback(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if (root / REVISION_RECORD).exists():
        raise AcceptanceV4Error("acceptance-v4 feedback cycle already consumed")
    summary = load_json(root / INITIAL_SUMMARY)
    target = summary.get("review_target")
    if not isinstance(target, dict):
        raise AcceptanceV4Error("no acceptance-v4 review target")
    review = load_json(root / PERCEPTUAL_REVIEW)
    if review.get("passed") is not True:
        raise AcceptanceV4Error("human feedback requires a passing initial perceptual review")

    feedback_text = Path(args.feedback_file).expanduser().resolve().read_text(encoding="utf-8").strip()
    if not feedback_text or len(feedback_text) > 4000:
        raise AcceptanceV4Error("human feedback must be non-empty and <=4000 characters")
    workspace = Path(str(target["workspace"])).resolve()
    before_hash = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    if before_hash != target.get("workspace_sha256"):
        raise AcceptanceV4Error("review-target workspace changed before feedback")

    revision_root = root / "revision"
    revision_root.mkdir(parents=True, exist_ok=False)
    (revision_root / "human-feedback.txt").write_text(feedback_text + "\n", encoding="utf-8")
    prompt = revision_root / "human-feedback-revision-prompt.md"
    prompt.write_text(build_revision_prompt(feedback_text, review), encoding="utf-8")
    result_path = revision_root / "agent-result.json"
    verifier_path = revision_root / "verifier-result.json"
    started_at = v1.utc_now()
    process = v1.run_agent(
        workspace=workspace,
        prompt=prompt,
        result_path=result_path,
        contract=contract,
        task_env_id="b2-acceptance-v4-human-revision-v1",
    )
    (revision_root / "adapter.stdout.txt").write_text(str(process.get("stdout", "")), encoding="utf-8")
    (revision_root / "adapter.stderr.txt").write_text(str(process.get("stderr", "")), encoding="utf-8")
    agent_result = v1.load_optional_result(result_path)
    identity_ok = v1.agent_identity_ok(agent_result, v1.load_profile())
    verifier_process, verifier = v1.run_verifier(workspace, verifier_path, 801)
    (revision_root / "verifier.stdout.txt").write_text(str(verifier_process.get("stdout", "")), encoding="utf-8")
    (revision_root / "verifier.stderr.txt").write_text(str(verifier_process.get("stderr", "")), encoding="utf-8")
    gate = v2.presentation_gate(workspace, contract)
    after_hash = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    freeze_valid = _freeze_valid()
    machine_success = (
        identity_ok
        and v1.deterministic_pass(verifier)
        and gate["passed"]
        and after_hash != before_hash
        and freeze_valid
    )
    record = {
        "schema_version": 4,
        "kind": "trace2d_b2_nonscored_acceptance_v4_revision",
        "benchmark_id": "trace2d-b2",
        "acceptance_version": 4,
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "scored": False,
        "phase": "human_feedback_revision",
        "target_trial_id": target["trial_id"],
        "started_at": started_at,
        "finished_at": v1.utc_now(),
        "status": "awaiting_final_perceptual_confirmation" if machine_success else "revision_failed_acceptance_v4",
        "agent_identity_ok": identity_ok,
        "human_feedback": {
            "events": 1,
            "revision_cycles": 1,
            "text": feedback_text,
            "sha256": v1.sha256_text(feedback_text),
            "source": "real owner feedback relayed from ChatGPT conversation",
        },
        "metrics": v1.metrics_from(agent_result, process, verifier_process),
        "budget": v1.budget_summary(agent_result, contract),
        "deterministic_verifier": verifier,
        "presentation_gate": gate,
        "workspace_before_sha256": before_hash,
        "workspace_after_sha256": after_hash,
        "workspace_changed": after_hash != before_hash,
        "integrity": {
            "scored": False,
            "scored_record_write_forbidden": True,
            "acceptance_v1_write_forbidden": True,
            "acceptance_v2_write_forbidden": True,
            "acceptance_v3_write_forbidden": True,
            "v4_freeze_valid_after": freeze_valid,
            "human_feedback_events": 1,
            "feedback_revision_cycles": 1,
        },
        "machine_acceptance_passed": machine_success,
        "full_loop_passed": False,
    }
    write_json(root / REVISION_RECORD, record)
    if machine_success:
        write_json(root / FINAL_REVIEW_TARGET, {
            "schema_version": 4,
            "kind": "trace2d_b2_nonscored_acceptance_v4_final_review_target",
            "acceptance_version": 4,
            "scored": False,
            "review_target": {
                "trial_id": target["trial_id"],
                "workspace": str(workspace),
                "captures": _capture_refs(gate),
            },
        })
    return record


def record_final_review(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if (root / FINAL_REVIEW).exists():
        raise AcceptanceV4Error("final perceptual review already recorded")
    revision = load_json(root / REVISION_RECORD)
    if revision.get("machine_acceptance_passed") is not True:
        raise AcceptanceV4Error("final review requires passing revision machine evidence")
    target_doc = load_json(root / FINAL_REVIEW_TARGET)
    summary = {"review_target": target_doc["review_target"]}
    payload = v2.validate_review_payload(
        load_json(Path(args.review_file).expanduser().resolve()), summary, contract, final=True
    )
    final_review = {
        "schema_version": 4,
        "kind": "trace2d_b2_nonscored_acceptance_v4_final_perceptual_review",
        "acceptance_version": 4,
        "scored": False,
        "recorded_at": v1.utc_now(),
        **payload,
        "final_confirmation": True,
    }
    write_json(root / FINAL_REVIEW, final_review)
    revision["status"] = "accepted"
    revision["full_loop_passed"] = True
    revision["final_perceptual_confirmation"] = {
        "passed": True,
        "recorded_at": final_review["recorded_at"],
        "rubric": final_review["rubric"],
    }
    write_json(root / REVISION_RECORD, revision)
    return {"final_review": final_review, "revision": revision}


def status(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)

    def optional(name: str) -> dict[str, Any] | None:
        path = root / name
        return load_json(path) if path.is_file() else None

    revision = optional(REVISION_RECORD)
    final_review = optional(FINAL_REVIEW)
    return {
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "acceptance_version": 4,
        "scored": False,
        "contract_sha256": v1.sha256_file(REPO_ROOT / CONTRACT_PATH),
        "task_prompt_sha256": contract["task_prompt_sha256"],
        "initial": optional(INITIAL_SUMMARY),
        "perceptual_review": optional(PERCEPTUAL_REVIEW),
        "revision": revision,
        "final_perceptual_review": final_review,
        "full_loop_passed": bool(
            revision
            and revision.get("full_loop_passed") is True
            and final_review
            and final_review.get("passed") is True
        ),
        "scored_cohort_unchanged": True,
        "acceptance_v1_unchanged": True,
        "acceptance_v2_unchanged": True,
        "acceptance_v3_unchanged": True,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D B2 non-scored acceptance-v4 harness")
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate-contract")
    validate.set_defaults(handler=lambda _: validate_contract())
    preflight = commands.add_parser("preflight")
    preflight.add_argument("--runs-root", required=True)
    preflight.set_defaults(handler=lambda args: {
        "contract": validate_contract(),
        "root": str(require_acceptance_root(args.runs_root)),
        "environment": scored.preflight_environment(LANE),
    })
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
    final = commands.add_parser("record-final-review")
    final.add_argument("--runs-root", required=True)
    final.add_argument("--review-file", required=True)
    final.set_defaults(handler=record_final_review)
    state = commands.add_parser("status")
    state.add_argument("--runs-root", required=True)
    state.set_defaults(handler=status)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        result = args.handler(args)
    except (
        AcceptanceV4Error,
        v2.AcceptanceV2Error,
        v1.AcceptanceError,
        scored.B2HarnessError,
        OSError,
        subprocess.SubprocessError,
        ValueError,
        zlib.error,
    ) as exc:
        print(f"B2 acceptance-v4 harness error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
