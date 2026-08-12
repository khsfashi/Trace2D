#!/usr/bin/env python3
"""Benchmark B1 Codex adapter on the already-qualified B0 Windows ACL boundary.

B1 preserves the frozen Codex CLI/model/budget/isolation semantics from B0 while
adapting only environment bootstrap required by the selected B1 capabilities:

- godot.generic: stock pinned Godot + filesystem/process workflow;
- godot.agent: stock pinned Godot + frozen hi-godot/godot-ai HTTP MCP addon;
- trace2d.agent: frozen Trace2D public CLI/filesystem workflow for the B1 content
  fixtures. No benchmark-only scene or hidden MCP authority is injected merely
  to make content fixtures fit the scene-bound trace2d_mcp executable.

All model turns still run through benchmark_b0_codex_windows_acl_wrapper's real
CodexSandboxOffline SID guard, provider usage accounting, zero-human contract,
and completed-over-budget classification.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Iterable

import benchmark_b0_codex_wrapper as core
import benchmark_b0_codex_windows_acl_wrapper as windows

GODOT_AI_ID = "hi-godot/godot-ai"
GODOT_AI_VERSION = "3.0.6"
GODOT_AI_COMMIT = "f3d99dfbd38c9e095edf1467f85bee507ace2c3a"
GODOT_AI_PLUGIN = 'enabled=PackedStringArray("res://addons/godot_ai/plugin.cfg")'
B1_PERMISSION_PROFILE = f":workspace+{windows.ISOLATION_BACKEND}"


class B1WrapperError(core.WrapperError):
    pass


def _require_file(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise B1WrapperError(f"{label} does not exist: {resolved}")
    return resolved


def _require_dir(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_dir():
        raise B1WrapperError(f"{label} does not exist: {resolved}")
    return resolved


def _env_file(name: str, required: bool = True) -> Path | None:
    raw = os.environ.get(name, "").strip()
    if not raw:
        if required:
            raise B1WrapperError(f"{name} is required")
        return None
    return _require_file(Path(raw), name)


def _env_dir(name: str, required: bool = True) -> Path | None:
    raw = os.environ.get(name, "").strip()
    if not raw:
        if required:
            raise B1WrapperError(f"{name} is required")
        return None
    return _require_dir(Path(raw), name)


def _unique_roots(values: Iterable[Path]) -> list[Path]:
    result: list[Path] = []
    seen: set[str] = set()
    for value in values:
        resolved = value.expanduser().resolve()
        key = os.path.normcase(str(resolved))
        if key in seen:
            continue
        seen.add(key)
        result.append(resolved)
    return result


def tool_roots_for_b1(codex: str, workspace: Path, lane: str) -> tuple[list[Path], dict[str, Path | None]]:
    roots: list[Path] = [Path(codex).resolve().parent]
    package_root = core.discover_codex_package_root(codex, workspace)
    if package_root is not None:
        roots.append(package_root)

    godot_bin = _env_file("TRACE2D_BENCH_GODOT_BIN", lane.startswith("godot."))
    trace2d_bin = _env_file("TRACE2D_BENCH_TRACE2D_BIN", lane == "trace2d.agent")
    godot_ai_python = _env_file("TRACE2D_B1_GODOT_AI_PYTHON", lane == "godot.agent")
    godot_ai_addon = _env_dir("TRACE2D_B1_GODOT_AI_ADDON_DIR", lane == "godot.agent")

    for path in (godot_bin, trace2d_bin):
        if path is not None:
            roots.append(path.parent)

    extra = os.environ.get("TRACE2D_BENCH_CODEX_READ_ROOTS", "")
    for item in extra.split(os.pathsep):
        if item.strip():
            roots.append(Path(item.strip()))

    return _unique_roots(roots), {
        "godot_bin": godot_bin,
        "trace2d_bin": trace2d_bin,
        "godot_ai_python": godot_ai_python,
        "godot_ai_addon": godot_ai_addon,
    }


def write_b1_external_acl_config(
    *,
    codex_home: Path,
    workspace: Path,
    read_only_roots: list[Path],
    lane: str,
    godot_mcp_server: Path | None,
    trace2d_mcp_server: Path | None,
) -> Path:
    del workspace, read_only_roots, godot_mcp_server, trace2d_mcp_server
    lines = [
        f"model = {core.toml_string(windows.MODEL_REVISION)}",
        f"model_reasoning_effort = {core.toml_string(core.REASONING_EFFORT)}",
        'approval_policy = "never"',
        'default_permissions = ":workspace"',
        'web_search = "disabled"',
        "allow_login_shell = false",
        "",
        "[sandbox_workspace_write]",
        "network_access = false",
        "",
        "[shell_environment_policy]",
        'inherit = "core"',
    ]

    if lane == "godot.agent":
        endpoint = os.environ.get("TRACE2D_B1_GODOT_AI_ENDPOINT", "").strip()
        if not endpoint.startswith("http://127.0.0.1:") or not endpoint.endswith("/mcp"):
            raise B1WrapperError(
                "TRACE2D_B1_GODOT_AI_ENDPOINT must be a loopback streamable-HTTP MCP endpoint"
            )
        lines.extend(
            [
                "",
                "[mcp_servers.godot]",
                f"url = {core.toml_string(endpoint)}",
                "startup_timeout_sec = 20",
                "tool_timeout_sec = 45",
            ]
        )

    lines.extend(
        [
            "",
            "[windows]",
            f'sandbox = "{windows.WINDOWS_SANDBOX_MODE}"',
        ]
    )
    codex_home.mkdir(parents=True, exist_ok=True)
    config = codex_home / "config.toml"
    config.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return config


def _free_loopback_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def _wait_for_port(port: int, process: subprocess.Popen[str], timeout: float = 20.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise B1WrapperError("hi-godot/godot-ai server exited before accepting MCP connections")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.25):
                return
        except OSError:
            time.sleep(0.1)
    raise B1WrapperError("timed out waiting for hi-godot/godot-ai MCP server")


def _start_process(argv: list[str], *, cwd: Path, log_path: Path) -> tuple[subprocess.Popen[str], Any]:
    stream = log_path.open("w", encoding="utf-8")
    creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) if os.name == "nt" else 0
    try:
        process = subprocess.Popen(
            argv,
            cwd=cwd,
            stdout=stream,
            stderr=subprocess.STDOUT,
            text=True,
            creationflags=creationflags,
        )
    except BaseException:
        stream.close()
        raise
    return process, stream


def install_godot_ai_addon(addon_source: Path, workspace: Path) -> Path:
    destination = workspace / "addons" / "godot_ai"
    if destination.exists():
        raise B1WrapperError(f"reserved Godot AI addon path already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(addon_source, destination)
    plugin = destination / "plugin.cfg"
    if not plugin.is_file():
        shutil.rmtree(destination, ignore_errors=True)
        raise B1WrapperError(f"selected Godot AI addon is missing plugin.cfg: {plugin}")
    return destination


def enable_godot_ai_plugin(project: Path) -> bool:
    text = project.read_text(encoding="utf-8")
    if GODOT_AI_PLUGIN in text:
        return False
    if "[editor_plugins]" in text:
        raise B1WrapperError(
            "candidate project already has an [editor_plugins] section; refusing ambiguous harness injection"
        )
    project.write_text(
        text.rstrip() + "\n\n[editor_plugins]\n" + GODOT_AI_PLUGIN + "\n",
        encoding="utf-8",
    )
    return True


def remove_injected_godot_ai_plugin(project: Path) -> None:
    if not project.is_file():
        return
    text = project.read_text(encoding="utf-8")
    pattern = re.compile(
        r"\n?\[editor_plugins\]\s*\n"
        r'enabled=PackedStringArray\("res://addons/godot_ai/plugin\.cfg"\)\s*\n?',
        re.MULTILINE,
    )
    cleaned, count = pattern.subn("\n", text, count=1)
    if count:
        project.write_text(cleaned.rstrip() + "\n", encoding="utf-8")


def _stop_process(process: subprocess.Popen[str] | None, stream: Any | None) -> None:
    core.stop_process(process, stream)


def _usage_and_tools(events: list[dict[str, Any]]) -> tuple[dict[str, int], dict[str, Any]]:
    return core.token_usage(events), core.tool_metrics(events)


def _budget_details(usage: dict[str, int], tools: dict[str, Any]) -> dict[str, Any]:
    limits = {
        "max_tool_calls": int(os.environ.get("TRACE2D_BENCH_MAX_TOOL_CALLS", "80")),
        "max_input_tokens": int(os.environ.get("TRACE2D_BENCH_MAX_INPUT_TOKENS", "100000")),
        "max_output_tokens": int(os.environ.get("TRACE2D_BENCH_MAX_OUTPUT_TOKENS", "20000")),
    }
    observed = {
        "tool_calls": int(tools.get("tool_calls", 0)),
        "input_tokens": int(usage.get("input_tokens", 0)),
        "output_tokens": int(usage.get("output_tokens", 0)),
    }
    within = {
        "tool_calls": observed["tool_calls"] <= limits["max_tool_calls"],
        "input_tokens": observed["input_tokens"] <= limits["max_input_tokens"],
        "output_tokens": observed["output_tokens"] <= limits["max_output_tokens"],
    }
    return {
        "limits": limits,
        "observed": observed,
        "within": within,
        "exceeded": [name for name, ok in within.items() if not ok],
    }


def run_b1_trial(args: argparse.Namespace) -> int:
    workspace = Path(args.workspace).resolve()
    if not workspace.is_dir():
        raise B1WrapperError(f"workspace does not exist: {workspace}")
    prompt_path = _require_file(Path(args.prompt_file), "prompt file")
    result_path = Path(args.result_file).resolve()
    result_path.parent.mkdir(parents=True, exist_ok=True)

    codex = core.resolve_codex(args.codex)
    core.verify_codex_version(codex, workspace)
    read_roots, tools = tool_roots_for_b1(codex, workspace, args.lane)

    endpoint: str | None = None
    port: int | None = None
    addon_destination: Path | None = None
    injected_plugin = False
    server: subprocess.Popen[str] | None = None
    server_stream: Any | None = None
    editor: subprocess.Popen[str] | None = None
    editor_stream: Any | None = None

    if args.lane == "godot.agent":
        port = _free_loopback_port()
        endpoint = f"http://127.0.0.1:{port}/mcp"
        os.environ["TRACE2D_B1_GODOT_AI_ENDPOINT"] = endpoint

    codex_home = core.setup_codex_home(
        codex=codex,
        workspace=workspace,
        lane=args.lane,
        result_root=result_path.parent,
        read_roots=read_roots,
        godot_mcp=None,
        trace2d_mcp=None,
    )

    completed: subprocess.CompletedProcess[str] | None = None
    events: list[dict[str, Any]] = []
    try:
        if args.lane == "godot.agent":
            godot = tools["godot_bin"]
            python = tools["godot_ai_python"]
            addon = tools["godot_ai_addon"]
            if godot is None or python is None or addon is None or port is None:
                raise B1WrapperError("godot.agent selected toolchain is incomplete")
            project = _require_file(workspace / "project.godot", "Godot project")
            addon_destination = install_godot_ai_addon(addon, workspace)
            injected_plugin = enable_godot_ai_plugin(project)
            server, server_stream = _start_process(
                [str(python), "-m", "godot_ai", "--transport", "streamable-http", "--port", str(port)],
                cwd=workspace,
                log_path=result_path.parent / "godot-ai-server.log",
            )
            _wait_for_port(port, server)
            editor, editor_stream = core.start_godot_editor(
                godot,
                workspace,
                result_path.parent / "godot-editor.log",
            )

        prompt = core.common_agent_prompt(prompt_path.read_text(encoding="utf-8"))
        completed, events = core.run_codex(
            codex=codex,
            workspace=workspace,
            codex_home=codex_home,
            read_roots=read_roots,
            prompt=prompt,
            timeout=float(os.environ.get("TRACE2D_BENCH_WRAPPER_TIMEOUT", "285")),
        )
    finally:
        _stop_process(editor, editor_stream)
        _stop_process(server, server_stream)
        if args.lane == "godot.agent":
            project = workspace / "project.godot"
            if injected_plugin:
                remove_injected_godot_ai_plugin(project)
            if addon_destination is not None:
                shutil.rmtree(addon_destination, ignore_errors=True)
                addons = workspace / "addons"
                if addons.is_dir() and not any(addons.iterdir()):
                    addons.rmdir()
            os.environ.pop("TRACE2D_B1_GODOT_AI_ENDPOINT", None)

    if completed is None:
        raise B1WrapperError("Codex model turn did not start")

    events_path = result_path.parent / "codex-events.jsonl"
    events_path.write_text(completed.stdout, encoding="utf-8")
    (result_path.parent / "codex.stderr.txt").write_text(completed.stderr, encoding="utf-8")

    usage, tools_metric = _usage_and_tools(events)
    budget = _budget_details(usage, tools_metric)
    provider_completed = completed.returncode == 0 and core.saw_turn_completed(events)
    if provider_completed and not budget["exceeded"]:
        status = "completed"
    elif provider_completed:
        status = "budget_exceeded"
    else:
        status = "tool_transport_failure"

    result = {
        "schema_version": 1,
        "status": status,
        "model": {
            "agent_id": core.AGENT_ID,
            "model_id": windows.MODEL_ID,
            "model_revision": windows.MODEL_REVISION,
        },
        "human_interventions": 0,
        "metrics": {
            "revisions": tools_metric["revisions"],
            "tool_calls": tools_metric["tool_calls"],
            "input_tokens": usage["input_tokens"],
            "output_tokens": usage["output_tokens"],
            "cached_input_tokens": usage["cached_input_tokens"],
            "reasoning_output_tokens": usage["reasoning_output_tokens"],
            "normalized_operations": tools_metric["normalized_operations"],
            "engine_native_operations": tools_metric["engine_native_operations"],
        },
        "budget": budget,
        "wrapper": {
            "codex_version": core.EXPECTED_CODEX_VERSION,
            "reasoning_effort": core.REASONING_EFFORT,
            "permission_profile": B1_PERMISSION_PROFILE,
            "isolation_backend": windows.ISOLATION_BACKEND,
            "ephemeral": True,
            "approval_policy": "never",
            "web_search": "disabled",
            "process_return_code": completed.returncode,
            "turn_completed": core.saw_turn_completed(events),
            "budget_ok": not budget["exceeded"],
            "trajectory": str(events_path),
            "b1_environment_adapter": {
                "lane": args.lane,
                "godot_agent": (
                    {
                        "id": GODOT_AI_ID,
                        "version": GODOT_AI_VERSION,
                        "commit": GODOT_AI_COMMIT,
                        "transport": "streamable-http-loopback",
                    }
                    if args.lane == "godot.agent"
                    else None
                ),
                "trace2d_benchmark_only_scene_injected": False,
            },
        },
    }
    result_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0 if status == "completed" else 1


def completed_mcp_tool_names(events: list[dict[str, Any]]) -> list[str]:
    names: list[str] = []
    for item in core.completed_items(events):
        if item.get("type") not in {"mcp_tool_call", "dynamic_tool_call"}:
            continue
        name = item.get("tool", item.get("name", ""))
        if isinstance(name, str) and name:
            names.append(name)
    return names


def configure() -> None:
    windows.configure()
    core.write_isolated_config = write_b1_external_acl_config
    core.tool_roots_for_lane = tool_roots_for_b1
    core.PERMISSION_PROFILE = B1_PERMISSION_PROFILE


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D B1 frozen Codex Windows adapter")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="run one B1 Agent trial")
    run.add_argument("--workspace", required=True)
    run.add_argument("--prompt-file", required=True)
    run.add_argument("--lane", choices=tuple(core.EXPECTED_LANES) if hasattr(core, "EXPECTED_LANES") else ("godot.generic", "godot.agent", "trace2d.agent"), required=True)
    run.add_argument("--result-file", required=True)
    run.add_argument("--codex", default=os.environ.get("TRACE2D_BENCH_CODEX_BIN", "codex"))
    run.set_defaults(handler=run_b1_trial)

    probe = subparsers.add_parser("probe-isolation", help="reuse the accepted real-model B0 ACL canary")
    probe.add_argument("--workspace", required=True)
    probe.add_argument("--canary", required=True)
    probe.add_argument("--evidence", required=True)
    probe.add_argument("--timeout", type=float, default=90.0)
    probe.add_argument("--codex", default=os.environ.get("TRACE2D_BENCH_CODEX_BIN", "codex"))
    probe.set_defaults(handler=core.run_isolation_probe)
    return parser


def main() -> int:
    configure()
    args = build_parser().parse_args()
    try:
        return int(args.handler(args))
    except (B1WrapperError, core.WrapperError, OSError, subprocess.SubprocessError) as exc:
        print(f"B1 Codex wrapper error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
