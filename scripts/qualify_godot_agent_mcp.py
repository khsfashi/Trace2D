#!/usr/bin/env python3
"""Shared stdio MCP protocol helpers for the B0 Godot Agent qualifier.

This module is intentionally not a qualification entrypoint. The only live
qualification driver is ``qualify_godot_agent_mcp_live.py`` so rejected early
measurement criteria cannot accidentally be run as an alternative protocol.
"""

from __future__ import annotations

import hashlib
import json
import queue
import subprocess
import threading
import time
from pathlib import Path
from typing import Any


PROTOCOL_VERSION = "2025-06-18"
PLAYER_PATH = "/root/Root/Player"


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
                self._messages.put(
                    {"_reader_error": f"invalid MCP JSON: {exc}: {line[:200]}"}
                )
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

    def request(
        self,
        method: str,
        params: dict[str, Any] | None = None,
        timeout: float = 30.0,
    ) -> dict[str, Any]:
        request_id = self._next_id
        self._next_id += 1
        message: dict[str, Any] = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
        }
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
                raise QualificationError(
                    f"MCP server exited with code {self._proc.returncode}"
                )
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
                raise QualificationError(
                    f"MCP protocol error for {method}: {response['error']}"
                )
            return response

    def initialize(self) -> dict[str, Any]:
        response = self.request(
            "initialize",
            {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {
                    "name": "trace2d-b0-qualifier",
                    "version": "1",
                },
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
        return {
            item.get("name")
            for item in tools
            if isinstance(item, dict) and isinstance(item.get("name"), str)
        }

    def call_tool(
        self,
        name: str,
        arguments: dict[str, Any],
        timeout: float = 45.0,
    ) -> Any:
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
    texts = [
        chunk.get("text", "")
        for chunk in chunks
        if isinstance(chunk, dict) and chunk.get("type") == "text"
    ]
    return "\n".join(str(text) for text in texts if text)


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(
        candidate
        for candidate in root.rglob("*")
        if candidate.is_file() and ".godot" not in candidate.parts
    ):
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
                {
                    "action": "digest",
                    "select": "group",
                    "group": "mcp_watch",
                    "max_nodes": 8,
                },
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
            state = client.call_tool(
                "godot_editor_read", {"action": "get_state"}, timeout=15.0
            )
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
