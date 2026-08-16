#!/usr/bin/env python3
"""Non-scored live qualification for the B2 Godot Agent Loop candidate.

This fixture is intentionally distinct from the frozen B2 scored task. It proves
that the selected candidate can participate in a normal coding-agent loop:
host-file authoring, launch, semantic input, structured observation,
presentation capture, bounded verification, and clean teardown. A separate
Trace2D-owned Godot verifier supplies known-good/known-bad acceptance evidence.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
from typing import Any

import qualify_godot_agent_mcp as base

EXPECTED_BRIDGE_VERSION = "3.0.0"
EXPECTED_SOURCE_COMMIT = "7bc6062f90e2f96a04f997b202f7a24dd152a9fd"
EXPECTED_GODOT_VERSION = "4.7.1-stable"
MOVE_ACTION = "qualification_move"
PROBE_PATH = "/root/QualificationRoot/Probe"

REQUIRED_CORE_TOOLS = {
    "godot_catalog",
    "godot_call",
    "run_project",
    "stop_project",
    "game_screenshot",
    "game_get_scene_tree",
    "game_get_ui",
    "game_get_node_info",
    "game_get_errors",
    "game_get_logs",
    "game_scenario",
    "game_wait_until",
    "run_project_tests",
    "verify_project",
}


def json_text(value: Any) -> str:
    try:
        return json.dumps(value, sort_keys=True, ensure_ascii=False)
    except TypeError:
        return repr(value)


def find_mapping_with_key(value: Any, key: str) -> dict[str, Any] | None:
    if isinstance(value, dict):
        if key in value:
            return value
        for child in value.values():
            found = find_mapping_with_key(child, key)
            if found is not None:
                return found
    elif isinstance(value, list):
        for child in value:
            found = find_mapping_with_key(child, key)
            if found is not None:
                return found
    return None


def find_named_value(value: Any, name: str) -> Any:
    if isinstance(value, dict):
        if value.get("name") == name and "value" in value:
            return value.get("value")
        if name in value:
            return value.get(name)
        for child in value.values():
            found = find_named_value(child, name)
            if found is not None:
                return found
    elif isinstance(value, list):
        for child in value:
            found = find_named_value(child, name)
            if found is not None:
                return found
    return None


def find_existing_artifact(value: Any) -> Path | None:
    path_keys = {"artifact_path", "artifactPath", "path", "file", "file_path", "filePath"}
    if isinstance(value, dict):
        for key, child in value.items():
            if key in path_keys and isinstance(child, str):
                candidate = Path(child)
                if candidate.is_file() and candidate.suffix.lower() == ".png":
                    return candidate
        for child in value.values():
            found = find_existing_artifact(child)
            if found is not None:
                return found
    elif isinstance(value, list):
        for child in value:
            found = find_existing_artifact(child)
            if found is not None:
                return found
    return None


def run_independent_verifier(godot: Path, project: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            str(godot),
            "--headless",
            "--path",
            str(project),
            "--script",
            "res://qualification_verify.gd",
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=45,
        check=False,
    )


def prove_independent_good_and_bad(godot: Path, project: Path, source_fixture: Path) -> dict[str, Any]:
    good = run_independent_verifier(godot, project)
    base.require(good.returncode == 0, f"known-good independent verifier failed: {good.stdout}")
    base.require(
        "qualification-independent-verifier-pass" in good.stdout,
        f"known-good verifier did not emit its acceptance marker: {good.stdout}",
    )

    with tempfile.TemporaryDirectory(prefix="trace2d-b2-known-bad-") as temp_dir:
        bad_project = Path(temp_dir) / "project"
        shutil.copytree(source_fixture, bad_project)
        probe_path = bad_project / "probe.gd"
        source = probe_path.read_text(encoding="utf-8")
        needle = "const STEP_UNITS := 2.0"
        base.require(source.count(needle) == 1, "known-bad fixture mutation boundary drifted")
        probe_path.write_text(source.replace(needle, "const STEP_UNITS := 3.0"), encoding="utf-8")
        bad = run_independent_verifier(godot, bad_project)

    base.require(bad.returncode != 0, "known-bad independent verifier unexpectedly accepted the broken fixture")
    base.require(
        "qualification-independent-verifier-fail" in bad.stdout,
        f"known-bad verifier did not emit its rejection marker: {bad.stdout}",
    )
    return {
        "known_good": {
            "accepted": True,
            "exit_code": good.returncode,
            "marker": "qualification-independent-verifier-pass",
        },
        "known_bad": {
            "accepted": False,
            "exit_code": bad.returncode,
            "mutation": "probe STEP_UNITS 2.0 -> 3.0",
            "marker": "qualification-independent-verifier-fail",
        },
    }


def candidate_version(init: dict[str, Any]) -> str:
    server_info = init.get("serverInfo", {}) if isinstance(init, dict) else {}
    return str(server_info.get("version", "unknown")) if isinstance(server_info, dict) else "unknown"


def run_live_candidate(args: argparse.Namespace) -> tuple[dict[str, Any], Path | None]:
    client = base.McpClient(args.server, args.mcp_stderr)
    screenshot_copy: Path | None = None
    live_started = False
    try:
        init = client.initialize()
        version = candidate_version(init)
        base.require(version == EXPECTED_BRIDGE_VERSION, f"unexpected candidate version: {version}")

        tools = client.list_tools()
        missing = sorted(REQUIRED_CORE_TOOLS - tools)
        base.require(not missing, f"candidate core surface is missing required tools: {missing}")

        catalog = client.call_tool(
            "godot_catalog",
            {"action": "describe", "toolName": "game_input_action", "detail": "schema"},
        )
        base.require("add_action" in json_text(catalog), "semantic input action tool was not discoverable through catalog")

        started = client.call_tool(
            "run_project",
            {"projectPath": str(args.project), "timingMode": "deterministic"},
            timeout=60.0,
        )
        live_started = True

        scene_tree = client.call_tool("game_get_scene_tree", {})
        tree_text = json_text(scene_tree)
        base.require("QualificationRoot" in tree_text and "Probe" in tree_text, "running scene tree did not expose qualification nodes")

        ui = client.call_tool("game_get_ui", {})
        base.require("qualification" in json_text(ui).lower(), "running UI was not observable")

        add_action = client.call_tool(
            "godot_call",
            {
                "toolName": "game_input_action",
                "arguments": {
                    "action": "add_action",
                    "actionName": MOVE_ACTION,
                    "key": "RIGHT",
                },
            },
        )

        scenario = client.call_tool(
            "game_scenario",
            {
                "projectPath": str(args.project),
                "name": "Trace2D B2 non-scored semantic input probe",
                "timeoutSeconds": 20,
                "steps": [
                    {
                        "type": "input",
                        "tool": "game_input_action",
                        "arguments": {"action": "set_strength", "actionName": MOVE_ACTION, "strength": 1.0},
                        "label": "press semantic movement action",
                    },
                    {
                        "type": "wait",
                        "condition": {
                            "condition": "log",
                            "text": "qualification-input-8",
                            "fresh": True,
                            "timeoutSeconds": 10,
                        },
                        "label": "wait for gameplay to consume semantic input",
                    },
                    {
                        "type": "observe",
                        "tool": "game_get_node_info",
                        "arguments": {
                            "nodePath": PROBE_PATH,
                            "detail": "compact",
                            "propertyNames": ["position", "active_ticks"],
                        },
                        "label": "observe gameplay state",
                    },
                    {
                        "type": "input",
                        "tool": "game_input_action",
                        "arguments": {"action": "set_strength", "actionName": MOVE_ACTION, "strength": 0.0},
                        "label": "release semantic movement action",
                    },
                    {
                        "type": "assert",
                        "condition": {"condition": "node", "nodePath": PROBE_PATH},
                        "label": "probe remains observable",
                    },
                    {"type": "screenshot", "label": "capture presentation evidence"},
                ],
            },
            timeout=60.0,
        )
        scenario_pass = find_mapping_with_key(scenario, "passed")
        base.require(
            scenario_pass is not None and scenario_pass.get("passed") is True,
            f"compound semantic-input scenario did not pass: {scenario!r}",
        )

        node_info = client.call_tool(
            "game_get_node_info",
            {"nodePath": PROBE_PATH, "detail": "compact", "propertyNames": ["position", "active_ticks"]},
        )
        active_ticks = find_named_value(node_info, "active_ticks")
        if isinstance(active_ticks, (int, float)) and not isinstance(active_ticks, bool):
            base.require(active_ticks >= 8, f"semantic input did not reach eight gameplay ticks: {node_info!r}")
        else:
            base.require("active_ticks" in json_text(node_info), f"gameplay state did not expose active_ticks: {node_info!r}")

        screenshot = client.call_tool("game_screenshot", {"retainArtifact": True}, timeout=45.0)
        source_screenshot = find_existing_artifact(screenshot)
        if source_screenshot is not None:
            args.evidence.parent.mkdir(parents=True, exist_ok=True)
            screenshot_copy = args.evidence.parent / "godot-agent-loop-presentation.png"
            shutil.copy2(source_screenshot, screenshot_copy)

        errors = client.call_tool("game_get_errors", {})
        client.call_tool("stop_project", {}, timeout=30.0)
        live_started = False

        verified = client.call_tool(
            "verify_project",
            {
                "projectPath": str(args.project),
                "waitFrames": 2,
                "assertions": [
                    {"kind": "node_exists", "nodePath": PROBE_PATH},
                    {"kind": "group_count", "group": "qualification_probe", "count": 1},
                    {"kind": "log_contains", "text": "qualification-ready"},
                ],
                "captureScreenshot": True,
                "teardown": True,
            },
            timeout=60.0,
        )
        verify_pass = find_mapping_with_key(verified, "passed")
        if verify_pass is not None:
            base.require(verify_pass.get("passed") is True, f"candidate verify_project rejected known-good fixture: {verified!r}")
        else:
            base.require("false" not in json_text(verified).lower(), f"candidate verify_project reported failure: {verified!r}")

        return (
            {
                "initialize": init,
                "advertised_tools": sorted(tools),
                "catalog_game_input_action": catalog,
                "run_project": started,
                "scene_tree": scene_tree,
                "ui": ui,
                "add_semantic_action": add_action,
                "scenario": scenario,
                "post_scenario_node_info": node_info,
                "screenshot": screenshot,
                "runtime_errors": errors,
                "verify_project": verified,
            },
            screenshot_copy,
        )
    finally:
        if live_started:
            try:
                client.call_tool("stop_project", {}, timeout=5.0)
            except Exception:
                pass
        client.close()


def run_qualification(args: argparse.Namespace) -> dict[str, Any]:
    godot_env = os.environ.get("GODOT_PATH", "")
    base.require(bool(godot_env), "GODOT_PATH must identify the pinned official Godot binary")
    godot = Path(godot_env).resolve()
    base.require(godot.is_file(), f"GODOT_PATH does not exist: {godot}")

    source_hash = base.tree_sha256(args.source_fixture)
    installed_hash_before = base.tree_sha256(args.project)
    base.require(source_hash == installed_hash_before, "installed qualification fixture drifted before execution")

    independent = prove_independent_good_and_bad(godot, args.project, args.source_fixture)
    live, screenshot_copy = run_live_candidate(args)

    installed_hash_after = base.tree_sha256(args.project)
    base.require(installed_hash_after == installed_hash_before, "candidate mutated retained qualification source")

    version_output = subprocess.run(
        [str(godot), "--version"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=15,
        check=False,
    )
    base.require(version_output.returncode == 0, f"Godot version probe failed: {version_output.stdout}")
    base.require(version_output.stdout.startswith("4.7.1.stable"), f"unexpected Godot version: {version_output.stdout}")

    return {
        "schema_version": 1,
        "benchmark_id": "trace2d-b2",
        "qualification_id": "b2-gameplay-loop-qualification-v1",
        "scored": False,
        "qualified": True,
        "scored_task_prompt_exposed": False,
        "candidate": {
            "id": "beremaran/godot-agent-loop",
            "package": "@beremaran/godot-agent-loop@3.0.0",
            "version": EXPECTED_BRIDGE_VERSION,
            "source_commit": EXPECTED_SOURCE_COMMIT,
            "npm_integrity": os.environ.get("TRACE2D_B2_AGENT_LOOP_INTEGRITY", "unknown"),
        },
        "engine": {
            "id": "godot",
            "version": EXPECTED_GODOT_VERSION,
            "reported_version": version_output.stdout.strip(),
        },
        "checks": {
            "host_file_authoring_surface": True,
            "core_feedback_loop_surface": True,
            "semantic_input_configuration": True,
            "running_game_launch": True,
            "structured_runtime_observation": True,
            "player_like_input": True,
            "presentation_capture": True,
            "candidate_bounded_verification": True,
            "independent_known_good_acceptance": True,
            "independent_known_bad_rejection": True,
            "retained_source_unchanged": True,
            "privileged_reflection_or_code_execution_required": False,
        },
        "fixture": {
            "source_tree_sha256": source_hash,
            "installed_tree_sha256_before": installed_hash_before,
            "installed_tree_sha256_after": installed_hash_after,
        },
        "independent_verifier": independent,
        "live": live,
        "presentation_artifact": screenshot_copy.name if screenshot_copy is not None else None,
        "environment": {
            "os": platform.platform(),
            "architecture": platform.machine(),
            "node_version": os.environ.get("TRACE2D_B2_NODE_VERSION", "unknown"),
            "mcp_protocol": base.PROTOCOL_VERSION,
        },
        "generated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--source-fixture", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--mcp-stderr", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        evidence = run_qualification(args)
    except Exception as exc:
        print(f"B2 Godot Agent Loop qualification failed: {exc}", flush=True)
        return 1

    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
