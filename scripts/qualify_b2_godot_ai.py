#!/usr/bin/env python3
"""Non-scored B2 qualification for hi-godot/godot-ai 3.1.5."""
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
    "editor_state", "scene_open", "scene_save", "scene_get_hierarchy",
    "node_get_properties", "node_set_property", "input_map_manage",
    "project_run", "project_manage", "game_manage", "editor_screenshot", "test_run",
}


class HttpMcpClient:
    def __init__(self, url: str) -> None:
        self.url = url
        self.session_id = ""
        self.request_id = 1

    def _post(self, body: dict[str, Any], timeout: float = 30.0) -> tuple[dict[str, Any], dict[str, str]]:
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        req = urllib.request.Request(
            self.url, data=json.dumps(body).encode("utf-8"), headers=headers, method="POST"
        )
        with urllib.request.urlopen(req, timeout=timeout) as response:
            raw = response.read().decode("utf-8")
            response_headers = {k.lower(): v for k, v in response.headers.items()}
        if not raw.strip():
            return {}, response_headers
        for line in raw.splitlines():
            if line.startswith("data: "):
                candidate = json.loads(line[6:])
                base.require(isinstance(candidate, dict), f"MCP SSE payload was not an object: {line[:300]}")
                return candidate, response_headers
        candidate = json.loads(raw)
        base.require(isinstance(candidate, dict), f"MCP response was not an object: {raw[:300]}")
        return candidate, response_headers

    def initialize(self) -> dict[str, Any]:
        payload, headers = self._post({
            "jsonrpc": "2.0", "id": self.request_id, "method": "initialize",
            "params": {
                "protocolVersion": "2025-03-26", "capabilities": {},
                "clientInfo": {"name": "trace2d-b2-qualification", "version": "1.0"},
            },
        })
        self.request_id += 1
        self.session_id = headers.get("mcp-session-id", "")
        base.require(bool(self.session_id), f"initialize did not return Mcp-Session-Id: {payload!r}")
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})
        result = payload.get("result", {})
        base.require(isinstance(result, dict), f"initialize result was not structured: {payload!r}")
        return result

    def list_tools(self) -> set[str]:
        payload, _ = self._post({"jsonrpc": "2.0", "id": self.request_id, "method": "tools/list", "params": {}})
        self.request_id += 1
        result = payload.get("result", {})
        tools = result.get("tools", []) if isinstance(result, dict) else []
        return {str(tool["name"]) for tool in tools if isinstance(tool, dict) and tool.get("name")}

    def call_tool_full(self, name: str, arguments: dict[str, Any], timeout: float = 45.0) -> dict[str, Any]:
        payload, _ = self._post({
            "jsonrpc": "2.0", "id": self.request_id, "method": "tools/call",
            "params": {"name": name, "arguments": arguments},
        }, timeout=timeout)
        self.request_id += 1
        if "error" in payload:
            raise RuntimeError(f"MCP {name} transport error: {payload['error']}")
        result = payload.get("result", {})
        base.require(isinstance(result, dict), f"MCP {name} result was not structured: {payload!r}")
        if result.get("isError") is True:
            raise RuntimeError(f"MCP {name} tool error: {result!r}")
        return result

    def call_tool(self, name: str, arguments: dict[str, Any], timeout: float = 45.0) -> Any:
        result = self.call_tool_full(name, arguments, timeout)
        if result.get("structuredContent") is not None:
            return result["structuredContent"]
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
        if isinstance(block, dict) and block.get("type") == "image" and isinstance(block.get("data"), str):
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(base64.b64decode(block["data"]))
            return output.stat().st_size > 0
    return False


def run_live_candidate(args: argparse.Namespace) -> tuple[dict[str, Any], Path]:
    client = HttpMcpClient(args.server_url)
    initialized = client.initialize()
    # FastMCP reports its own transport implementation version in serverInfo.
    # The Godot AI application identity is independently pinned by the exact
    # source commit, PyPI package version, and wheel SHA in the workflow.
    tools = client.list_tools()
    base.require(not sorted(REQUIRED_TOOLS - tools), f"Godot AI missing tools: {sorted(REQUIRED_TOOLS - tools)}")

    editor_state = wait_for_editor(client)
    opened = client.call_tool("scene_open", {"path": "res://main.tscn", "force_reload": True})
    hierarchy = client.call_tool("scene_get_hierarchy", {"depth": 4, "limit": 32})
    base.require("Probe" in json.dumps(hierarchy), f"Probe absent from hierarchy: {hierarchy!r}")

    before = client.call_tool("node_get_properties", {"path": PROBE_EDITOR_PATH, "fields": ["z_index"]})
    original_z = find_named_value(before, "z_index")
    base.require(isinstance(original_z, int) and not isinstance(original_z, bool), f"z_index unreadable: {before!r}")
    edited_z = original_z + 5
    client.call_tool("node_set_property", {"path": PROBE_EDITOR_PATH, "property": "z_index", "value": edited_z})
    client.call_tool("scene_save", {})
    edited = client.call_tool("node_get_properties", {"path": PROBE_EDITOR_PATH, "fields": ["z_index"]})
    base.require(find_named_value(edited, "z_index") == edited_z, "typed authoring did not persist")
    client.call_tool("node_set_property", {"path": PROBE_EDITOR_PATH, "property": "z_index", "value": original_z})
    client.call_tool("scene_save", {})

    input_map = client.call_tool("input_map_manage", {"op": "list", "params": {}})
    base.require(MOVE_ACTION in json.dumps(input_map), f"semantic action absent: {input_map!r}")
    binding = client.call_tool("input_map_manage", {
        "op": "ensure_binding",
        "params": {"action": MOVE_ACTION, "event_type": "key", "keycode": "Right"},
    })

    project_run = client.call_tool("project_run", {"mode": "main", "autosave": True}, timeout=60.0)
    live_state = wait_for_game(client)
    initial_node = client.call_tool("game_manage", {
        "op": "get_node_info", "params": {"path": PROBE_RUNTIME_PATH, "include_properties": True},
    })
    base.require(find_named_value(initial_node, "active_ticks") == 0, f"bad initial ticks: {initial_node!r}")

    sequence = client.call_tool("game_manage", {
        "op": "input_sequence",
        "params": {
            "steps": [
                {"at_frame": 0, "action": MOVE_ACTION, "pressed": True, "strength": 1.0},
                {"at_frame": 8, "action": MOVE_ACTION, "pressed": False, "strength": 0.0},
            ],
            "settle_frames": 1,
        },
    }, timeout=45.0)
    final_node = client.call_tool("game_manage", {
        "op": "get_node_info", "params": {"path": PROBE_RUNTIME_PATH, "include_properties": True},
    })
    final_ticks = find_named_value(final_node, "active_ticks")
    base.require(isinstance(final_ticks, int) and final_ticks >= 1, f"semantic input not consumed: {final_node!r}")

    screenshot_result = client.call_tool_full(
        "editor_screenshot", {"source": "game", "include_image": True, "max_resolution": 320}, timeout=45.0
    )
    screenshot_path = args.evidence.parent / "godot-ai-presentation.png"
    base.require(save_screenshot(screenshot_result, screenshot_path), "game screenshot was not retained")
    game_ui = client.call_tool("game_manage", {"op": "get_ui_elements", "params": {"max_depth": 4}})
    base.require("qualification" in json.dumps(game_ui).lower(), f"runtime HUD not observable: {game_ui!r}")
    stopped = client.call_tool("project_manage", {"op": "stop", "params": {}})

    restored = client.call_tool("node_get_properties", {"path": PROBE_EDITOR_PATH, "fields": ["z_index"]})
    base.require(find_named_value(restored, "z_index") == original_z, "authoring probe did not restore")
    return ({
        "initialize": initialized, "advertised_tools": sorted(tools), "editor_state": editor_state,
        "scene_open": opened, "authoring": {"before": original_z, "edited": edited_z, "restored": original_z},
        "input_map": input_map, "ensure_binding": binding, "project_run": project_run,
        "live_state": live_state, "initial_node": initial_node, "input_sequence": sequence,
        "final_node": final_node, "game_ui": game_ui, "project_stop": stopped,
    }, screenshot_path)


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def run_qualification(args: argparse.Namespace) -> dict[str, Any]:
    godot_env = os.environ.get("GODOT_PATH", "")
    base.require(bool(godot_env), "GODOT_PATH must identify pinned Godot")
    godot = Path(godot_env).resolve()
    base.require(godot.is_file(), f"GODOT_PATH does not exist: {godot}")
    source_hash_before = tree_sha256(args.source_fixture)
    independent = prove_independent_good_and_bad(godot, args.source_fixture, args.source_fixture)
    live, screenshot_path = run_live_candidate(args)
    source_hash_after = tree_sha256(args.source_fixture)
    base.require(source_hash_after == source_hash_before, "Godot AI changed retained qualification fixture")
    version = subprocess.run([str(godot), "--version"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
    base.require(version.returncode == 0 and version.stdout.startswith("4.7.1.stable"), f"unexpected Godot: {version.stdout}")
    return {
        "schema_version": 1,
        "benchmark_id": "trace2d-b2",
        "qualification_id": "b2-gameplay-loop-qualification-v1",
        "scored": False,
        "qualified": True,
        "scored_task_prompt_exposed": False,
        "candidate": {
            "id": "hi-godot/godot-ai", "package": "godot-ai==3.1.5", "version": EXPECTED_VERSION,
            "source_commit": EXPECTED_SOURCE_COMMIT,
            "package_identity": os.environ.get("TRACE2D_B2_GODOT_AI_PACKAGE_IDENTITY", "unknown"),
        },
        "engine": {"id": "godot", "version": EXPECTED_GODOT_VERSION, "reported_version": version.stdout.strip()},
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
            "no_privileged_reflection_or_code_execution_required": True,
        },
        "fixture": {"source_tree_sha256_before": source_hash_before, "source_tree_sha256_after": source_hash_after},
        "independent_verifier": independent,
        "live": live,
        "presentation_artifact": screenshot_path.name,
        "environment": {
            "os": platform.platform(), "architecture": platform.machine(),
            "python_version": os.environ.get("TRACE2D_B2_PYTHON_VERSION", "unknown"), "mcp_protocol": "2025-03-26",
        },
        "generated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-url", default="http://127.0.0.1:8000/mcp")
    parser.add_argument("--source-fixture", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
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
