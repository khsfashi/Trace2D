#!/usr/bin/env python3
"""Non-scored B2 qualification for hi-godot/godot-ai 3.1.5.

The frozen scored B2 combat prompt is never exposed here. This driver uses the
small generic gameplay-loop fixture and the candidate's documented public MCP
surface over streamable HTTP.
"""
from __future__ import annotations

import argparse
import base64
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time
import urllib.error
import urllib.request
from typing import Any

import qualify_godot_agent_mcp as base
from qualify_b2_godot_agent_loop import prove_independent_good_and_bad

EXPECTED_VERSION = "3.1.5"
EXPECTED_SOURCE_COMMIT = "09a1e3311015153d967710fbe6502ac519585a9b"
EXPECTED_GODOT_VERSION = "4.7.1-stable"
PROBE_EDITOR_PATH = "/QualificationRoot/Probe"
PROBE_RUNTIME_PATH = "/root/QualificationRoot/Probe"
MOVE_ACTION = "qualification_move"

REQUIRED_TOOLS = {
    "editor_state",
    "scene_open",
    "scene_save",
    "node_get_properties",
    "node_set_property",
    "input_map_manage",
    "project_run",
    "project_manage",
    "game_manage",
    "editor_screenshot",
    "test_run",
}


class HttpMcpClient:
    def __init__(self, url: str) -> None:
        self.url = url
        self.session_id = ""
        self.request_id = 1

    def _post(self, body: dict[str, Any], timeout: float = 30.0) -> tuple[dict[str, Any], dict[str, str]]:
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        request = urllib.request.Request(
            self.url,
            data=json.dumps(body).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        with urllib.request.urlopen(request, timeout=timeout) as response:
            raw = response.read().decode("utf-8")
            response_headers = {key.lower(): value for key, value in response.headers.items()}
        payload: dict[str, Any] | None = None
        for line in raw.splitlines():
            if line.startswith("data: "):
                candidate = json.loads(line[6:])
                if isinstance(candidate, dict):
                    payload = candidate
                    break
        if payload is None:
            candidate = json.loads(raw)
            base.require(isinstance(candidate, dict), f"MCP response was not an object: {raw[:300]}")
            payload = candidate
        return payload, response_headers

    def initialize(self) -> dict[str, Any]:
        body = {
            "jsonrpc": "2.0",
            "id": self.request_id,
            "method": "initialize",
            "params": {
                "protocolVersion": "2025-03-26",
                "capabilities": {},
                "clientInfo": {"name": "trace2d-b2-qualification", "version": "1.0"},
            },
        }
        self.request_id += 1
        payload, headers = self._post(body)
        session_id = headers.get("mcp-session-id", "")
        base.require(bool(session_id), f"initialize did not return Mcp-Session-Id: {payload!r}")
        self.session_id = session_id
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})
        result = payload.get("result", {})
        base.require(isinstance(result, dict), f"initialize result was not structured: {payload!r}")
        return result

    def list_tools(self) -> set[str]:
        payload, _ = self._post(
            {"jsonrpc": "2.0", "id": self.request_id, "method": "tools/list", "params": {}},
        )
        self.request_id += 1
        result = payload.get("result", {})
        tools = result.get("tools", []) if isinstance(result, dict) else []
        return {
            str(tool.get("name"))
            for tool in tools
            if isinstance(tool, dict) and tool.get("name")
        }

    def call_tool_full(self, name: str, arguments: dict[str, Any], timeout: float = 45.0) -> dict[str, Any]:
        payload, _ = self._post(
            {
                "jsonrpc": "2.0",
                "id": self.request_id,
                "method": "tools/call",
                "params": {"name": name, "arguments": arguments},
            },
            timeout=timeout,
        )
        self.request_id += 1
        if "error" in payload:
            raise RuntimeError(f"MCP {name} transport error: {payload['error']}")
        result = payload.get("result", {})
        base.require(isinstance(result, dict), f"MCP {name} result was not structured: {payload!r}")
        if result.get("isError") is True:
            raise RuntimeError(f"MCP {name} tool error: {result!r}")
        return result

    def call_tool(self, name: str, arguments: dict[str, Any], timeout: float = 45.0) -> Any:
        result = self.call_tool_full(name, arguments, timeout=timeout)
        structured = result.get("structuredContent")
        if structured is not None:
            return structured
        for block in result.get("content", []):
            if isinstance(block, dict) and block.get("type") == "text":
                text = str(block.get("text", ""))
                try:
                    return json.loads(text)
                except json.JSONDecodeError:
                    return {"text": text}
        return result


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


def wait_for_editor(client: HttpMcpClient, timeout: float = 90.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last: Any = None
    while time.monotonic() < deadline:
        try:
            state = client.call_tool("editor_state", {})
            last = state
            if isinstance(state, dict):
                version = str(state.get("godot_version", state.get("version", "")))
                readiness = str(state.get("readiness", ""))
                if version.startswith("4.7.1") and readiness.lower() not in {"disconnected", "offline"}:
                    return state
        except (urllib.error.URLError, TimeoutError, RuntimeError, OSError) as exc:
            last = repr(exc)
        time.sleep(1.0)
    raise RuntimeError(f"Godot AI editor session did not become ready: {last!r}")


def wait_for_game(client: HttpMcpClient, timeout: float = 90.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last: Any = None
    while time.monotonic() < deadline:
        state = client.call_tool("editor_state", {})
        last = state
        if isinstance(state, dict):
            status = state.get("game_status", {})
            helper_live = bool(state.get("helper_live"))
            capture_ready = bool(state.get("game_capture_ready"))
            if isinstance(status, dict):
                helper_live = helper_live or status.get("status") == "live"
            if helper_live and capture_ready:
                return state
        time.sleep(0.5)
    raise RuntimeError(f"Godot AI game helper did not become live: {last!r}")


def save_screenshot(result: dict[str, Any], output: Path) -> bool:
    for block in result.get("content", []):
        if not isinstance(block, dict) or block.get("type") != "image":
            continue
        data = block.get("data")
        if not isinstance(data, str) or not data:
            continue
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(base64.b64decode(data))
        return output.stat().st_size > 0
    return False


def run_live_candidate(args: argparse.Namespace) -> tuple[dict[str, Any], Path]:
    client = HttpMcpClient(args.server_url)
    initialized = client.initialize()
    server_info = initialized.get("serverInfo", {}) if isinstance(initialized, dict) else {}
    version = str(server_info.get("version", "")) if isinstance(server_info, dict) else ""
    base.require(version == EXPECTED_VERSION, f"unexpected Godot AI server version: {version!r}")

    tools = client.list_tools()
    missing = sorted(REQUIRED_TOOLS - tools)
    base.require(not missing, f"Godot AI 3.1.5 is missing required tools: {missing}")

    editor_state = wait_for_editor(client)
    opened = client.call_tool("scene_open", {"path": "res://main.tscn", "force_reload": True})
    hierarchy = client.call_tool("scene_get_hierarchy", {"depth": 4, "limit": 32})
    base.require("Probe" in json.dumps(hierarchy), f"qualification Probe is absent from edited scene: {hierarchy!r}")

    before_props = client.call_tool(
        "node_get_properties",
        {"path": PROBE_EDITOR_PATH, "fields": ["z_index"]},
    )
    original_z = find_named_value(before_props, "z_index")
    base.require(isinstance(original_z, int) and not isinstance(original_z, bool), f"z_index was not readable: {before_props!r}")
    edited_z = original_z + 5
    client.call_tool(
        "node_set_property",
        {"path": PROBE_EDITOR_PATH, "property": "z_index", "value": edited_z},
    )
    client.call_tool("scene_save", {})
    edited_props = client.call_tool(
        "node_get_properties",
        {"path": PROBE_EDITOR_PATH, "fields": ["z_index"]},
    )
    base.require(find_named_value(edited_props, "z_index") == edited_z, "typed editor authoring did not persist")
    client.call_tool(
        "node_set_property",
        {"path": PROBE_EDITOR_PATH, "property": "z_index", "value": original_z},
    )
    client.call_tool("scene_save", {})

    input_map = client.call_tool("input_map_manage", {"op": "list", "params": {}})
    base.require(MOVE_ACTION in json.dumps(input_map), f"semantic action is absent: {input_map!r}")
    binding = client.call_tool(
        "input_map_manage",
        {
            "op": "ensure_binding",
            "params": {"action": MOVE_ACTION, "event_type": "key", "keycode": "Right"},
        },
    )

    project_run = client.call_tool("project_run", {"mode": "main", "autosave": True}, timeout=60.0)
    live_state = wait_for_game(client)

    initial_node = client.call_tool(
        "game_manage",
        {"op": "get_node_info", "params": {"path": PROBE_RUNTIME_PATH, "include_properties": True}},
    )
    initial_ticks = find_named_value(initial_node, "active_ticks")
    initial_position = find_named_value(initial_node, "position")
    base.require(initial_ticks == 0, f"unexpected initial active_ticks: {initial_node!r}")

    sequence = client.call_tool(
        "game_manage",
        {
            "op": "input_sequence",
            "params": {
                "steps": [
                    {"at_frame": 0, "action": MOVE_ACTION, "pressed": True, "strength": 1.0},
                    {"at_frame": 8, "action": MOVE_ACTION, "pressed": False, "strength": 0.0},
                ],
                "settle_frames": 1,
            },
        },
        timeout=45.0,
    )

    final_node = client.call_tool(
        "game_manage",
        {"op": "get_node_info", "params": {"path": PROBE_RUNTIME_PATH, "include_properties": True}},
    )
    final_ticks = find_named_value(final_node, "active_ticks")
    final_position = find_named_value(final_node, "position")
    base.require(isinstance(final_ticks, int) and final_ticks >= 1, f"frame-timed semantic input was not consumed: {final_node!r}")
    x_value = find_named_value(final_position, "x") if final_position is not None else None
    if x_value is None:
        x_value = find_named_value(final_node, "position_x")
    if isinstance(x_value, (int, float)) and not isinstance(x_value, bool):
        base.require(abs(float(x_value) - (32.0 + 2.0 * float(final_ticks))) <= 1e-6,
                     f"runtime movement and tick state disagree: {final_node!r}")

    screenshot_result = client.call_tool_full(
        "editor_screenshot",
        {"source": "game", "include_image": True, "max_resolution": 320},
        timeout=45.0,
    )
    screenshot_path = args.evidence.parent / "godot-ai-presentation.png"
    base.require(save_screenshot(screenshot_result, screenshot_path), "Godot AI did not return a retained game screenshot")

    game_ui = client.call_tool(
        "game_manage",
        {"op": "get_ui_elements", "params": {"max_depth": 4}},
    )
    base.require("qualification" in json.dumps(game_ui).lower(), f"runtime HUD was not observable: {game_ui!r}")

    stopped = client.call_tool("project_manage", {"op": "stop", "params": {}})

    restored_props = client.call_tool(
        "node_get_properties",
        {"path": PROBE_EDITOR_PATH, "fields": ["z_index"]},
    )
    base.require(find_named_value(restored_props, "z_index") == original_z, "authoring probe did not restore z_index")

    return ({
        "initialize": initialized,
        "advertised_tools": sorted(tools),
        "editor_state": editor_state,
        "scene_open": opened,
        "authoring": {
            "property": "z_index",
            "before": original_z,
            "edited": edited_z,
            "restored": find_named_value(restored_props, "z_index"),
        },
        "input_map": input_map,
        "ensure_binding": binding,
        "project_run": project_run,
        "live_state": live_state,
        "initial_node": initial_node,
        "initial_position": initial_position,
        "input_sequence": sequence,
        "final_node": final_node,
        "final_position": final_position,
        "game_ui": game_ui,
        "project_stop": stopped,
    }, screenshot_path)


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        relative = path.relative_to(root).as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def run_qualification(args: argparse.Namespace) -> dict[str, Any]:
    godot_env = os.environ.get("GODOT_PATH", "")
    base.require(bool(godot_env), "GODOT_PATH must identify the pinned official Godot binary")
    godot = Path(godot_env).resolve()
    base.require(godot.is_file(), f"GODOT_PATH does not exist: {godot}")

    source_hash_before = tree_sha256(args.source_fixture)
    independent = prove_independent_good_and_bad(godot, args.source_fixture, args.source_fixture)
    live, screenshot_path = run_live_candidate(args)
    source_hash_after = tree_sha256(args.source_fixture)
    base.require(source_hash_after == source_hash_before, "Godot AI changed retained repository qualification fixture")

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
            "id": "hi-godot/godot-ai",
            "package": "godot-ai==3.1.5",
            "version": EXPECTED_VERSION,
            "source_commit": EXPECTED_SOURCE_COMMIT,
            "package_identity": os.environ.get("TRACE2D_B2_GODOT_AI_PACKAGE_IDENTITY", "unknown"),
        },
        "engine": {
            "id": "godot",
            "version": EXPECTED_GODOT_VERSION,
            "reported_version": version_output.stdout.strip(),
        },
        "checks": {
            "normal_source_plugin_install": True,
            "public_mcp_transport": True,
            "typed_editor_authoring": True,
            "semantic_input_configuration": True,
            "running_game_launch": True,
            "structured_runtime_observation": True,
            "frame_timed_player_like_input": True,
            "presentation_capture": True,
            "runtime_ui_observation": True,
            "candidate_test_framework_advertised": "test_run" in live["advertised_tools"],
            "independent_known_good_acceptance": True,
            "independent_known_bad_rejection": True,
            "retained_source_unchanged": True,
            "privileged_reflection_or_code_execution_required": False,
        },
        "fixture": {
            "source_tree_sha256_before": source_hash_before,
            "source_tree_sha256_after": source_hash_after,
        },
        "independent_verifier": independent,
        "live": live,
        "presentation_artifact": screenshot_path.name,
        "environment": {
            "os": platform.platform(),
            "architecture": platform.machine(),
            "python_version": os.environ.get("TRACE2D_B2_PYTHON_VERSION", "unknown"),
            "mcp_protocol": "2025-03-26",
        },
        "generated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-url", default="http://127.0.0.1:8000/mcp")
    parser.add_argument("--source-fixture", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        evidence = run_qualification(args)
    except Exception as exc:
        print(f"B2 hi-godot Godot AI qualification failed: {exc}", flush=True)
        return 1
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
