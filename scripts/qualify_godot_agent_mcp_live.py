#!/usr/bin/env python3
"""Hosted live qualification for the Benchmark B0 godot.agent lane.

Protocol plumbing lives in qualify_godot_agent_mcp.py. Deterministic replay is
bounded by authoritative fixture physics ticks through the bridge's public
step_until control. Render frames and fixed millisecond windows are retained as
observations, but neither is used as the equality boundary for fixed-physics
replay on an uncapped hosted runner.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import qualify_godot_agent_mcp as base


TARGET_PHYSICS_TICKS = 12
STEP_UNTIL = f'tree.get_nodes_in_group("mcp_watch")[0].physics_ticks >= {TARGET_PHYSICS_TICKS}'
STEP_REPORT = [
    'tree.get_nodes_in_group("mcp_watch")[0].physics_ticks',
    'tree.get_nodes_in_group("mcp_watch")[0].position.x',
]
# Keep D held beyond the safety window; step_until releases all holds when the
# predicate ends, so input duration is not the stop condition.
STEP_INPUT = [{"key": "d", "start_ms": 0, "duration_ms": 1000}]


def wait_for_play_state(client: base.McpClient, expected: bool, timeout: float = 45.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last: Any = None
    while time.monotonic() < deadline:
        try:
            state = client.call_tool("godot_editor_read", {"action": "get_state"}, timeout=15.0)
            last = state
            if isinstance(state, dict) and state.get("is_playing") is expected:
                return state
        except base.QualificationError as exc:
            last = str(exc)
        time.sleep(0.25)
    raise base.QualificationError(f"editor play state did not become {expected}: {last!r}")


def wait_for_frozen_game(client: base.McpClient, timeout: float = 45.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last: Any = None
    while time.monotonic() < deadline:
        try:
            status = client.call_tool("godot_game_time", {"action": "status"}, timeout=15.0)
            last = status
            if isinstance(status, dict) and status.get("frozen") is True and status.get("launched_frozen") is True:
                return status
        except base.QualificationError as exc:
            last = str(exc)
        time.sleep(0.25)
    raise base.QualificationError(f"game never reached launch-frozen state: {last!r}")


def start_frozen_run(client: base.McpClient) -> tuple[dict[str, Any], dict[str, Any]]:
    client.call_tool("godot_editor_edit", {"action": "run", "frozen": True}, timeout=45.0)
    editor_state = wait_for_play_state(client, True)
    frozen_status = wait_for_frozen_game(client)
    return editor_state, frozen_status


def stop_run(client: base.McpClient) -> None:
    client.call_tool("godot_editor_edit", {"action": "stop"}, timeout=15.0)
    wait_for_play_state(client, False)


def exact_tick_step(client: base.McpClient) -> tuple[dict[str, Any], dict[str, Any]]:
    step = client.call_tool(
        "godot_game_time",
        {
            "action": "step_until",
            "until": STEP_UNTIL,
            "max_ms": 1000,
            "report": STEP_REPORT,
            "inputs": STEP_INPUT,
        },
        timeout=45.0,
    )
    state = base.player_snapshot(client)
    base.require(isinstance(step, dict) and step.get("completed") is True, f"step_until did not complete: {step!r}")
    base.require(step.get("predicate_met") is True, f"step_until missed physics tick predicate: {step!r}")
    input_kinds = step.get("input_kinds", {})
    base.require(
        isinstance(input_kinds, dict) and base.as_number(input_kinds.get("key", 0), "step.input_kinds.key") >= 1,
        "bridge did not report raw key injection",
    )
    base.require(
        base.as_number(state.get("physics_ticks"), "state.physics_ticks") == float(TARGET_PHYSICS_TICKS),
        f"bridge did not freeze on exact target physics tick {TARGET_PHYSICS_TICKS}: {state!r}; step={step!r}",
    )
    return step, state


def run_qualification(args: argparse.Namespace) -> dict[str, Any]:
    source_hash = base.tree_sha256(args.source_fixture)
    installed_hash_before = base.tree_sha256(args.project)
    client = base.McpClient(args.server, args.mcp_stderr)
    try:
        init = client.initialize()
        server_info = init.get("serverInfo", {}) if isinstance(init, dict) else {}
        tools = client.list_tools()
        required_tools = {
            "godot_editor_read",
            "godot_editor_edit",
            "godot_scene",
            "godot_node_read",
            "godot_node_edit",
            "godot_runtime_state",
            "godot_game_time",
        }
        missing = sorted(required_tools - tools)
        base.require(not missing, f"selected bridge is missing required tools: {missing}")

        editor_state = base.wait_for_editor(client)
        client.call_tool("godot_scene", {"action": "open", "scene_path": "res://main.tscn"})

        # Q1 — real editor authoring, saved/read back, then restored.
        before_props = client.call_tool("godot_node_read", {"action": "get_properties", "node_path": base.PLAYER_PATH})
        base.require(isinstance(before_props, dict), "Player properties were not structured")
        original_z = before_props.get("z_index")
        base.require(isinstance(original_z, int), f"unexpected Player z_index: {original_z!r}")
        probe_z = original_z + 7
        client.call_tool(
            "godot_node_edit",
            {"action": "update", "node_path": base.PLAYER_PATH, "properties": {"z_index": probe_z}},
        )
        client.call_tool("godot_scene", {"action": "save"})
        edited_props = client.call_tool("godot_node_read", {"action": "get_properties", "node_path": base.PLAYER_PATH})
        base.require(isinstance(edited_props, dict) and edited_props.get("z_index") == probe_z, "Q1 authoring edit did not persist")
        client.call_tool(
            "godot_node_edit",
            {"action": "update", "node_path": base.PLAYER_PATH, "properties": {"z_index": original_z}},
        )
        client.call_tool("godot_scene", {"action": "save"})
        restored_props = client.call_tool("godot_node_read", {"action": "get_properties", "node_path": base.PLAYER_PATH})
        base.require(isinstance(restored_props, dict) and restored_props.get("z_index") == original_z, "Q1 fixture restore failed")

        # Q2 — frame-zero freeze plus wall-clock independence.
        _, first_frozen_status = start_frozen_run(client)
        initial = base.player_snapshot(client)
        base.require(initial.get("semantic_id") == "player", f"Q2 wrong semantic id: {initial!r}")
        base.require(abs(base.as_number(initial.get("position_x"), "initial.position_x")) <= 1e-9, f"Q2 initial x moved: {initial!r}")
        base.require(abs(base.as_number(initial.get("position_y"), "initial.position_y")) <= 1e-9, f"Q2 initial y moved: {initial!r}")
        base.require(base.as_number(initial.get("physics_ticks"), "initial.physics_ticks") == 0.0, f"Q2 frame-zero freeze failed: {initial!r}")
        time.sleep(0.5)
        after_wall_wait = base.player_snapshot(client)
        base.require(after_wall_wait.get("physics_ticks") == initial.get("physics_ticks"), "Q2 wall time advanced physics ticks")
        base.require(after_wall_wait.get("position_x") == initial.get("position_x"), "Q2 wall time moved Player")

        # Q3 — real raw D key while the bridge advances until the fixture's
        # authoritative physics counter reaches exactly the frozen target.
        step_one, first = exact_tick_step(client)
        base.require(base.as_number(first.get("position_x"), "first.position_x") > 0.0, f"Q3 D input did not move Player: {first!r}")

        # Q4 — fully stop the first debug session, start a clean frozen run,
        # wait in wall time, then hit the same authoritative physics boundary.
        stop_run(client)
        time.sleep(0.25)
        _, replay_frozen_status = start_frozen_run(client)
        replay_initial = base.player_snapshot(client)
        base.require(base.as_number(replay_initial.get("physics_ticks"), "replay_initial.physics_ticks") == 0.0, f"Q4 replay did not restart at frame zero: {replay_initial!r}")
        time.sleep(0.75)
        replay_after_wait = base.player_snapshot(client)
        base.require(replay_after_wait.get("physics_ticks") == replay_initial.get("physics_ticks"), "Q4 replay wall wait advanced physics ticks")
        step_two, second = exact_tick_step(client)
        base.require(first.get("physics_ticks") == second.get("physics_ticks") == TARGET_PHYSICS_TICKS, f"Q4 physics tick replay mismatch: {first!r} vs {second!r}")
        base.require(abs(base.as_number(first.get("position_x"), "first.position_x") - base.as_number(second.get("position_x"), "second.position_x")) <= 1e-9, f"Q4 position replay mismatch: {first!r} vs {second!r}")
        base.require(step_one.get("physics_ticks") == step_two.get("physics_ticks"), f"Q4 bridge physics tick count mismatch: {step_one!r} vs {step_two!r}")
        stop_run(client)

        return {
            "schema_version": 1,
            "suite_id": "trace2d-b0",
            "lane_id": "godot.agent",
            "qualified": True,
            "engine": {
                "id": "godot",
                "version": "4.7.1-stable",
                "reported_version": editor_state.get("godot_version"),
            },
            "bridge": {
                "id": "satelliteoflove/godot-mcp",
                "version": str(server_info.get("version", "unknown")),
                "npm_package": "@satelliteoflove/godot-mcp@4.1.0",
                "npm_integrity": os.environ.get("TRACE2D_B0_GODOT_MCP_INTEGRITY", "unknown"),
            },
            "checks": {
                "authoring": True,
                "runtime_inspection": True,
                "timed_input": True,
                "deterministic_step": True,
            },
            "determinism_boundary": {
                "kind": "fixture_physics_ticks",
                "target": TARGET_PHYSICS_TICKS,
                "predicate": STEP_UNTIL,
                "max_game_time_ms": 1000,
                "render_frames_authoritative": False,
                "fixed_milliseconds_authoritative": False,
            },
            "environment": {
                "os": platform.platform(),
                "architecture": platform.machine(),
                "node_version": os.environ.get("TRACE2D_B0_NODE_VERSION", "unknown"),
                "mcp_client": "scripts/qualify_godot_agent_mcp_live.py@2",
                "runner_image": os.environ.get("ImageOS", "unknown"),
            },
            "fixture": {
                "source_tree_sha256": source_hash,
                "installed_tree_sha256_before_qualification": installed_hash_before,
                "installed_tree_sha256_after_qualification": base.tree_sha256(args.project),
            },
            "observations": {
                "authoring": {"property": "z_index", "before": original_z, "edited": probe_z, "restored": restored_props.get("z_index")},
                "first_frozen_status": first_frozen_status,
                "runtime_initial": initial,
                "runtime_after_wall_wait": after_wall_wait,
                "step_one": step_one,
                "runtime_after_step_one": first,
                "replay_frozen_status": replay_frozen_status,
                "replay_initial": replay_initial,
                "replay_after_wall_wait": replay_after_wait,
                "step_two": step_two,
                "runtime_after_step_two": second,
            },
            "evidence": [
                "GitHub Actions godot-agent-qualification job log",
                "godot-agent-qualification-evidence workflow artifact",
            ],
            "generated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        }
    finally:
        try:
            client.call_tool("godot_editor_edit", {"action": "stop"}, timeout=5.0)
        except Exception:
            pass
        client.close()


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
        print(f"godot.agent qualification failed: {exc}", flush=True)
        return 1
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
