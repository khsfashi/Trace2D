#!/usr/bin/env python3
"""Non-scored B2 acceptance-v2 with an independent playable-presentation gate.

Acceptance-v2 is additive evidence only. It never rescales, rewrites or reuses
the immutable scored B2 cohort or the consumed acceptance-v1 durable root.
"""
from __future__ import annotations

import argparse
import collections
import json
import math
import platform
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
for _path in (REPO_ROOT, SCRIPT_DIR):
    _value = str(_path)
    if _value not in sys.path:
        sys.path.insert(0, _value)

import benchmark_b0_stable_harness  # noqa: E402
import benchmark_b2_acceptance as v1  # noqa: E402
import benchmark_b2_execution_freeze  # noqa: E402
import benchmark_b2_scored_harness as scored  # noqa: E402

CONTRACT_PATH = Path("benchmarks/b2/acceptance/contract-v2.json")
TASK_PROMPT_PATH = Path("benchmarks/b2/acceptance/tasks/ember-hall-v2/PROMPT.md")
EXECUTION_PATH = Path("benchmarks/b2/execution-v1.json")
LANE = "trace2d.agent"
SCORING_TASK_ID = "b2-topdown-combat-v1"
ACCEPTANCE_TASK_ID = "b2-acceptance-ember-hall-playable-v2"
FORBIDDEN_SCORED_ROOT_TOKEN = "benchmark-b2-scored-v1"
FORBIDDEN_V1_ROOT_TOKEN = "benchmark-b2-acceptance-v1"
ROOT_TOKEN = "benchmark-b2-acceptance-v2"
INITIAL_SUMMARY = "initial-summary.json"
PERCEPTUAL_REVIEW = "perceptual-review.json"
REVISION_RECORD = "revision-record.json"
FINAL_REVIEW = "final-perceptual-review.json"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class AcceptanceV2Error(RuntimeError):
    pass


def load_json(path: Path) -> dict[str, Any]:
    return v1.load_json(path)


def write_json(path: Path, value: dict[str, Any]) -> None:
    v1.write_json(path, value)


def validate_contract() -> dict[str, Any]:
    contract = load_json(REPO_ROOT / CONTRACT_PATH)
    if contract.get("kind") != "trace2d_b2_nonscored_acceptance_contract":
        raise AcceptanceV2Error("unexpected acceptance-v2 contract kind")
    if contract.get("state") != "frozen_pre_acceptance" or contract.get("scored") is not False:
        raise AcceptanceV2Error("acceptance-v2 must stay frozen and non-scored")
    if contract.get("acceptance_version") != 2:
        raise AcceptanceV2Error("acceptance-v2 version drifted")
    if contract.get("task_id") != ACCEPTANCE_TASK_ID:
        raise AcceptanceV2Error("acceptance-v2 task id drifted")
    if contract.get("lane") != LANE or int(contract.get("initial_runs", 0)) != 2:
        raise AcceptanceV2Error("acceptance-v2 lane/cohort drifted")
    if contract.get("deterministic_contract_task_id") != SCORING_TASK_ID:
        raise AcceptanceV2Error("deterministic gameplay authority drifted")
    if contract.get("task_prompt") != TASK_PROMPT_PATH.as_posix():
        raise AcceptanceV2Error("acceptance-v2 prompt path drifted")
    if v1.sha256_file(REPO_ROOT / TASK_PROMPT_PATH) != contract.get("task_prompt_sha256"):
        raise AcceptanceV2Error("acceptance-v2 prompt SHA-256 drifted")
    isolation = contract.get("isolation", {})
    if isolation.get("durable_root_name") != ROOT_TOKEN:
        raise AcceptanceV2Error("acceptance-v2 durable root drifted")
    if isolation.get("previous_acceptance_root_name") != FORBIDDEN_V1_ROOT_TOKEN:
        raise AcceptanceV2Error("acceptance-v1 immutability guard drifted")
    if isolation.get("scored_durable_root_name") != FORBIDDEN_SCORED_ROOT_TOKEN:
        raise AcceptanceV2Error("scored root immutability guard drifted")
    if isolation.get("previous_acceptance_write_forbidden") is not True:
        raise AcceptanceV2Error("acceptance-v1 write guard must remain enabled")
    if isolation.get("scored_record_write_forbidden") is not True:
        raise AcceptanceV2Error("scored write guard must remain enabled")
    required_roles = contract.get("presentation_gate", {}).get("required_capture_roles", {})
    if list(required_roles) != ["overview", "attack", "hit", "death"]:
        raise AcceptanceV2Error("presentation capture role order drifted")
    rubric = contract.get("perceptual_review", {}).get("rubric", [])
    if len(rubric) != 6 or len(set(rubric)) != 6:
        raise AcceptanceV2Error("perceptual rubric drifted")
    benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    return contract


def require_acceptance_root(path_text: str) -> Path:
    root = Path(path_text).expanduser().resolve()
    lowered = str(root).casefold()
    if FORBIDDEN_SCORED_ROOT_TOKEN.casefold() in lowered:
        raise AcceptanceV2Error("refusing immutable B2 scored root")
    if FORBIDDEN_V1_ROOT_TOKEN.casefold() in lowered:
        raise AcceptanceV2Error("refusing consumed acceptance-v1 root")
    if ROOT_TOKEN.casefold() not in lowered:
        raise AcceptanceV2Error("acceptance-v2 root identity is required")
    try:
        root.relative_to(REPO_ROOT.resolve())
    except ValueError:
        pass
    else:
        raise AcceptanceV2Error("acceptance-v2 durable state must live outside repository")
    if (root / "raw.jsonl").exists():
        raise AcceptanceV2Error("refusing scored-style raw.jsonl")
    return root


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png_rgb(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise AcceptanceV2Error(f"not a PNG: {path}")
    pos = len(PNG_SIGNATURE)
    width = height = bit_depth = color_type = interlace = None
    idat = bytearray()
    while pos + 12 <= len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", payload)
            if compression != 0 or filter_method != 0:
                raise AcceptanceV2Error("unsupported PNG compression/filter method")
        elif kind == b"IDAT":
            idat.extend(payload)
        elif kind == b"IEND":
            break
    if not width or not height or bit_depth != 8 or interlace != 0:
        raise AcceptanceV2Error("presentation gate supports non-interlaced 8-bit PNG only")
    channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color_type)
    if channels is None:
        raise AcceptanceV2Error(f"unsupported PNG color type: {color_type}")
    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    if len(raw) != height * (stride + 1):
        raise AcceptanceV2Error("unexpected PNG decoded size")
    rows: list[bytearray] = []
    offset = 0
    prior = bytearray(stride)
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        src = bytearray(raw[offset:offset + stride])
        offset += stride
        out = bytearray(stride)
        for i, value in enumerate(src):
            left = out[i - channels] if i >= channels else 0
            up = prior[i]
            up_left = prior[i - channels] if i >= channels else 0
            if filter_type == 0:
                recon = value
            elif filter_type == 1:
                recon = value + left
            elif filter_type == 2:
                recon = value + up
            elif filter_type == 3:
                recon = value + ((left + up) // 2)
            elif filter_type == 4:
                recon = value + _paeth(left, up, up_left)
            else:
                raise AcceptanceV2Error(f"unsupported PNG filter type: {filter_type}")
            out[i] = recon & 0xFF
        rows.append(out)
        prior = out
    pixels: list[tuple[int, int, int]] = []
    for row in rows:
        for i in range(0, stride, channels):
            if color_type in (0, 4):
                g = row[i]
                pixels.append((g, g, g))
            else:
                pixels.append((row[i], row[i + 1], row[i + 2]))
    return width, height, pixels


def _luma(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def _sample(pixels: list[tuple[int, int, int]], max_samples: int = 160000) -> list[tuple[int, int, int]]:
    if len(pixels) <= max_samples:
        return pixels
    step = max(1, len(pixels) // max_samples)
    return pixels[::step]


def analyze_png(path: Path, image_policy: dict[str, Any]) -> dict[str, Any]:
    width, height, pixels = decode_png_rgb(path)
    sampled = _sample(pixels)
    luminances = [_luma(pixel) for pixel in sampled]
    mean = sum(luminances) / len(luminances)
    variance = sum((value - mean) ** 2 for value in luminances) / len(luminances)
    quantized = collections.Counter((r // 32, g // 32, b // 32) for r, g, b in sampled)
    dominant_ratio = max(quantized.values()) / len(sampled)
    dark_ratio = sum(value < 90.0 for value in luminances) / len(luminances)
    cool_ratio = sum(g >= 120 and b >= 140 and r <= 120 for r, g, b in sampled) / len(sampled)
    hostile_ratio = sum(r >= 150 and b >= 100 and g <= 115 for r, g, b in sampled) / len(sampled)
    ember_ratio = sum(r >= 170 and 65 <= g <= 180 and b <= 90 for r, g, b in sampled) / len(sampled)

    hud_rows = max(1, height // 4)
    hud_pixels = []
    row_step = max(1, (width * hud_rows) // 50000)
    for index in range(0, width * hud_rows, row_step):
        hud_pixels.append(pixels[index])
    hud_luma = [_luma(pixel) for pixel in hud_pixels]
    hud_mean = sum(hud_luma) / len(hud_luma)
    hud_stddev = math.sqrt(sum((value - hud_mean) ** 2 for value in hud_luma) / len(hud_luma))
    hud_bright_ratio = sum(value >= 165.0 for value in hud_luma) / len(hud_luma)

    checks = {
        "minimum_width": width >= int(image_policy["minimum_width"]),
        "minimum_height": height >= int(image_policy["minimum_height"]),
        "minimum_byte_count": path.stat().st_size >= int(image_policy["minimum_byte_count"]),
        "dominant_color_ratio": dominant_ratio <= float(image_policy["maximum_dominant_quantized_color_ratio"]),
        "quantized_color_count": len(quantized) >= int(image_policy["minimum_quantized_color_count"]),
        "luminance_stddev": math.sqrt(variance) >= float(image_policy["minimum_luminance_stddev"]),
        "minimum_dark_pixel_ratio": dark_ratio >= float(image_policy["minimum_dark_pixel_ratio"]),
        "maximum_dark_pixel_ratio": dark_ratio <= float(image_policy["maximum_dark_pixel_ratio"]),
        "hud_bright_pixel_ratio": hud_bright_ratio >= float(image_policy["minimum_hud_bright_pixel_ratio"]),
        "hud_luminance_stddev": hud_stddev >= float(image_policy["minimum_hud_luminance_stddev"]),
    }
    return {
        "path": str(path),
        "sha256": v1.sha256_file(path),
        "byte_count": path.stat().st_size,
        "width": width,
        "height": height,
        "sample_count": len(sampled),
        "metrics": {
            "dominant_quantized_color_ratio": dominant_ratio,
            "quantized_color_count": len(quantized),
            "luminance_stddev": math.sqrt(variance),
            "dark_pixel_ratio": dark_ratio,
            "cool_player_ratio": cool_ratio,
            "hostile_enemy_ratio": hostile_ratio,
            "ember_orange_ratio": ember_ratio,
            "hud_bright_pixel_ratio": hud_bright_ratio,
            "hud_luminance_stddev": hud_stddev,
        },
        "checks": checks,
        "passed": all(checks.values()),
        "_sampled_pixels": sampled,
    }


def sampled_difference_ratio(left: dict[str, Any], right: dict[str, Any]) -> float:
    left_pixels = left["_sampled_pixels"]
    right_pixels = right["_sampled_pixels"]
    if left["width"] != right["width"] or left["height"] != right["height"]:
        return 1.0
    count = min(len(left_pixels), len(right_pixels))
    if not count:
        return 0.0
    changed = 0
    for a, b in zip(left_pixels[:count], right_pixels[:count]):
        if max(abs(a[i] - b[i]) for i in range(3)) >= 20:
            changed += 1
    return changed / count


def presentation_gate(workspace: Path, contract: dict[str, Any]) -> dict[str, Any]:
    gate = contract["presentation_gate"]
    role_paths = gate["required_capture_roles"]
    analyses: dict[str, dict[str, Any]] = {}
    failures: list[str] = []
    for role, relative in role_paths.items():
        path = workspace / relative
        if not path.is_file():
            failures.append(f"missing_capture:{role}")
            continue
        try:
            analyses[role] = analyze_png(path, gate["image"])
        except (OSError, ValueError, zlib.error, AcceptanceV2Error) as exc:
            failures.append(f"invalid_capture:{role}:{exc}")

    if set(analyses) == set(role_paths):
        overview = analyses["overview"]
        families = gate["visual_families"]
        metrics = overview["metrics"]
        family_checks = {
            "cool_player": metrics["cool_player_ratio"] >= float(families["minimum_cool_player_ratio"]),
            "hostile_enemy": metrics["hostile_enemy_ratio"] >= float(families["minimum_hostile_enemy_ratio"]),
            "ember_orange": metrics["ember_orange_ratio"] >= float(families["minimum_ember_orange_ratio"]),
        }
        overview["visual_family_checks"] = family_checks
        if not all(family_checks.values()):
            failures.extend(f"overview_visual_family:{name}" for name, passed in family_checks.items() if not passed)

        hashes = [analyses[role]["sha256"] for role in role_paths]
        if gate["state_change"]["all_capture_sha256_values_must_be_distinct"] and len(set(hashes)) != len(hashes):
            failures.append("capture_hashes_not_distinct")
        minimum_difference = float(gate["state_change"]["minimum_pairwise_sample_difference_ratio"])
        pairs = [("overview", "attack"), ("attack", "hit"), ("hit", "death")]
        differences = {}
        for left, right in pairs:
            ratio = sampled_difference_ratio(analyses[left], analyses[right])
            differences[f"{left}->{right}"] = ratio
            if ratio < minimum_difference:
                failures.append(f"state_change_too_small:{left}->{right}")
    else:
        differences = {}

    public_analyses = {}
    for role, result in analyses.items():
        public_analyses[role] = {key: value for key, value in result.items() if key != "_sampled_pixels"}
        if not result["passed"]:
            failures.extend(f"{role}:{name}" for name, passed in result["checks"].items() if not passed)
    return {
        "version": gate["version"],
        "authority": gate["authority"],
        "passed": not failures and len(analyses) == len(role_paths),
        "failures": failures,
        "captures": public_analyses,
        "state_change_sample_difference_ratio": differences,
        "may_override_deterministic_failure": False,
    }


def initial_status(process: dict[str, Any], identity_ok: bool, verifier: dict[str, Any] | None, gate: dict[str, Any]) -> str:
    if process.get("timed_out"):
        return "agent_timeout"
    if not identity_ok:
        return "agent_identity_or_result_failure"
    if not v1.deterministic_pass(verifier):
        return "deterministic_failure"
    if not gate.get("passed"):
        return "presentation_gate_failure"
    return "accepted_for_perceptual_review"


def initial_record(*, root: Path, run_index: int, contract: dict[str, Any], execution: dict[str, Any], profile: dict[str, Any]) -> dict[str, Any]:
    trial_id = f"accept-v2-initial-{run_index:02d}-trace2d-agent"
    trial_root = root / "initial" / trial_id
    if trial_root.exists():
        raise AcceptanceV2Error(f"acceptance-v2 trial already exists: {trial_id}")
    trial_root.mkdir(parents=True)
    workspace = trial_root / "workspace"
    starter = REPO_ROOT / execution["lane_starters"][LANE]["root"]
    shutil.copytree(starter, workspace)
    scored.verify_starter(workspace, LANE, execution)
    prompt = trial_root / "frozen-acceptance-v2-prompt.md"
    shutil.copy2(REPO_ROOT / TASK_PROMPT_PATH, prompt)
    if v1.sha256_file(prompt) != contract["task_prompt_sha256"]:
        raise AcceptanceV2Error("copied acceptance-v2 prompt drifted")

    benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    agent_result_path = trial_root / "agent-result.json"
    verifier_result_path = trial_root / "verifier-result.json"
    started_at = v1.utc_now()
    process = v1.run_agent(workspace=workspace, prompt=prompt, result_path=agent_result_path, contract=contract, task_env_id=ACCEPTANCE_TASK_ID)
    (trial_root / "adapter.stdout.txt").write_text(str(process.get("stdout", "")), encoding="utf-8")
    (trial_root / "adapter.stderr.txt").write_text(str(process.get("stderr", "")), encoding="utf-8")
    agent_result = v1.load_optional_result(agent_result_path)
    identity_ok = v1.agent_identity_ok(agent_result, profile)

    verifier_process, verifier = v1.run_verifier(workspace, verifier_result_path, 300 + run_index)
    (trial_root / "verifier.stdout.txt").write_text(str(verifier_process.get("stdout", "")), encoding="utf-8")
    (trial_root / "verifier.stderr.txt").write_text(str(verifier_process.get("stderr", "")), encoding="utf-8")
    gate = presentation_gate(workspace, contract)
    freeze_valid = True
    try:
        benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    except Exception:
        freeze_valid = False
    status = initial_status(process, identity_ok, verifier, gate)
    record = {
        "schema_version": 2,
        "kind": "trace2d_b2_nonscored_acceptance_v2_initial",
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
            "scored_freeze_valid_before": True,
            "scored_freeze_valid_after": freeze_valid,
            "automatic_retries": 0,
            "replacement_trials": 0
        },
        "environment": {
            "os": platform.system(),
            "os_release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version()
        },
        "artifacts": {
            "trial_root": str(trial_root),
            "workspace": str(workspace),
            "agent_result": str(agent_result_path),
            "verifier_result": str(verifier_result_path)
        }
    }
    write_json(trial_root / "record.json", record)
    return record


def start(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if root.exists() and any(root.iterdir()):
        raise AcceptanceV2Error("acceptance-v2 root is already consumed; refusing rerun-until-win")
    root.mkdir(parents=True, exist_ok=True)
    preflight = scored.preflight_environment(LANE)
    execution = load_json(REPO_ROOT / EXECUTION_PATH)
    profile = v1.load_profile()
    records = [
        initial_record(root=root, run_index=index, contract=contract, execution=execution, profile=profile)
        for index in range(1, int(contract["initial_runs"]) + 1)
    ]
    eligible = []
    for record in records:
        if record["eligible_for_perceptual_review"]:
            captures = {
                role: {
                    "path": result["path"],
                    "sha256": result["sha256"],
                    "byte_count": result["byte_count"],
                    "width": result["width"],
                    "height": result["height"]
                }
                for role, result in record["presentation_gate"]["captures"].items()
            }
            eligible.append({
                "run_index": record["run_index"],
                "trial_id": record["trial_id"],
                "workspace": record["artifacts"]["workspace"],
                "workspace_sha256": record["workspace_sha256"],
                "captures": captures
            })
    review_target = eligible[0] if eligible else None
    summary = {
        "schema_version": 2,
        "kind": "trace2d_b2_nonscored_acceptance_v2_initial_summary",
        "benchmark_id": "trace2d-b2",
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
                "metrics": record["metrics"]
            }
            for record in records
        ],
        "eligible_trials": eligible,
        "review_target": review_target,
        "preflight": preflight,
        "next_required_phase": "perceptual_review" if review_target else "acceptance_v2_failed_no_reviewable_candidate",
        "scored_cohort_unchanged": True,
        "acceptance_v1_unchanged": True
    }
    write_json(root / INITIAL_SUMMARY, summary)
    return summary


def validate_review_payload(payload: dict[str, Any], summary: dict[str, Any], contract: dict[str, Any], *, final: bool = False) -> dict[str, Any]:
    target = summary.get("review_target")
    if not isinstance(target, dict):
        raise AcceptanceV2Error("no acceptance-v2 review target exists")
    if payload.get("reviewer_agent") != "ChatGPT" or payload.get("model") != "GPT-5.6 Sol":
        raise AcceptanceV2Error("perceptual reviewer identity drifted")
    if payload.get("target_trial_id") != target.get("trial_id"):
        raise AcceptanceV2Error("perceptual review target drifted")
    expected_hashes = {role: value["sha256"] for role, value in target["captures"].items()}
    if payload.get("capture_sha256s") != expected_hashes:
        raise AcceptanceV2Error("perceptual review capture hashes drifted")
    rubric = payload.get("rubric")
    required = contract["perceptual_review"]["rubric"]
    if not isinstance(rubric, dict) or list(rubric) != required:
        raise AcceptanceV2Error("perceptual rubric shape drifted")
    if not all(isinstance(rubric[name], bool) for name in required):
        raise AcceptanceV2Error("perceptual rubric values must be boolean")
    passed = all(rubric.values())
    if payload.get("passed") is not passed:
        raise AcceptanceV2Error("perceptual pass flag must equal rubric conjunction")
    findings = payload.get("findings")
    if not isinstance(findings, list) or not findings or not all(isinstance(item, str) and item.strip() for item in findings):
        raise AcceptanceV2Error("perceptual review requires findings")
    recommendation = payload.get("recommendation")
    if not isinstance(recommendation, str) or not recommendation.strip():
        raise AcceptanceV2Error("perceptual review requires recommendation")
    if final and not passed:
        raise AcceptanceV2Error("final perceptual confirmation must pass all rubric items")
    return payload


def record_review(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    summary = load_json(root / INITIAL_SUMMARY)
    review_path = root / PERCEPTUAL_REVIEW
    if review_path.exists():
        raise AcceptanceV2Error("initial perceptual review already recorded")
    payload = validate_review_payload(load_json(Path(args.review_file).expanduser().resolve()), summary, contract)
    review = {
        "schema_version": 2,
        "kind": "trace2d_b2_nonscored_acceptance_v2_perceptual_review",
        "scored": False,
        "recorded_at": v1.utc_now(),
        **payload,
        "may_override_deterministic_failure": False,
        "candidate_modified": False
    }
    write_json(review_path, review)
    return review


def build_revision_prompt(feedback_text: str, review: dict[str, Any]) -> str:
    return f"""# B2 non-scored acceptance-v2: one human-feedback revision

This retained Trace2D candidate already passed the independent deterministic
gameplay verifier and the acceptance-v2 machine presentation gate.

Preserve every deterministic gameplay semantic and every presentation-v2
requirement. Do not change benchmark/verifier/harness files and do not add
acceptance-only shortcuts.

## Perceptual review recommendation

{review.get('recommendation', '')}

## Human feedback

--- BEGIN HUMAN FEEDBACK ---
{feedback_text}
--- END HUMAN FEEDBACK ---

Apply exactly this presentation/usability feedback while preserving gameplay.
Regenerate all four required acceptance-v2 captures:
`ember-hall-overview.png`, `ember-hall-attack.png`, `ember-hall-hit.png`, and
`ember-hall-death.png`.
"""


def feedback(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if (root / REVISION_RECORD).exists():
        raise AcceptanceV2Error("acceptance-v2 feedback cycle already consumed")
    summary = load_json(root / INITIAL_SUMMARY)
    target = summary.get("review_target")
    if not isinstance(target, dict):
        raise AcceptanceV2Error("no acceptance-v2 review target")
    review = load_json(root / PERCEPTUAL_REVIEW)
    if review.get("passed") is not True:
        raise AcceptanceV2Error("human feedback is gated on a passing initial perceptual review")
    feedback_text = Path(args.feedback_file).expanduser().resolve().read_text(encoding="utf-8").strip()
    if not feedback_text or len(feedback_text) > 4000:
        raise AcceptanceV2Error("human feedback must be non-empty and <=4000 characters")
    workspace = Path(str(target["workspace"])).resolve()
    before_hash = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    if before_hash != target.get("workspace_sha256"):
        raise AcceptanceV2Error("review-target workspace changed before feedback")

    revision_root = root / "revision"
    revision_root.mkdir(parents=True, exist_ok=False)
    feedback_copy = revision_root / "human-feedback.txt"
    feedback_copy.write_text(feedback_text + "\n", encoding="utf-8")
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
        task_env_id=f"{ACCEPTANCE_TASK_ID}-human-revision-v1"
    )
    (revision_root / "adapter.stdout.txt").write_text(str(process.get("stdout", "")), encoding="utf-8")
    (revision_root / "adapter.stderr.txt").write_text(str(process.get("stderr", "")), encoding="utf-8")
    agent_result = v1.load_optional_result(result_path)
    identity_ok = v1.agent_identity_ok(agent_result, v1.load_profile())
    verifier_process, verifier = v1.run_verifier(workspace, verifier_path, 401)
    (revision_root / "verifier.stdout.txt").write_text(str(verifier_process.get("stdout", "")), encoding="utf-8")
    (revision_root / "verifier.stderr.txt").write_text(str(verifier_process.get("stderr", "")), encoding="utf-8")
    gate = presentation_gate(workspace, contract)
    after_hash = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    freeze_valid = True
    try:
        benchmark_b2_execution_freeze.validate_repository(REPO_ROOT)
    except Exception:
        freeze_valid = False
    machine_success = identity_ok and v1.deterministic_pass(verifier) and gate["passed"] and after_hash != before_hash and freeze_valid
    record = {
        "schema_version": 2,
        "kind": "trace2d_b2_nonscored_acceptance_v2_revision",
        "benchmark_id": "trace2d-b2",
        "acceptance_id": ACCEPTANCE_TASK_ID,
        "scored": False,
        "phase": "human_feedback_revision",
        "target_trial_id": target["trial_id"],
        "started_at": started_at,
        "finished_at": v1.utc_now(),
        "status": "awaiting_final_perceptual_confirmation" if machine_success else "revision_failed_acceptance_v2",
        "agent_identity_ok": identity_ok,
        "human_feedback": {
            "events": 1,
            "revision_cycles": 1,
            "text": feedback_text,
            "sha256": v1.sha256_text(feedback_text),
            "source": "real owner feedback relayed from ChatGPT conversation"
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
            "scored_freeze_valid_after": freeze_valid,
            "human_feedback_events": 1,
            "feedback_revision_cycles": 1
        },
        "machine_acceptance_passed": machine_success,
        "full_loop_passed": False
    }
    write_json(root / REVISION_RECORD, record)

    if machine_success:
        revised_captures = {
            role: {
                "path": result["path"],
                "sha256": result["sha256"],
                "byte_count": result["byte_count"],
                "width": result["width"],
                "height": result["height"]
            }
            for role, result in gate["captures"].items()
        }
        write_json(root / "final-review-target.json", {
            "schema_version": 2,
            "kind": "trace2d_b2_nonscored_acceptance_v2_final_review_target",
            "scored": False,
            "review_target": {
                "trial_id": target["trial_id"],
                "workspace": str(workspace),
                "captures": revised_captures
            }
        })
    return record


def record_final_review(args: argparse.Namespace) -> dict[str, Any]:
    contract = validate_contract()
    root = require_acceptance_root(args.runs_root)
    if (root / FINAL_REVIEW).exists():
        raise AcceptanceV2Error("final perceptual review already recorded")
    revision = load_json(root / REVISION_RECORD)
    if revision.get("machine_acceptance_passed") is not True:
        raise AcceptanceV2Error("final review requires passing revision machine evidence")
    target_doc = load_json(root / "final-review-target.json")
    summary = {"review_target": target_doc["review_target"]}
    payload = validate_review_payload(load_json(Path(args.review_file).expanduser().resolve()), summary, contract, final=True)
    final_review = {
        "schema_version": 2,
        "kind": "trace2d_b2_nonscored_acceptance_v2_final_perceptual_review",
        "scored": False,
        "recorded_at": v1.utc_now(),
        **payload,
        "final_confirmation": True
    }
    write_json(root / FINAL_REVIEW, final_review)
    revision["status"] = "accepted"
    revision["full_loop_passed"] = True
    revision["final_perceptual_confirmation"] = {
        "passed": True,
        "recorded_at": final_review["recorded_at"],
        "rubric": final_review["rubric"]
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
        "acceptance_version": 2,
        "scored": False,
        "contract_sha256": v1.sha256_file(REPO_ROOT / CONTRACT_PATH),
        "task_prompt_sha256": contract["task_prompt_sha256"],
        "initial": optional(INITIAL_SUMMARY),
        "perceptual_review": optional(PERCEPTUAL_REVIEW),
        "revision": revision,
        "final_perceptual_review": final_review,
        "full_loop_passed": bool(revision and revision.get("full_loop_passed") is True and final_review and final_review.get("passed") is True),
        "scored_cohort_unchanged": True,
        "acceptance_v1_unchanged": True
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D B2 non-scored acceptance-v2 harness")
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate-contract")
    validate.set_defaults(handler=lambda _: validate_contract())
    preflight = commands.add_parser("preflight")
    preflight.add_argument("--runs-root", required=True)
    preflight.set_defaults(handler=lambda args: {
        "contract": validate_contract(),
        "root": str(require_acceptance_root(args.runs_root)),
        "environment": scored.preflight_environment(LANE)
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
    final_review = commands.add_parser("record-final-review")
    final_review.add_argument("--runs-root", required=True)
    final_review.add_argument("--review-file", required=True)
    final_review.set_defaults(handler=record_final_review)
    state = commands.add_parser("status")
    state.add_argument("--runs-root", required=True)
    state.set_defaults(handler=status)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        result = args.handler(args)
    except (
        AcceptanceV2Error,
        v1.AcceptanceError,
        scored.B2HarnessError,
        OSError,
        subprocess.SubprocessError,
        ValueError,
        zlib.error
    ) as exc:
        print(f"B2 acceptance-v2 harness error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
