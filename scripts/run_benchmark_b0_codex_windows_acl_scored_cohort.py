#!/usr/bin/env python3
"""Run the preregistered B0 scored cohort with the frozen Windows ACL profile.

This owner-local entrypoint is intentionally separate from the unscored
calibration runner. It consumes the committed accepted-calibration evidence,
reuses only the already-qualified immutable toolchain, re-proves the real model
ACL boundary, executes exactly the preregistered nine scored slots with zero
replacement retries, independently reverifies every preserved workspace, and
packages the complete evidence directory.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import time
import uuid
from pathlib import Path
from typing import Any

import benchmark_b0
import benchmark_b0_codex_wrapper as core
import run_benchmark_b0_codex_chatgpt_calibration as calibration
import run_benchmark_b0_codex_windows_acl_calibration as windows

CODEX_VERSION = "0.144.6"
MODEL_ID = "gpt-5.5"
GODOT_VERSION = "4.7.1-stable"
NODE_VERSION = "22.18.0"
TASK_ID = "b0-semantic-scene-authoring"
WRAPPER_MODULE = "benchmark_b0_codex_windows_acl_wrapper"
PROFILE_RELATIVE = Path("benchmarks/b0/agent-profile.codex-0.144.6.json")
POLICY_RELATIVE = Path("benchmarks/b0/scored-cohort-v1.json")
ACCEPTANCE_RELATIVE = Path(
    "benchmarks/b0/qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json"
)
STABLE_HARNESS = Path(__file__).resolve().with_name("benchmark_b0_stable_harness.py")


class ScoredCohortError(RuntimeError):
    pass


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ScoredCohortError(f"failed to read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ScoredCohortError(f"expected JSON object: {path}")
    return value


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def require_file(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise ScoredCohortError(f"{label} not found: {resolved}")
    return resolved


def require_dir(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_dir():
        raise ScoredCohortError(f"{label} not found: {resolved}")
    return resolved


def policy_schedule(policy: dict[str, Any]) -> list[tuple[int, str]]:
    orders = policy.get("lane_order_by_repetition")
    if not isinstance(orders, list):
        raise ScoredCohortError("scored cohort policy requires lane_order_by_repetition")
    schedule: list[tuple[int, str]] = []
    expected_lanes = set(benchmark_b0.EXPECTED_LANES)
    for repetition, order in enumerate(orders, start=1):
        if not isinstance(order, list) or len(order) != 3 or set(order) != expected_lanes:
            raise ScoredCohortError(f"invalid lane order for repetition {repetition}: {order}")
        schedule.extend((repetition, str(lane)) for lane in order)
    if len(schedule) != int(policy.get("total_planned_trials", -1)):
        raise ScoredCohortError("policy total_planned_trials does not match lane schedule")
    if int(policy.get("repetitions_per_lane", -1)) != len(orders):
        raise ScoredCohortError("policy repetitions_per_lane does not match schedule")
    retry = policy.get("retry_policy", {})
    if retry.get("automatic_retries_per_trial") != 0:
        raise ScoredCohortError("B0 scored policy must keep automatic retries at zero")
    if retry.get("replacement_trials_for_infrastructure_failure") != 0:
        raise ScoredCohortError("B0 scored policy must keep replacement trials at zero")
    if retry.get("early_stopping") is not False:
        raise ScoredCohortError("B0 scored policy must disable early stopping")
    return schedule


def validate_frozen_contract(
    *,
    repo_root: Path,
    suite: dict[str, Any],
    policy: dict[str, Any],
    acceptance: dict[str, Any],
    profile: dict[str, Any],
) -> list[tuple[int, str]]:
    task = benchmark_b0.find_task(suite, TASK_ID)
    if suite.get("state") != "eligible" or task.get("state") != "eligible":
        raise ScoredCohortError("suite/task are not eligible")
    if policy.get("state") != "ready":
        raise ScoredCohortError("scored cohort policy is not ready")
    if acceptance.get("accepted") is not True or acceptance.get("scored") is not False:
        raise ScoredCohortError("accepted unscored calibration evidence is missing or invalid")
    if acceptance.get("promotion_decision", {}).get("suite_task_eligible") is not True:
        raise ScoredCohortError("accepted calibration does not authorize eligibility")
    validated_profile = benchmark_b0.validate_agent_profile(
        repo_root / PROFILE_RELATIVE,
        task,
    )
    if validated_profile != profile:
        raise ScoredCohortError("profile changed while validating frozen contract")
    profile_hash = benchmark_b0.sha256_json(profile)
    expected_hash = str(policy.get("agent_profile_canonical_sha256", ""))
    if profile_hash != expected_hash:
        raise ScoredCohortError(
            f"canonical profile hash mismatch: expected {expected_hash}, got {profile_hash}"
        )
    if profile_hash != str(acceptance.get("agent", {}).get("canonical_profile_sha256", "")):
        raise ScoredCohortError("accepted calibration used a different canonical Agent profile")
    if profile.get("model_id") != MODEL_ID or profile.get("model_revision") != MODEL_ID:
        raise ScoredCohortError("frozen model selector changed")
    if profile.get("budget") != policy.get("budget"):
        raise ScoredCohortError("scored policy budget differs from frozen Agent profile")
    for lane in benchmark_b0.EXPECTED_LANES:
        benchmark_b0.validate_qualification(suite, lane)
    return policy_schedule(policy)


def accepted_local_run_root(runs_root: Path, acceptance: dict[str, Any]) -> Path:
    archive = str(acceptance.get("source_archive", ""))
    if not archive.endswith(".zip"):
        raise ScoredCohortError("accepted calibration source_archive is invalid")
    run_root = runs_root / archive[:-4]
    if not run_root.is_dir():
        raise ScoredCohortError(
            f"accepted calibration run directory is missing: {run_root}. "
            "Keep the accepted owner-local run directory until #102 is complete."
        )
    return run_root


def build_environment(
    *,
    repo_root: Path,
    scripts_root: Path,
    local_base: Path,
    accepted_run: Path,
    profile: dict[str, Any],
) -> tuple[dict[str, str], dict[str, Any]]:
    accepted_toolchain = load_json(require_file(accepted_run / "toolchain.json", "accepted toolchain"))
    frozen_trace2d = str(accepted_toolchain.get("frozen_trace2d_commit", ""))
    if not frozen_trace2d:
        raise ScoredCohortError("accepted toolchain lacks frozen Trace2D commit")
    if accepted_toolchain.get("codex", {}).get("version") != CODEX_VERSION:
        raise ScoredCohortError("accepted toolchain Codex version changed")
    if accepted_toolchain.get("codex", {}).get("model_id") != MODEL_ID:
        raise ScoredCohortError("accepted toolchain model changed")
    if accepted_toolchain.get("godot", {}).get("version") != GODOT_VERSION:
        raise ScoredCohortError("accepted toolchain Godot version changed")
    if accepted_toolchain.get("node", {}).get("version") != NODE_VERSION:
        raise ScoredCohortError("accepted toolchain Node version changed")
    if accepted_toolchain.get("godot_mcp", {}).get("version") != "4.1.0":
        raise ScoredCohortError("accepted toolchain Godot MCP version changed")

    tools_root = local_base / "tools"
    trace2d_root = tools_root / f"trace2d-{frozen_trace2d}"
    trace2d_bin = require_file(trace2d_root / "cli" / "trace2d.exe", "frozen Trace2D CLI")
    trace2d_mcp = require_file(trace2d_root / "mcp" / "trace2d_mcp.exe", "frozen Trace2D MCP")

    godot_root = tools_root / f"godot-{GODOT_VERSION}"
    godot_archive = require_file(
        godot_root / f"Godot_v{GODOT_VERSION}_win64.exe.zip",
        "cached Godot archive",
    )
    if calibration.sha512_file(godot_archive) != str(accepted_toolchain["godot"]["archive_sha512"]).lower():
        raise ScoredCohortError("cached Godot archive differs from accepted calibration")
    godot_bin = require_file(
        godot_root / "extracted" / f"Godot_v{GODOT_VERSION}_win64.exe",
        "cached Godot executable",
    )

    node_root = tools_root / f"node-v{NODE_VERSION}"
    node_archive = require_file(node_root / f"node-v{NODE_VERSION}-win-x64.zip", "cached Node archive")
    if calibration.sha256_file(node_archive) != str(accepted_toolchain["node"]["archive_sha256"]).lower():
        raise ScoredCohortError("cached Node archive differs from accepted calibration")
    node_dir = require_dir(
        node_root / "extracted" / f"node-v{NODE_VERSION}-win-x64",
        "cached Node directory",
    )
    mcp_server = require_file(
        tools_root / f"godot-mcp-4.1.0-node-{NODE_VERSION}" / "node_modules" / ".bin" / "godot-mcp.cmd",
        "cached Godot MCP server",
    )
    shim_root = require_dir(tools_root / f"command-shims-{GODOT_VERSION}", "Godot command shims")

    auth_file = require_file(Path.home() / ".codex" / "auth.json", "file-backed Codex auth")
    env = os.environ.copy()
    env["PYTHONPATH"] = str(scripts_root)
    env["TRACE2D_BENCH_CODEX_AUTH_FILE"] = str(auth_file)
    env["TRACE2D_BENCH_GODOT_BIN"] = str(godot_bin)
    env["TRACE2D_BENCH_GODOT_MCP_SERVER"] = str(mcp_server)
    env["TRACE2D_BENCH_TRACE2D_BIN"] = str(trace2d_bin)
    env["TRACE2D_BENCH_TRACE2D_MCP_BIN"] = str(trace2d_mcp)
    env["TRACE2D_BENCH_CODEX_READ_ROOTS"] = os.pathsep.join([str(node_dir), str(shim_root)])
    env["TRACE2D_BENCH_NODE_VERSION"] = f"v{NODE_VERSION}"
    env["TRACE2D_BENCH_WRAPPER_TIMEOUT"] = "285"
    budget = profile["budget"]
    env["TRACE2D_BENCH_MAX_TOOL_CALLS"] = str(budget["max_tool_calls"])
    env["TRACE2D_BENCH_MAX_INPUT_TOKENS"] = str(budget["max_input_tokens"])
    env["TRACE2D_BENCH_MAX_OUTPUT_TOKENS"] = str(budget["max_output_tokens"])
    return env, accepted_toolchain


def preserve_process(root: Path, stem: str, completed: Any) -> None:
    calibration.preserve_process(root, stem, completed)


def main() -> int:
    parser = argparse.ArgumentParser(description="Trace2D B0 preregistered scored cohort")
    parser.add_argument("--runs-root")
    args = parser.parse_args()

    windows.configure()
    repo_root = Path(__file__).resolve().parent.parent
    scripts_root = repo_root / "scripts"
    suite_path = require_file(repo_root / "benchmarks/b0/suite.json", "B0 suite")
    profile_path = require_file(repo_root / PROFILE_RELATIVE, "frozen Agent profile")
    policy_path = require_file(repo_root / POLICY_RELATIVE, "B0 scored cohort policy")
    acceptance_path = require_file(repo_root / ACCEPTANCE_RELATIVE, "accepted unscored calibration")
    packager_path = require_file(scripts_root / "package_benchmark_b0_evidence.py", "evidence packager")

    suite = benchmark_b0.validate_suite(suite_path)
    profile = load_json(profile_path)
    policy = load_json(policy_path)
    acceptance = load_json(acceptance_path)
    schedule = validate_frozen_contract(
        repo_root=repo_root,
        suite=suite,
        policy=policy,
        acceptance=acceptance,
        profile=profile,
    )

    codex = core.resolve_codex("codex")
    actual_codex = core.verify_codex_version(codex, repo_root)
    if actual_codex != CODEX_VERSION:
        raise ScoredCohortError(f"Codex mismatch: expected {CODEX_VERSION}, got {actual_codex}")
    login = calibration.run_external(codex, ["login", "status"], cwd=repo_root)
    if login.returncode != 0:
        raise ScoredCohortError(f"codex login status failed: {login.stdout}\n{login.stderr}")

    local_appdata = os.environ.get("LOCALAPPDATA")
    if not local_appdata:
        raise ScoredCohortError("LOCALAPPDATA is required on the Windows owner-local host")
    local_base = Path(local_appdata) / "Trace2D" / "b0"
    runs_root = Path(args.runs_root).expanduser().resolve() if args.runs_root else local_base / "runs"
    runs_root.mkdir(parents=True, exist_ok=True)
    accepted_run = accepted_local_run_root(runs_root, acceptance)
    env, accepted_toolchain = build_environment(
        repo_root=repo_root,
        scripts_root=scripts_root,
        local_base=local_base,
        accepted_run=accepted_run,
        profile=profile,
    )

    git = shutil.which("git")
    if not git:
        raise ScoredCohortError("git executable not found")
    head = calibration.run([git, "rev-parse", "HEAD"], cwd=repo_root, capture=True).stdout.strip()

    stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime())
    run_root = runs_root / f"codex-chatgpt-scored-{stamp}-{uuid.uuid4().hex[:8]}"
    scored_root = run_root / "scored"
    preflight_root = run_root / "model-preflight"
    probe_root = run_root / "isolation-probe"
    orchestration_logs = run_root / "orchestration-logs"
    replay_path = run_root / "replay.jsonl"
    scored_root.mkdir(parents=True)
    probe_root.mkdir(parents=True)
    zip_path = run_root.with_suffix(".zip")
    canary_path = repo_root / "benchmarks" / "b0" / "verifiers" / f".codex-isolation-canary-{uuid.uuid4().hex}.txt"

    primary_error: Exception | None = None
    stage = "manifest"
    completed = False
    try:
        write_json(
            run_root / "cohort-manifest.json",
            {
                "schema_version": 1,
                "kind": "trace2d_b0_scored_cohort_run",
                "cohort_id": policy["cohort_id"],
                "repository_head": head,
                "accepted_unscored_archive": acceptance["source_archive"],
                "accepted_unscored_archive_sha256": acceptance["source_archive_sha256"],
                "agent_profile_canonical_sha256": benchmark_b0.sha256_json(profile),
                "schedule": [
                    {"slot": index, "repetition": repetition, "lane_id": lane}
                    for index, (repetition, lane) in enumerate(schedule, start=1)
                ],
                "retry_policy": policy["retry_policy"],
                "scored": true,
            },
        )

        stage = "model_preflight"
        print("Running frozen gpt-5.5 model preflight...")
        preflight = windows.stdin_model_preflight(
            codex=codex,
            auth_file=Path(env["TRACE2D_BENCH_CODEX_AUTH_FILE"]),
            workspace=preflight_root / "workspace",
            evidence_root=preflight_root,
        )
        if not preflight["passed"]:
            raise ScoredCohortError("model preflight failed")

        stage = "isolation_probe"
        canary_path.write_text(f"TRACE2D-B0-DENY-{uuid.uuid4().hex}", encoding="utf-8")
        probe_workspace = probe_root / "workspace"
        probe_workspace.mkdir()
        probe_evidence = probe_root / "isolation.json"
        probe = calibration.run(
            [
                sys.executable,
                "-m",
                WRAPPER_MODULE,
                "probe-isolation",
                "--workspace",
                str(probe_workspace),
                "--canary",
                str(canary_path),
                "--evidence",
                str(probe_evidence),
                "--timeout",
                str(windows.ISOLATION_TIMEOUT_SECONDS),
            ],
            cwd=repo_root,
            env=env,
            check=False,
            capture=True,
        )
        preserve_process(probe_root, "isolation-wrapper", probe)
        if probe.returncode != 0 or not probe_evidence.is_file():
            raise ScoredCohortError(f"real-model isolation probe failed with exit code {probe.returncode}")
        isolation = load_json(probe_evidence)
        if isolation.get("passed") is not True:
            raise ScoredCohortError("real-model isolation verdict is not positive")
        windows.scrub_transient_codex_state(probe_root)

        stage = "scored_trials"
        exit_records: list[dict[str, Any]] = []
        for slot, (repetition, lane) in enumerate(schedule, start=1):
            lane_env = env.copy()
            frozen_trace2d = str(accepted_toolchain["frozen_trace2d_commit"])
            lane_env["TRACE2D_BENCH_ENGINE_VERSION"] = (
                GODOT_VERSION if lane.startswith("godot.") else f"trace2d@{frozen_trace2d}"
            )
            trial_id = f"{TASK_ID}-{lane}-scored-r{repetition}"
            print(f"Running scored B0 slot {slot}/9: repetition {repetition}, {lane}")
            trial = calibration.run(
                [
                    sys.executable,
                    str(STABLE_HARNESS),
                    "run-trial",
                    "--task",
                    TASK_ID,
                    "--lane",
                    lane,
                    "--agent-profile",
                    str(profile_path),
                    "--runs-root",
                    str(scored_root),
                    "--trial-id",
                    trial_id,
                    "--scored",
                ],
                cwd=repo_root,
                env=lane_env,
                check=False,
                capture=True,
            )
            preserve_process(orchestration_logs, f"slot-{slot:02d}-{lane.replace('.', '-')}", trial)
            exit_records.append(
                {
                    "slot": slot,
                    "repetition": repetition,
                    "lane_id": lane,
                    "trial_id": trial_id,
                    "return_code": trial.returncode,
                }
            )
            windows.scrub_transient_codex_state(scored_root)
        write_json(run_root / "scored-exit-codes.json", exit_records)

        raw_path = require_file(scored_root / "raw.jsonl", "scored raw records")
        records = [json.loads(line) for line in raw_path.read_text(encoding="utf-8").splitlines() if line.strip()]
        if len(records) != len(schedule):
            raise ScoredCohortError(f"expected {len(schedule)} scored records, got {len(records)}")
        expected_hash = str(policy["agent_profile_canonical_sha256"])
        for index, (record, (repetition, lane)) in enumerate(zip(records, schedule), start=1):
            if record.get("scored") is not True:
                raise ScoredCohortError(f"slot {index} is not marked scored")
            if record.get("lane_id") != lane:
                raise ScoredCohortError(f"slot {index} lane mismatch")
            if record.get("agent_profile_sha256") != expected_hash:
                raise ScoredCohortError(f"slot {index} Agent profile hash mismatch")
            if int(record.get("metrics", {}).get("human_interventions", -1)) != 0:
                raise ScoredCohortError(f"slot {index} recorded human intervention")

        stage = "aggregate_report"
        report = calibration.run(
            [sys.executable, str(STABLE_HARNESS), "report", "--records", str(raw_path)],
            cwd=repo_root,
            env=env,
            capture=True,
        )
        preserve_process(orchestration_logs, "report", report)
        (run_root / "scored-report.json").write_text(report.stdout, encoding="utf-8")
        report_json = json.loads(report.stdout)
        if int(report_json.get("record_count", -1)) != len(schedule):
            raise ScoredCohortError("scored report count mismatch")
        if report_json.get("integrity", {}).get("same_agent_profile_per_task") is not True:
            raise ScoredCohortError("scored cohort mixed Agent profile hashes")

        stage = "independent_reverify"
        replay_exit: list[dict[str, Any]] = []
        for index, record in enumerate(records, start=1):
            trial_id = str(record["trial_id"])
            replay = calibration.run(
                [
                    sys.executable,
                    str(STABLE_HARNESS),
                    "reverify",
                    "--trial-id",
                    trial_id,
                    "--records",
                    str(raw_path),
                    "--replay-records",
                    str(replay_path),
                ],
                cwd=repo_root,
                env=env,
                check=False,
                capture=True,
            )
            preserve_process(orchestration_logs, f"reverify-{index:02d}", replay)
            replay_exit.append({"slot": index, "trial_id": trial_id, "return_code": replay.returncode})
        write_json(run_root / "reverify-exit-codes.json", replay_exit)
        if any(item["return_code"] != 0 for item in replay_exit):
            raise ScoredCohortError("one or more independent reverifications disagreed with preserved evidence")
        replay_records = [line for line in replay_path.read_text(encoding="utf-8").splitlines() if line.strip()]
        if len(replay_records) != len(schedule):
            raise ScoredCohortError("independent reverify record count mismatch")

        completed = True
    except Exception as exc:
        primary_error = exc
        write_json(
            run_root / "failure.json",
            {
                "schema_version": 1,
                "kind": "trace2d_b0_scored_cohort_failure",
                "stage": stage,
                "exception_type": type(exc).__name__,
                "message": str(exc),
                "scored": true,
            },
        )
    finally:
        try:
            canary_path.unlink(missing_ok=True)
        except OSError:
            pass
        windows.scrub_transient_codex_state(run_root)
        package = calibration.run(
            [sys.executable, str(packager_path), "--run-root", str(run_root), "--output", str(zip_path)],
            cwd=repo_root,
            env=env,
            check=False,
            capture=True,
        )
        if package.returncode != 0:
            raise ScoredCohortError(
                f"evidence packager failed with exit code {package.returncode}: {package.stderr}"
            )

    print(f"Evidence ZIP: {zip_path}")
    if primary_error is not None:
        raise ScoredCohortError(
            f"B0 scored cohort did not complete: {primary_error}. Upload the generated ZIP; do not rerun a failed slot."
        ) from primary_error
    if not completed:
        raise ScoredCohortError("B0 scored cohort did not complete")
    print("B0 preregistered scored cohort completed with exactly nine scheduled attempts.")
    print("Upload the scrubbed evidence ZIP for final #102 review; do not rerun any slot.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ScoredCohortError as exc:
        print(f"B0 scored cohort error: {exc}", file=sys.stderr)
        raise SystemExit(2)
