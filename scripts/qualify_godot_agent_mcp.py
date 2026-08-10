#!/usr/bin/env python3
"""Live hosted qualification for the Benchmark B0 godot.agent lane.

This is deliberately not a scored benchmark runner. It exercises the selected
public Godot MCP bridge over its real stdio MCP protocol and records evidence
for the four qualification capabilities required by GODOT_AGENT.md.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import queue
import subprocess
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PROTOCOL_VERSION = "2025-06-18"
PLAYER_PATH = "/root/Root/Player"
STEP_FRAMES = 12
STEP_INPUT = [{"key": "d", "start_ms": 0, "duration_ms": 180}]


class QualificationError(RuntimeError):
    pass


class McpClient:
    def __init__(self, server: Path, stderr_path: Path) -> None:
        stderr_path.parent.mkdir(parents=True, exist_ok=True)
        self._stderr = stderr_path.open("w", encoding="utf-8")
        self._proc = subprocess.Popen(
            [str(server)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self._stderr,
            text=True,
            bufsize=1,
        )
        if self._proc.stdin is None or self._proc.stdout is None:
            raise QualificationError("failed to open MCP stdio pipes")
        self._stdin = self._proc.stdin
        self._messages: queue.Queue[dict[str, Any]] = queue.Queue()
        self._deferred: dict[int, dict[str, Any]] = {}
        self._next_id = 1
        self._reader = threading.Thread(target=self._read_stdout, daemon=True)
        self._reader.start()

    def _read_stdout(self) -> None:
        assert self._proc.stdout is not None
        for raw_line in self._proc.stdout:
            line = raw_line.strip()
            if not line:
                continue
            try:
                message = json.loads(line)
            except json.JSONDecodeError as exc:
                self._messages.put({"_reader_error": f"invalid MCP JSON: {exc}: {line[:200]}"})
                continue
            if isinstance(message, dict):
                self._messages.put(message)

    def close(self) -> None:
        try:
            if self._proc.poll() is None:
                self._proc.terminate()
                self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self._proc.kill()
        finally:
            self._stderr.close()

    def notify(self, method: str, params: dict[str, Any] | None = None) -> None:
        message: dict[str, Any] = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            message["params"] = params
        self._stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
        self._stdin.flush()

    def request(self, method: str, params: dict[str, Any] | None = None, timeout: float = 30.0) -> dict[str, Any]:
        request_id = self._next_id
        self._next_id += 1
        message: dict[str, Any] = {"jsonrpc": "2.0", "id": request_id, "method": method}
        if params is not None:
            message["params"] = params
        self._stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
        self._stdin.flush()

        if request_id in self._deferred:
            return self._deferred.pop(request_id)

        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise QualificationError(f"MCP request timed out: {method}")
            if self._proc.poll() is not None and self._messages.empty():
                raise QualificationError(f"MCP server exited with code {self._proc.returncode}")
            try:
                response = self._messages.get(timeout=min(remaining, 0.5))
            except queue.Empty:
                continue
            if "_reader_error" in response:
                raise QualificationError(str(response["_reader_error"]))
            response_id = response.get("id")
            if not isinstance(response_id, int):
                continue
            if response_id != request_id:
                self._deferred[response_id] = response
                continue
            if "error" in response:
                raise QualificationError(f"MCP protocol error for {method}: {response['error']}")
            return response

    def initialize(self) -> dict[str, Any]:
        response = self.request(
            "initialize",
            {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": "trace2d-b0-qualifier", "version": "1"},
            },
        )
        result = response.get("result")
        if not isinstance(result, dict):
            raise QualificationError("initialize returned no result object")
        self.notify("notifications/initialized")
        return result

    def list_tools(self) -> set[str]:
        response = self.request("tools/list")
        result = response.get("result", {})
        tools = result.get("tools", []) if isinstance(result, dict) else []
        return {item.get("name") for item in tools if isinstance(item, dict) and isinstance(item.get("name"), str)}

    def call_tool(self, name: str, arguments: dict[str, Any], timeout: float = 45.0) -> Any:
        response = self.request(
            "tools/call",
            {"name": name, "arguments": arguments},
            timeout=timeout,
        )
        result = response.get("result")
        if not isinstance(result, dict):
            raise QualificationError(f"{name} returned no result object")
        if result.get("isError") is True:
            raise QualificationError(f"{name} failed: {tool_text(result)}")
        structured = result.get("structuredContent")
        if structured is not None:
            return structured
        text = tool_text(result)
        if text:
            try:
                return json.loads(text)
            except json.JSONDecodeError:
                return text
        return result


def tool_text(result: dict[str, Any]) -> str:
    chunks = result.get("content", [])
    if not isinstance(chunks, list):
        return ""
    texts = [chunk.get("text", "") for chunk in chunks if isinstance(chunk, dict) and chunk.get("type") == "text"]
    return "\n".join(str(text) for text in texts if text)


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file() and ".godot" not in p.parts):
        relative = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        data = path.read_bytes()
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def find_player_state(value: Any) -> dict[str, Any] | None:
    if isinstance(value, dict):
        if value.get("semantic_id") == "player":
            return value
        for child in value.values():
            found = find_player_state(child)
            if found is not None:
                return found
    elif isinstance(value, list):
        for child in value:
            found = find_player_state(child)
            if found is not None:
                return found
    return None


def as_number(value: Any, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise QualificationError(f"{label} is not numeric: {value!r}")
    return float(value)


def player_snapshot(client: McpClient, timeout: float = 45.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last_error = "runtime digest unavailable"
    while time.monotonic() < deadline:
        try:
            digest = client.call_tool(
                "godot_runtime_state",
                {"action": "digest", "select": "group", "group": "mcp_watch", "max_nodes": 8},
                timeout=15.0,
            )
            player = find_player_state(digest)
            if player is not None:
                return player
            last_error = f"player semantic state not found in digest: {digest!r}"
        except QualificationError as exc:
            last_error = str(exc)
        time.sleep(0.5)
    raise QualificationError(last_error)


def wait_for_editor(client: McpClient, timeout: float = 60.0) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last_error = "editor unavailable"
    while time.monotonic() < deadline:
        try:
            state = client.call_tool("godot_editor_read", {"action": "get_state"}, timeout=15.0)
            if isinstance(state, dict) and state.get("godot_version"):
                return state
            last_error = f"unexpected editor state: {state!r}"
        except QualificationError as exc:
            last_error = str(exc)
        time.sleep(0.75)
    raise QualificationError(last_error)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise QualificationError(message)


def run_qualification(args: argparse.Namespace) -> dict[str, Any]:
    source_hash = tree_sha256(args.source_fixture)
    installed_hash_before = tree_sha256(args.project)
    client = McpClient(args.server, args.mcp_stderr)
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
        require(not missing, f"selected bridge is missing required tools: {missing}")

        editor_state = wait_for_editor(client)
        client.call_tool("godot_scene", {"action": "open", "scene_path": "res://main.tscn"})

        # Q1: reversible editor authoring through the normal node/scene tools.
        before_props = client.call_tool("godot_node_read", {"action": "get_properties", "node_path": PLAYER_PATH})
        require(isinstance(before_props, dict), "Player properties were not structured")
        original_z = before_props.get("z_index")
        require(isinstance(original_z, int), f"unexpected Player z_index: {original_z!r}")
        probe_z = original_z + 7
        client.call_tool(
            "godot_node_edit",
            {"action": "update", "node_path": PLAYER_PATH, "properties": {"z_index": probe_z}},
        )
        client.call_tool("godot_scene", {"action": "save"})
        edited_props = client.call_tool("godot_node_read", {"action": "get_properties", "node_path": PLAYER_PATH})
        require(isinstance(edited_props, dict) and edited_props.get("z_index") == probe_z, "Q1 authoring edit did not persist")
        client.call_tool(
            "godot_node_edit",
            {"action": "update", "node_path": PLAYER_PATH, "properties": {"z_index": original_z}},
        )
        client.call_tool("godot_scene", {"action": "save"})
        restored_props = client.call_tool("godot_node_read", {"action": "get_properties", "node_path": PLAYER_PATH})
        require(isinstance(restored_props, dict) and restored_props.get("z_index") == original_z, "Q1 fixture restore failed")

        # Q2: launch frozen at frame zero and prove wall time does not advance state.
        client.call_tool("godot_editor_edit", {"action": "run", "frozen": True}, timeout=45.0)
        initial = player_snapshot(client)
        status = client.call_tool("godot_game_time", {"action": "status"})
        require(isinstance(status, dict) and status.get("frozen") is True, f"game did not launch frozen: {status!r}")
        require(initial.get("semantic_id") == "player", f"Q2 wrong semantic id: {initial!r}")
        require(abs(as_number(initial.get("position_x"), "initial.position_x")) <= 1e-9, f"Q2 initial x moved: {initial!r}")
        require(abs(as_number(initial.get("position_y"), "initial.position_y")) <= 1e-9, f"Q2 initial y moved: {initial!r}")
        require(as_number(initial.get("physics_ticks"), "initial.physics_ticks") == 0.0, f"Q2 frame-zero freeze failed: {initial!r}")
        time.sleep(0.5)
        after_wall_wait = player_snapshot(client)
        require(after_wall_wait.get("physics_ticks") == initial.get("physics_ticks"), "Q2 wall time advanced physics ticks")
        require(after_wall_wait.get("position_x") == initial.get("position_x"), "Q2 wall time moved Player")

        # Q3 + first half of Q4: real D key input inside an exact stepped window.
        step_one = client.call_tool(
            "godot_game_time",
            {"action": "step", "frames": STEP_FRAMES, "inputs": STEP_INPUT},
            timeout=45.0,
        )
        first = player_snapshot(client)
        require(isinstance(step_one, dict), f"Q3 step result was not structured: {step_one!r}")
        input_kinds = step_one.get("input_kinds", {})
        require(isinstance(input_kinds, dict) and as_number(input_kinds.get("key", 0), "step_one.input_kinds.key") >= 1, "Q3 bridge did not report raw key injection")
        require(as_number(first.get("position_x"), "first.position_x") > 0.0, f"Q3 D input did not move Player: {first!r}")
        require(as_number(first.get("physics_ticks"), "first.physics_ticks") > 0.0, f"Q3 step did not advance physics: {first!r}")

        # Q4: clean restart, deliberate wall wait while frozen, identical step+input.
        client.call_tool("godot_editor_edit", {"action": "stop"})
        client.call_tool("godot_editor_edit", {"action": "run", "frozen": True}, timeout=45.0)
        replay_initial = player_snapshot(client)
        require(as_number(replay_initial.get("physics_ticks"), "replay_initial.physics_ticks") == 0.0, f"Q4 replay did not restart at frame zero: {replay_initial!r}")
        time.sleep(0.75)
        replay_after_wait = player_snapshot(client)
        require(replay_after_wait.get("physics_ticks") == replay_initial.get("physics_ticks"), "Q4 replay wall wait advanced physics ticks")
        step_two = client.call_tool(
            "godot_game_time",
            {"action": "step", "frames": STEP_FRAMES, "inputs": STEP_INPUT},
            timeout=45.0,
        )
        second = player_snapshot(client)
        require(first.get("physics_ticks") == second.get("physics_ticks"), f"Q4 physics tick replay mismatch: {first!r} vs {second!r}")
        require(abs(as_number(first.get("position_x"), "first.position_x") - as_number(second.get("position_x"), "second.position_x")) <= 1e-9, f"Q4 position replay mismatch: {first!r} vs {second!r}")
        require(isinstance(step_two, dict) and step_one.get("frames") == step_two.get("frames"), "Q4 bridge frame count mismatch")
        require(step_one.get("physics_ticks") == step_two.get("physics_ticks"), "Q4 bridge physics tick count mismatch")
        client.call_tool("godot_editor_edit", {"action": "stop"})

        evidence = {
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
            "environment": {
                "os": platform.platform(),
                "architecture": platform.machine(),
                "node_version": os.environ.get("TRACE2D_B0_NODE_VERSION", "unknown"),
                "mcp_client": "scripts/qualify_godot_agent_mcp.py@1",
                "runner_image": os.environ.get("ImageOS", "unknown"),
            },
            "fixture": {
                "source_tree_sha256": source_hash,
                "installed_tree_sha256_before_qualification": installed_hash_before,
                "installed_tree_sha256_after_qualification": tree_sha256(args.project),
            },
            "observations": {
                "authoring": {"property": "z_index", "before": original_z, "edited": probe_z, "restored": restored_props.get("z_index")},
                "runtime_initial": initial,
                "runtime_after_wall_wait": after_wall_wait,
                "step_one": step_one,
                "runtime_after_step_one": first,
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
        return evidence
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
