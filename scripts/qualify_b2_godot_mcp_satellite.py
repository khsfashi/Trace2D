#!/usr/bin/env python3
"""Non-scored B2 qualification for satelliteoflove/godot-mcp.

The scored B2 combat prompt is intentionally not visible here. This driver uses
only the generic gameplay-loop fixture frozen for baseline qualification.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import subprocess
from typing import Any

import qualify_godot_agent_mcp as base
import qualify_godot_agent_mcp_live as b0_live
from qualify_b2_godot_agent_loop import prove_independent_good_and_bad

EXPECTED_BRIDGE_VERSION = "4.1.0"
EXPECTED_SOURCE_COMMIT = "1b7d40537240fd54300f54bf6fda1ea91f06c878"
EXPECTED_GODOT_VERSION = "4.7.1-stable"
PROBE_PATH = "/root/QualificationRoot/Probe"
PROBE_GROUP = "qualification_probe"
MOVE_ACTION = "qualification_move"
TARGET_TICKS = 8
EXPECTED_START_X = 32.0
EXPECTED_END_X = 48.0

REQUIRED_TOOLS = {
    "godot_editor_read",
    "godot_editor_edit",
    "godot_scene",
    "godot_node_read",
    "godot_node_edit",
    "godot_project",
    "godot_input",
    "godot_runtime_state",
    "godot_game_time",
}


def json_text(value: Any) -> str:
    try:
        return json.dumps(value, sort_keys=True, ensure_ascii=False)
    except TypeError:
        return repr(value)


def find_probe_state(value: Any) -> dict[str, Any] | None:
    if isinstance(value, dict):
        if value.get("semantic_id") == "qualification_probe":
            return value
        for child in value.values():
            found = find_probe_state(child)
            if found is not None:
                return found
    elif isinstance(value, list):
        for child in value:
            found = find_probe_state(child)
            if found is not None:
                return found
    return None


def probe_snapshot(client: base.McpClient) -> dict[str, Any]:
    digest = client.call_tool(
        "godot_runtime_state",
        {
            "action": "digest",
            "select": "group",
            "group": PROBE_GROUP,
            "max_nodes": 4,
        },
    )
    state = find_probe_state(digest)
    base.require(state is not None, f"qualification probe state not found: {digest!r}")
    return state


def run_live_candidate(args: argparse.Namespace) -> dict[str, Any]:
    client = base.McpClient(args.server, args.mcp_stderr)
    live_started = False
    try:
        init = client.initialize()
        server_info = init.get("serverInfo", {}) if isinstance(init, dict) else {}
        version = str(server_info.get("version", "unknown")) if isinstance(server_info, dict) else "unknown"
        base.require(version == EXPECTED_BRIDGE_VERSION, f"unexpected satellite bridge version: {version}")

        tools = client.list_tools()
        missing = sorted(REQUIRED_TOOLS - tools)
        base.require(not missing, f"satellite bridge is missing required tools: {missing}")

        editor_state = base.wait_for_editor(client)
        base.require(
            str(editor_state.get("godot_version", "")).startswith("4.7.1"),
            f"editor is not the pinned Godot 4.7.1 build: {editor_state!r}",
        )
        client.call_tool("godot_scene", {"action": "open", "scene_path": "res://main.tscn"})

        # Normal editor authoring probe: persist one typed node property, read it
        # back, then restore it before runtime qualification.
        before_props = client.call_tool(
            "godot_node_read",
            {"action": "get_properties", "node_path": PROBE_PATH},
        )
        base.require(isinstance(before_props, dict), "Probe properties were not structured")
        original_z = before_props.get("z_index")
        base.require(isinstance(original_z, int), f"unexpected Probe z_index: {original_z!r}")
        probe_z = original_z + 5
        client.call_tool(
            "godot_node_edit",
            {"action": "update", "node_path": PROBE_PATH, "properties": {"z_index": probe_z}},
        )
        client.call_tool("godot_scene", {"action": "save"})
        edited_props = client.call_tool(
            "godot_node_read",
            {"action": "get_properties", "node_path": PROBE_PATH},
        )
        base.require(
            isinstance(edited_props, dict) and edited_props.get("z_index") == probe_z,
            "satellite authoring mutation did not persist",
        )
        client.call_tool(
            "godot_node_edit",
            {"action": "update", "node_path": PROBE_PATH, "properties": {"z_index": original_z}},
        )
        client.call_tool("godot_scene", {"action": "save"})
        restored_props = client.call_tool(
            "godot_node_read",
            {"action": "get_properties", "node_path": PROBE_PATH},
        )
        base.require(
            isinstance(restored_props, dict) and restored_props.get("z_index") == original_z,
            "satellite authoring restore failed",
        )

        _, frozen_status = b0_live.start_frozen_run(client)
        live_started = True

        input_map = client.call_tool("godot_input", {"action": "get_map"})
        base.require(
            MOVE_ACTION in json_text(input_map),
            f"semantic action {MOVE_ACTION!r} was not discoverable in the running InputMap: {input_map!r}",
        )

        initial = probe_snapshot(client)
        base.require(
            base.as_number(initial.get("active_ticks"), "initial.active_ticks") == 0.0,
            f"frozen run did not start at zero active ticks: {initial!r}",
        )
        base.require(
            abs(base.as_number(initial.get("position_x"), "initial.position_x") - EXPECTED_START_X) <= 1e-9,
            f"unexpected frozen initial position: {initial!r}",
        )

        until = f'tree.get_nodes_in_group("{PROBE_GROUP}")[0].active_ticks >= {TARGET_TICKS}'
        step = client.call_tool(
            "godot_game_time",
            {
                "action": "step_until",
                "until": until,
                "max_ms": 1000,
                "report": [
                    f'tree.get_nodes_in_group("{PROBE_GROUP}")[0].active_ticks',
                    f'tree.get_nodes_in_group("{PROBE_GROUP}")[0].position.x',
                ],
                "inputs": [
                    {
                        "action_name": MOVE_ACTION,
                        "start_ms": 0,
                        "duration_ms": 1000,
                    }
                ],
            },
            timeout=45.0,
        )
        base.require(isinstance(step, dict) and step.get("completed") is True, f"bounded step did not complete: {step!r}")
        base.require(step.get("predicate_met") is True, f"semantic input never reached the target state: {step!r}")

        final = probe_snapshot(client)
        base.require(
            base.as_number(final.get("active_ticks"), "final.active_ticks") == float(TARGET_TICKS),
            f"deterministic input boundary overshot target ticks: {final!r}",
        )
        base.require(
            abs(base.as_number(final.get("position_x"), "final.position_x") - EXPECTED_END_X) <= 1e-9,
            f"semantic input produced wrong deterministic movement: {final!r}",
        )

        screenshot = client.call_tool(
            "godot_editor_read",
            {"action": "screenshot_game", "max_width": 320},
            timeout=30.0,
        )
        base.require(screenshot is not None, "screenshot_game returned no presentation evidence")
        editor_errors = client.call_tool(
            "godot_editor_read",
            {"action": "get_log_messages", "severity": "error", "limit": 20},
        )

        b0_live.stop_run(client)
        live_started = False

        return {
            "initialize": init,
            "advertised_tools": sorted(tools),
            "editor_state": editor_state,
            "authoring": {
                "property": "z_index",
                "before": original_z,
                "edited": probe_z,
                "restored": restored_props.get("z_index"),
            },
            "frozen_status": frozen_status,
            "input_map": input_map,
            "initial_state": initial,
            "step": step,
            "final_state": final,
            "screenshot": screenshot,
            "editor_errors": editor_errors,
        }
    finally:
        if live_started:
            try:
                b0_live.stop_run(client)
            except Exception:
                pass
        client.close()


def run_qualification(args: argparse.Namespace) -> dict[str, Any]:
    godot_env = os.environ.get("GODOT_PATH", "")
    base.require(bool(godot_env), "GODOT_PATH must identify the pinned official Godot binary")
    godot = Path(godot_env).resolve()
    base.require(godot.is_file(), f"GODOT_PATH does not exist: {godot}")

    source_hash_before = base.tree_sha256(args.source_fixture)
    installed_hash_before = base.tree_sha256(args.project)
    independent = prove_independent_good_and_bad(godot, args.source_fixture, args.source_fixture)
    live = run_live_candidate(args)
    source_hash_after = base.tree_sha256(args.source_fixture)
    base.require(source_hash_after == source_hash_before, "candidate changed retained repository fixture source")

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
            "id": "satelliteoflove/godot-mcp",
            "package": "@satelliteoflove/godot-mcp@4.1.0",
            "version": EXPECTED_BRIDGE_VERSION,
            "source_commit": EXPECTED_SOURCE_COMMIT,
            "npm_integrity": os.environ.get("TRACE2D_B2_SATELLITE_INTEGRITY", "unknown"),
        },
        "engine": {
            "id": "godot",
            "version": EXPECTED_GODOT_VERSION,
            "reported_version": version_output.stdout.strip(),
        },
        "checks": {
            "host_file_authoring_surface": True,
            "editor_typed_authoring": True,
            "semantic_input_configuration": True,
            "running_game_launch": True,
            "structured_runtime_observation": True,
            "player_like_input": True,
            "deterministic_bounded_step": True,
            "presentation_capture": True,
            "independent_known_good_acceptance": True,
            "independent_known_bad_rejection": True,
            "retained_source_unchanged": True,
            "privileged_reflection_or_code_execution_required": False,
        },
        "fixture": {
            "source_tree_sha256_before": source_hash_before,
            "source_tree_sha256_after": source_hash_after,
            "installed_tree_sha256_before_live": installed_hash_before,
            "installed_tree_sha256_after_live": base.tree_sha256(args.project),
        },
        "independent_verifier": independent,
        "live": live,
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
        print(f"B2 satellite Godot MCP qualification failed: {exc}", flush=True)
        return 1
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
