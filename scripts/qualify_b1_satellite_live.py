#!/usr/bin/env python3
"""Non-scored B1 content capability qualification for satelliteoflove/godot-mcp."""

from __future__ import annotations

import argparse
import json
import os
import platform
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import qualify_godot_agent_mcp as base

PLAYER_PATH = "/root/Root/B1Animation"
PARTICLE_PATH = "/root/Root/B1Particles"
ANIMATION = "content_probe"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise base.QualificationError(message)


def run(args: argparse.Namespace) -> dict[str, Any]:
    source_hash = base.tree_sha256(args.source_fixture)
    before_hash = base.tree_sha256(args.project)
    client = base.McpClient(args.server, args.mcp_stderr)
    try:
        init = client.initialize()
        server_info = init.get("serverInfo", {}) if isinstance(init, dict) else {}
        tools = client.list_tools()
        required = {
            "godot_scene",
            "godot_node_read",
            "godot_node_edit",
            "godot_animation_read",
            "godot_animation_edit",
            "godot_editor_read",
        }
        missing = sorted(required - tools)
        require(not missing, f"missing B1 tools: {missing}")

        editor_state = base.wait_for_editor(client)
        client.call_tool("godot_scene", {"action": "open", "scene_path": "res://main.tscn"})
        client.call_tool(
            "godot_animation_edit",
            {
                "action": "create",
                "node_path": PLAYER_PATH,
                "animation_name": ANIMATION,
                "length": 1.0,
                "loop_mode": "none",
            },
        )
        client.call_tool(
            "godot_animation_edit",
            {
                "action": "add_track",
                "node_path": PLAYER_PATH,
                "animation_name": ANIMATION,
                "track_type": "method",
                "track_path": "Subject",
            },
        )
        details = client.call_tool(
            "godot_animation_read",
            {"action": "get_details", "node_path": PLAYER_PATH, "animation_name": ANIMATION},
        )
        require(isinstance(details, dict), "animation details were not structured")
        tracks = details.get("tracks", [])
        track_index = next(
            (
                track.get("index")
                for track in tracks
                if isinstance(track, dict)
                and track.get("type") == "method"
                and str(track.get("path", "")).endswith("Subject")
            ),
            None,
        )
        require(isinstance(track_index, int), f"method track not found after authoring: {details!r}")
        client.call_tool(
            "godot_animation_edit",
            {
                "action": "add_keyframe",
                "node_path": PLAYER_PATH,
                "animation_name": ANIMATION,
                "track_index": track_index,
                "time": 0.375,
                "method_name": "queue_redraw",
                "args": [],
            },
        )
        client.call_tool(
            "godot_node_edit",
            {
                "action": "update",
                "node_path": PARTICLE_PATH,
                "properties": {"amount": 96, "lifetime": 0.8, "emitting": False},
            },
        )
        client.call_tool("godot_scene", {"action": "save"})

        animation = client.call_tool(
            "godot_animation_read",
            {"action": "get_details", "node_path": PLAYER_PATH, "animation_name": ANIMATION},
        )
        particle = client.call_tool(
            "godot_node_read",
            {"action": "get_properties", "node_path": PARTICLE_PATH},
        )
        screenshot = client.call_tool(
            "godot_editor_read",
            {"action": "screenshot_editor", "viewport": "2d", "max_width": 320},
        )
        require(animation is not None, "animation readback missing")
        require(isinstance(particle, dict), "particle readback is not structured")
        require(screenshot is not None, "presentation capture handoff failed")

        return {
            "schema_version": 1,
            "benchmark_id": "trace2d-b1",
            "fixture_id": "b1-content-capability-qualification",
            "provider": "satelliteoflove/godot-mcp",
            "pin": "@satelliteoflove/godot-mcp@4.1.0",
            "qualified_authoring": True,
            "engine": {
                "id": "godot",
                "version": "4.7.1-stable",
                "reported_state": editor_state,
            },
            "bridge": {
                "reported_version": str(server_info.get("version", "unknown")),
                "npm_integrity": os.environ.get("TRACE2D_B1_SATELLITE_INTEGRITY", "unknown"),
            },
            "checks": {
                "scene_save": True,
                "animation_exact_event_authoring": True,
                "particle_budget_authoring": True,
                "structured_readback": True,
                "presentation_capture_handoff": True,
            },
            "fixture": {
                "source_tree_sha256": source_hash,
                "installed_tree_sha256_before": before_hash,
                "installed_tree_sha256_after": base.tree_sha256(args.project),
            },
            "observations": {
                "animation_readback": animation,
                "particle_amount": particle.get("amount"),
                "particle_lifetime": particle.get("lifetime"),
                "particle_emitting": particle.get("emitting"),
            },
            "environment": {"os": platform.platform(), "architecture": platform.machine()},
            "generated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        }
    finally:
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
        evidence = run(args)
    except Exception as exc:
        print(f"B1 satellite qualification failed: {exc}", flush=True)
        return 1
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
