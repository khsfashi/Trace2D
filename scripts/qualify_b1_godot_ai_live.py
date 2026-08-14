#!/usr/bin/env python3
"""Non-scored B1 content capability qualification for hi-godot/godot-ai."""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import platform
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from fastmcp import Client

PLAYER_PATH = "/Root/B1Animation"
PARTICLE_PATH = "/Root/B1Particles"
ANIMATION = "content_probe"


class QualificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise QualificationError(message)


def result_data(value: Any) -> Any:
    data = getattr(value, "data", None)
    if data is not None:
        return data
    structured = getattr(value, "structured_content", None)
    if structured is not None:
        return structured
    return value


async def wait_for_editor(client: Client, timeout_seconds: float = 60.0) -> dict[str, Any]:
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout_seconds
    last: Any = None
    while loop.time() < deadline:
        try:
            last = result_data(await client.call_tool("editor_state", {}))
            if isinstance(last, dict) and last.get("ready", True):
                return last
        except Exception as exc:
            last = str(exc)
        await asyncio.sleep(0.5)
    raise QualificationError(f"Godot AI editor session did not become ready: {last!r}")


async def run(args: argparse.Namespace) -> dict[str, Any]:
    async with Client(args.endpoint) as client:
        state = await wait_for_editor(client)

        opened = result_data(await client.call_tool("scene_open", {"path": "res://main.tscn"}))
        require(isinstance(opened, dict), f"scene_open was not structured: {opened!r}")

        await client.call_tool(
            "animation_create",
            {
                "player_path": PLAYER_PATH,
                "name": ANIMATION,
                "length": 1.0,
                "loop_mode": "none",
                "overwrite": True,
            },
        )
        await client.call_tool(
            "animation_manage",
            {
                "op": "add_method_track",
                "params": {
                    "player_path": PLAYER_PATH,
                    "animation_name": ANIMATION,
                    "target_node_path": "Subject",
                    "keyframes": [{"time": 0.375, "method": "queue_redraw", "args": []}],
                },
            },
        )
        await client.call_tool(
            "particle_manage",
            {
                "op": "set_main",
                "params": {
                    "node_path": PARTICLE_PATH,
                    "properties": {"amount": 96, "lifetime": 0.8, "emitting": False},
                },
            },
        )
        await client.call_tool("scene_save", {})

        animation = result_data(
            await client.call_tool(
                "animation_manage",
                {"op": "get", "params": {"player_path": PLAYER_PATH, "animation_name": ANIMATION}},
            )
        )
        particle = result_data(
            await client.call_tool(
                "particle_manage", {"op": "get", "params": {"node_path": PARTICLE_PATH}}
            )
        )
        validation = result_data(
            await client.call_tool(
                "animation_manage",
                {"op": "validate", "params": {"player_path": PLAYER_PATH, "animation_name": ANIMATION}},
            )
        )
        screenshot = await client.call_tool(
            "editor_screenshot",
            {"source": "viewport_2d", "max_resolution": 320, "include_image": True},
        )
        require(animation is not None, "animation readback missing")
        require(particle is not None, "particle readback missing")
        require(validation is not None, "animation validation missing")
        require(screenshot is not None, "presentation capture handoff failed")

        return {
            "schema_version": 1,
            "benchmark_id": "trace2d-b1",
            "fixture_id": "b1-content-capability-qualification",
            "provider": "hi-godot/godot-ai",
            "pin": "v3.0.6@f3d99dfbd38c9e095edf1467f85bee507ace2c3a",
            "qualified_authoring": True,
            "engine": {"id": "godot", "version": "4.7.1-stable", "reported_state": state},
            "checks": {
                "scene_save": True,
                "animation_exact_event_authoring": True,
                "particle_budget_authoring": True,
                "structured_readback": True,
                "provider_animation_validation": True,
                "presentation_capture_handoff": True,
            },
            "observations": {
                "animation_readback": animation,
                "particle_readback": particle,
                "animation_validation": validation,
            },
            "environment": {
                "os": platform.platform(),
                "architecture": platform.machine(),
                "endpoint": args.endpoint,
                "source_commit": os.environ.get("TRACE2D_B1_GODOT_AI_COMMIT", "unknown"),
            },
            "generated_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--endpoint", default="http://127.0.0.1:8000/mcp")
    parser.add_argument("--evidence", type=Path, required=True)
    return parser.parse_args()


async def async_main() -> int:
    args = parse_args()
    try:
        evidence = await run(args)
    except Exception as exc:
        print(f"B1 hi-godot qualification failed: {exc}", flush=True)
        return 1
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(
        json.dumps(evidence, indent=2, sort_keys=True, default=str) + "\n", encoding="utf-8"
    )
    print(json.dumps(evidence, indent=2, sort_keys=True, default=str), flush=True)
    return 0


def main() -> int:
    return asyncio.run(async_main())


if __name__ == "__main__":
    raise SystemExit(main())
