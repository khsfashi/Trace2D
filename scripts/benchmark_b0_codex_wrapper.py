#!/usr/bin/env python3
"""Frozen OpenAI Codex CLI wrapper for Trace2D Benchmark B0.

The wrapper is intentionally provider-thin: it runs one exact Codex CLI/model
configuration, preserves the JSONL trajectory, derives provider-reported usage,
and writes the agent-result contract consumed by benchmark_b0.py.

It also exposes an isolation probe used before scored eligibility. The probe is
not a benchmark trial; it proves that the named Codex filesystem permission
profile can write the candidate workspace while denying reads of a canary that
lives outside every allowed root.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Iterable

EXPECTED_CODEX_VERSION = "0.144.6"
AGENT_ID = f"openai-codex-cli@{EXPECTED_CODEX_VERSION}"
MODEL_ID = "gpt-5.5"
MODEL_REVISION = "gpt-5.5-2026-04-23"
REASONING_EFFORT = "high"
PERMISSION_PROFILE = "trace2d_b0_isolated"


class WrapperError(RuntimeError):
    pass


def toml_string(value: str) -> str:
    # JSON strings are valid TOML basic strings for the path/text values used here.
    return json.dumps(value, ensure_ascii=False)


def batch_aware_argv(executable: str, args: Iterable[str]) -> list[str]:
    values = [str(value) for value in args]
    suffix = Path(executable).suffix.lower()
    if os.name == "nt" and suffix in {".cmd", ".bat"}:
        comspec = os.environ.get("COMSPEC", "cmd.exe")
        command_line = subprocess.list2cmdline([executable, *values])
        return [comspec, "/d", "/s", "/c", command_line]
    return [executable, *values]


def capture(
    executable: str,
    args: Iterable[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    stdin_text: str | None = None,
    timeout: float = 30.0,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        batch_aware_argv(executable, args),
        cwd=cwd,
        env=env,
        input=stdin_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        check=False,
    )


def require_file(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise WrapperError(f"{label} does not exist: {resolved}")
    return resolved


def resolve_codex(executable: str) -> str:
    found = shutil.which(executable)
    if found is None:
        candidate = Path(executable).expanduser()
        if candidate.is_file():
            found = str(candidate.resolve())
    if found is None:
        raise WrapperError(f"Codex executable not found: {executable}")
    return found


def verify_codex_version(codex: str, cwd: Path) -> str:
    completed = capture(codex, ["--version"], cwd=cwd, timeout=15.0)
    text = (completed.stdout + "\n" + completed.stderr).strip()
    match = re.search(r"codex-cli\s+([0-9]+\.[0-9]+\.[0-9]+)", text)
    if completed.returncode != 0 or match is None:
        raise WrapperError(f"unable to read Codex CLI version: {text}")
    actual = match.group(1)
    if actual != EXPECTED_CODEX_VERSION:
        raise WrapperError(
            f"Codex CLI version mismatch: expected {EXPECTED_CODEX_VERSION}, got {actual}"
        )
    return actual


def discover_codex_package_root(codex: str, cwd: Path) -> Path | None:
    # npm installs expose codex.cmd beside npm.cmd while the executable/runtime
    # assets live under the global @openai/codex package. Allow that exact package
    # subtree read-only so a restrictive filesystem profile does not block Codex's
    # own runtime helpers.
    npm = shutil.which("npm")
    if npm is None:
        return None
    try:
        completed = capture(npm, ["root", "-g"], cwd=cwd, timeout=15.0)
    except (OSError, subprocess.SubprocessError):
        return None
    if completed.returncode != 0:
        return None
    root = Path(completed.stdout.strip()) / "@openai" / "codex"
    return root.resolve() if root.is_dir() else None


def unique_roots(values: Iterable[Path]) -> list[Path]:
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


def write_isolated_config(
    *,
    codex_home: Path,
    workspace: Path,
    read_only_roots: list[Path],
    lane: str,
    godot_mcp_server: Path | None,
    trace2d_mcp_server: Path | None,
) -> Path:
    lines = [
        f"model = {toml_string(MODEL_REVISION)}",
        f"model_reasoning_effort = {toml_string(REASONING_EFFORT)}",
        'approval_policy = "never"',
        f"default_permissions = {toml_string(PERMISSION_PROFILE)}",
        'web_search = "disabled"',
        "allow_login_shell = false",
        "",
        f"[permissions.{PERMISSION_PROFILE}.filesystem]",
        '":minimal" = "read"',
    ]
    for root in read_only_roots:
        lines.append(f"{toml_string(str(root))} = \"read\"")
    lines.extend(
        [
            "",
            f"[permissions.{PERMISSION_PROFILE}.filesystem.\":workspace_roots\"]",
            '"." = "write"',
            "",
            f"[permissions.{PERMISSION_PROFILE}.network]",
            "enabled = false",
            "",
            "[shell_environment_policy]",
            'inherit = "core"',
        ]
    )

    if lane == "godot.agent":
        if godot_mcp_server is None:
            raise WrapperError("TRACE2D_BENCH_GODOT_MCP_SERVER is required for godot.agent")
        if os.name == "nt" and godot_mcp_server.suffix.lower() in {".cmd", ".bat"}:
            command = os.environ.get("COMSPEC", "cmd.exe")
            command_line = subprocess.list2cmdline([str(godot_mcp_server)])
            mcp_args = ["/d", "/s", "/c", command_line]
        else:
            command = str(godot_mcp_server)
            mcp_args = []
        lines.extend(
            [
                "",
                "[mcp_servers.godot]",
                f"command = {toml_string(command)}",
                "args = [" + ", ".join(toml_string(value) for value in mcp_args) + "]",
                "startup_timeout_sec = 20",
                "tool_timeout_sec = 45",
            ]
        )
    elif lane == "trace2d.agent":
        if trace2d_mcp_server is None:
            raise WrapperError("TRACE2D_BENCH_TRACE2D_MCP_BIN is required for trace2d.agent")
        scene = workspace / "scene.trace2d.toml"
        lines.extend(
            [
                "",
                "[mcp_servers.trace2d]",
                f"command = {toml_string(str(trace2d_mcp_server))}",
                "args = ["
                + ", ".join(
                    toml_string(value)
                    for value in ["--scene", str(scene), "--seed", "42"]
                )
                + "]",
                "startup_timeout_sec = 10",
                "tool_timeout_sec = 30",
            ]
        )

    codex_home.mkdir(parents=True, exist_ok=True)
    config = codex_home / "config.toml"
    config.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return config


def copy_auth(codex_home: Path) -> Path:
    raw = os.environ.get("TRACE2D_BENCH_CODEX_AUTH_FILE", "")
    source = Path(raw).expanduser() if raw else Path.home() / ".codex" / "auth.json"
    source = require_file(source, "Codex auth file")
    target = codex_home / "auth.json"
    shutil.copy2(source, target)
    return target


def env_path(value: str) -> Path | None:
    raw = os.environ.get(value, "").strip()
    if not raw:
        return None
    return require_file(Path(raw), value)


def tool_roots_for_lane(codex: str, workspace: Path, lane: str) -> tuple[list[Path], dict[str, Path | None]]:
    roots: list[Path] = [Path(codex).resolve().parent]
    package_root = discover_codex_package_root(codex, workspace)
    if package_root is not None:
        roots.append(package_root)

    godot_bin = env_path("TRACE2D_BENCH_GODOT_BIN") if lane.startswith("godot.") else None
    trace2d_bin = env_path("TRACE2D_BENCH_TRACE2D_BIN") if lane == "trace2d.agent" else None
    trace2d_mcp = env_path("TRACE2D_BENCH_TRACE2D_MCP_BIN") if lane == "trace2d.agent" else None
    godot_mcp = env_path("TRACE2D_BENCH_GODOT_MCP_SERVER") if lane == "godot.agent" else None

    for path in (godot_bin, trace2d_bin, trace2d_mcp, godot_mcp):
        if path is not None:
            roots.append(path.parent)

    extra = os.environ.get("TRACE2D_BENCH_CODEX_READ_ROOTS", "")
    for item in extra.split(os.pathsep):
        if item.strip():
            roots.append(Path(item.strip()))

    return unique_roots(roots), {
        "godot_bin": godot_bin,
        "trace2d_bin": trace2d_bin,
        "trace2d_mcp": trace2d_mcp,
        "godot_mcp": godot_mcp,
    }


def enable_godot_mcp_plugin(project: Path) -> bool:
    text = project.read_text(encoding="utf-8")
    plugin = 'enabled=PackedStringArray("res://addons/godot_mcp/plugin.cfg")'
    if plugin in text:
        return False
    if "[editor_plugins]" in text:
        raise WrapperError("project already has an unrelated [editor_plugins] section")
    addition = "\n[editor_plugins]\n" + plugin + "\n"
    project.write_text(text.rstrip() + "\n" + addition, encoding="utf-8")
    return True


def remove_injected_godot_mcp_plugin(project: Path) -> None:
    if not project.is_file():
        return
    text = project.read_text(encoding="utf-8")
    pattern = re.compile(
        r"\n?\[editor_plugins\]\s*\n"
        r'enabled=PackedStringArray\("res://addons/godot_mcp/plugin\.cfg"\)\s*\n?',
        re.MULTILINE,
    )
    cleaned, count = pattern.subn("\n", text, count=1)
    if count:
        project.write_text(cleaned.rstrip() + "\n", encoding="utf-8")


def install_godot_mcp_addon(server: Path, workspace: Path) -> None:
    completed = capture(str(server), ["--install-addon", str(workspace)], cwd=workspace, timeout=60.0)
    if completed.returncode != 0:
        raise WrapperError(
            "Godot MCP addon installation failed:\n" + completed.stdout + "\n" + completed.stderr
        )
    plugin = workspace / "addons" / "godot_mcp" / "plugin.cfg"
    if not plugin.is_file():
        raise WrapperError(f"Godot MCP addon install did not produce {plugin}")


def start_godot_editor(godot: Path, workspace: Path, log_path: Path) -> tuple[subprocess.Popen[str], Any]:
    log_stream = log_path.open("w", encoding="utf-8")
    creationflags = 0
    if os.name == "nt":
        creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    process = subprocess.Popen(
        [
            str(godot),
            "--editor",
            "--path",
            str(workspace),
            "--rendering-method",
            "gl_compatibility",
        ],
        cwd=workspace,
        stdout=log_stream,
        stderr=subprocess.STDOUT,
        text=True,
        creationflags=creationflags,
    )
    time.sleep(3.0)
    if process.poll() is not None:
        log_stream.flush()
        log_stream.close()
        raise WrapperError(
            f"Godot editor exited before MCP qualification session; see {log_path}"
        )
    return process, log_stream


def stop_process(process: subprocess.Popen[str] | None, stream: Any | None) -> None:
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=8.0)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5.0)
    if stream is not None:
        stream.close()


def parse_jsonl(text: str) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            raise WrapperError(f"Codex --json emitted malformed JSONL at line {number}: {exc}") from exc
        if not isinstance(value, dict):
            raise WrapperError(f"Codex --json emitted non-object JSON at line {number}")
        events.append(value)
    return events


def item_from_event(event: dict[str, Any]) -> dict[str, Any] | None:
    item = event.get("item")
    return item if isinstance(item, dict) else None


def completed_items(events: list[dict[str, Any]]) -> list[dict[str, Any]]:
    values: list[dict[str, Any]] = []
    for event in events:
        if event.get("type") != "item.completed":
            continue
        item = item_from_event(event)
        if item is not None:
            values.append(item)
    return values


def token_usage(events: list[dict[str, Any]]) -> dict[str, int]:
    usage: dict[str, Any] = {}
    for event in events:
        if event.get("type") == "turn.completed" and isinstance(event.get("usage"), dict):
            usage = event["usage"]
    def integer(*keys: str) -> int:
        for key in keys:
            value = usage.get(key)
            if isinstance(value, int) and not isinstance(value, bool):
                return value
        return 0
    return {
        "input_tokens": integer("input_tokens", "inputTokens"),
        "cached_input_tokens": integer("cached_input_tokens", "cachedInputTokens"),
        "output_tokens": integer("output_tokens", "outputTokens"),
        "reasoning_output_tokens": integer("reasoning_output_tokens", "reasoningOutputTokens"),
    }


def tool_metrics(events: list[dict[str, Any]]) -> dict[str, Any]:
    native: collections.Counter[str] = collections.Counter()
    normalized: collections.Counter[str] = collections.Counter()
    revisions = 0
    tool_calls = 0

    non_tools = {"agent_message", "reasoning", "error"}
    for item in completed_items(events):
        item_type = str(item.get("type", "unknown"))
        if item_type in non_tools:
            continue
        tool_calls += 1
        native[item_type] += 1
        if item_type == "file_change":
            revisions += 1
            normalized["file_write"] += 1
        elif item_type == "command_execution":
            normalized["shell"] += 1
        elif item_type in {"mcp_tool_call", "dynamic_tool_call"}:
            tool_name = str(item.get("tool", item.get("name", "")))
            native_name = f"{item_type}:{tool_name}" if tool_name else item_type
            native[native_name] += 1
            lowered = tool_name.lower()
            if any(part in lowered for part in ("inspect", "read", "state", "query", "digest")):
                normalized["runtime_inspect"] += 1
            elif any(part in lowered for part in ("input", "step", "run")):
                normalized["runtime_input"] += 1
            else:
                normalized["engine_tool"] += 1
        else:
            normalized["other_tool"] += 1

    return {
        "revisions": revisions,
        "tool_calls": tool_calls,
        "normalized_operations": dict(sorted(normalized.items())),
        "engine_native_operations": dict(sorted(native.items())),
    }


def saw_turn_completed(events: list[dict[str, Any]]) -> bool:
    return any(event.get("type") == "turn.completed" for event in events)


def command_texts(events: list[dict[str, Any]]) -> list[str]:
    output: list[str] = []
    for item in completed_items(events):
        if item.get("type") != "command_execution":
            continue
        command = item.get("command")
        if isinstance(command, str):
            output.append(command)
        elif isinstance(command, list):
            output.append(" ".join(str(value) for value in command))
    return output


def codex_environment(codex_home: Path, read_roots: list[Path]) -> dict[str, str]:
    env = os.environ.copy()
    env["CODEX_HOME"] = str(codex_home)
    env["CODEX_CI"] = "1"
    path_roots = [str(root) for root in read_roots]
    current_path = env.get("PATH", "")
    if current_path:
        path_roots.append(current_path)
    env["PATH"] = os.pathsep.join(path_roots)
    return env


def run_codex(
    *,
    codex: str,
    workspace: Path,
    codex_home: Path,
    read_roots: list[Path],
    prompt: str,
    timeout: float,
) -> tuple[subprocess.CompletedProcess[str], list[dict[str, Any]]]:
    env = codex_environment(codex_home, read_roots)
    completed = capture(
        codex,
        [
            "exec",
            "--json",
            "--ephemeral",
            "--skip-git-repo-check",
            "--strict-config",
            "-C",
            str(workspace),
            "-m",
            MODEL_REVISION,
            "-",
        ],
        cwd=workspace,
        env=env,
        stdin_text=prompt,
        timeout=timeout,
    )
    events = parse_jsonl(completed.stdout) if completed.stdout.strip() else []
    return completed, events


def common_agent_prompt(task_prompt: str) -> str:
    return (
        "You are running one autonomous benchmark trial.\n"
        "Hard execution rules:\n"
        "- Work only inside the current workspace.\n"
        "- Do not attempt to read parent, sibling, benchmark harness, verifier, or repository paths.\n"
        "- Do not ask a human for help or approval.\n"
        "- Use only the normal tools exposed in this session; no task-specific hidden shortcuts exist.\n"
        "- Finish the authored workspace itself; a separate verifier will judge it after you exit.\n\n"
        "Task:\n"
        + task_prompt.strip()
        + "\n"
    )


def setup_codex_home(
    *,
    codex: str,
    workspace: Path,
    lane: str,
    result_root: Path,
    read_roots: list[Path],
    godot_mcp: Path | None,
    trace2d_mcp: Path | None,
) -> Path:
    codex_home = result_root / "codex-home"
    if codex_home.exists():
        shutil.rmtree(codex_home)
    codex_home.mkdir(parents=True)
    copy_auth(codex_home)
    write_isolated_config(
        codex_home=codex_home,
        workspace=workspace,
        read_only_roots=read_roots,
        lane=lane,
        godot_mcp_server=godot_mcp,
        trace2d_mcp_server=trace2d_mcp,
    )
    return codex_home


def run_trial(args: argparse.Namespace) -> int:
    workspace = Path(args.workspace).resolve()
    if not workspace.is_dir():
        raise WrapperError(f"workspace does not exist: {workspace}")
    prompt_path = require_file(Path(args.prompt_file), "prompt file")
    result_path = Path(args.result_file).resolve()
    result_path.parent.mkdir(parents=True, exist_ok=True)
    codex = resolve_codex(args.codex)
    verify_codex_version(codex, workspace)

    read_roots, tools = tool_roots_for_lane(codex, workspace, args.lane)
    codex_home = setup_codex_home(
        codex=codex,
        workspace=workspace,
        lane=args.lane,
        result_root=result_path.parent,
        read_roots=read_roots,
        godot_mcp=tools["godot_mcp"],
        trace2d_mcp=tools["trace2d_mcp"],
    )

    editor: subprocess.Popen[str] | None = None
    editor_log_stream: Any | None = None
    injected_plugin = False
    try:
        if args.lane == "godot.agent":
            godot = tools["godot_bin"]
            godot_mcp = tools["godot_mcp"]
            if godot is None or godot_mcp is None:
                raise WrapperError("godot.agent requires pinned Godot and Godot MCP paths")
            project = require_file(workspace / "project.godot", "Godot project")
            install_godot_mcp_addon(godot_mcp, workspace)
            injected_plugin = enable_godot_mcp_plugin(project)
            editor, editor_log_stream = start_godot_editor(
                godot, workspace, result_path.parent / "godot-editor.log"
            )

        prompt = common_agent_prompt(prompt_path.read_text(encoding="utf-8"))
        completed, events = run_codex(
            codex=codex,
            workspace=workspace,
            codex_home=codex_home,
            read_roots=read_roots,
            prompt=prompt,
            timeout=float(os.environ.get("TRACE2D_BENCH_WRAPPER_TIMEOUT", "290")),
        )
    finally:
        stop_process(editor, editor_log_stream)
        if args.lane == "godot.agent":
            project = workspace / "project.godot"
            if injected_plugin:
                remove_injected_godot_mcp_plugin(project)
            shutil.rmtree(workspace / "addons" / "godot_mcp", ignore_errors=True)
            addons = workspace / "addons"
            if addons.is_dir() and not any(addons.iterdir()):
                addons.rmdir()

    events_path = result_path.parent / "codex-events.jsonl"
    events_path.write_text(completed.stdout, encoding="utf-8")
    (result_path.parent / "codex.stderr.txt").write_text(completed.stderr, encoding="utf-8")

    usage = token_usage(events)
    tools_metric = tool_metrics(events)
    max_tool_calls = int(os.environ.get("TRACE2D_BENCH_MAX_TOOL_CALLS", "80"))
    max_input_tokens = int(os.environ.get("TRACE2D_BENCH_MAX_INPUT_TOKENS", "100000"))
    max_output_tokens = int(os.environ.get("TRACE2D_BENCH_MAX_OUTPUT_TOKENS", "20000"))
    budget_ok = (
        tools_metric["tool_calls"] <= max_tool_calls
        and usage["input_tokens"] <= max_input_tokens
        and usage["output_tokens"] <= max_output_tokens
    )
    completed_ok = completed.returncode == 0 and saw_turn_completed(events) and budget_ok

    result = {
        "schema_version": 1,
        "status": "completed" if completed_ok else "tool_transport_failure",
        "model": {
            "agent_id": AGENT_ID,
            "model_id": MODEL_ID,
            "model_revision": MODEL_REVISION,
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
        "wrapper": {
            "codex_version": EXPECTED_CODEX_VERSION,
            "reasoning_effort": REASONING_EFFORT,
            "permission_profile": PERMISSION_PROFILE,
            "ephemeral": True,
            "approval_policy": "never",
            "web_search": "disabled",
            "process_return_code": completed.returncode,
            "turn_completed": saw_turn_completed(events),
            "budget_ok": budget_ok,
            "trajectory": str(events_path),
        },
    }
    result_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0 if completed_ok else 1


def run_isolation_probe(args: argparse.Namespace) -> int:
    workspace = Path(args.workspace).resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    canary = require_file(Path(args.canary), "isolation canary")
    if canary.is_relative_to(workspace):
        raise WrapperError("isolation canary must live outside the allowed workspace")

    codex = resolve_codex(args.codex)
    verify_codex_version(codex, workspace)
    read_roots, _ = tool_roots_for_lane(codex, workspace, "godot.generic")
    for root in read_roots:
        try:
            canary.relative_to(root)
        except ValueError:
            continue
        raise WrapperError(f"isolation canary accidentally falls under allowed read root: {root}")

    allowed_secret = f"ALLOWED-{os.urandom(12).hex()}"
    denied_secret = canary.read_text(encoding="utf-8").strip()
    allowed_source = workspace / "ISOLATION_ALLOWED_SOURCE.txt"
    allowed_result = workspace / "ISOLATION_ALLOWED_RESULT.txt"
    denied_result = workspace / "ISOLATION_DENIED_RESULT.txt"
    allowed_source.write_text(allowed_secret, encoding="utf-8")

    result_root = workspace / ".probe-artifacts"
    codex_home = setup_codex_home(
        codex=codex,
        workspace=workspace,
        lane="godot.generic",
        result_root=result_root,
        read_roots=read_roots,
        godot_mcp=None,
        trace2d_mcp=None,
    )
    prompt = (
        "This is a filesystem-isolation probe, not a coding task. You must use the shell tool.\n"
        f"1. Read exactly {allowed_source} and write the exact contents to {allowed_result}.\n"
        f"2. Attempt to read exactly {canary} with the shell tool.\n"
        f"3. If that external read succeeds, write its exact contents to {denied_result}. "
        f"If the shell reports access denied/not permitted, write exactly DENIED to {denied_result}.\n"
        "Do not guess either file's contents.\n"
    )
    completed, events = run_codex(
        codex=codex,
        workspace=workspace,
        codex_home=codex_home,
        read_roots=read_roots,
        prompt=prompt,
        timeout=float(args.timeout),
    )
    commands = command_texts(events)
    attempted = any(os.path.normcase(str(canary)) in os.path.normcase(command) for command in commands)
    allowed_ok = allowed_result.is_file() and allowed_result.read_text(encoding="utf-8").strip() == allowed_secret
    denied_value = denied_result.read_text(encoding="utf-8").strip() if denied_result.is_file() else ""
    secret_leaked = denied_secret in completed.stdout or denied_secret in denied_value
    denied_ok = denied_value == "DENIED" and not secret_leaked
    passed = (
        completed.returncode == 0
        and saw_turn_completed(events)
        and attempted
        and allowed_ok
        and denied_ok
    )

    evidence = {
        "schema_version": 1,
        "kind": "codex_filesystem_isolation_probe",
        "passed": passed,
        "agent_id": AGENT_ID,
        "model_id": MODEL_ID,
        "model_revision": MODEL_REVISION,
        "reasoning_effort": REASONING_EFFORT,
        "permission_profile": PERMISSION_PROFILE,
        "workspace_write_proved": allowed_ok,
        "external_read_attempt_observed": attempted,
        "external_read_denied": denied_ok,
        "canary_secret_leaked": secret_leaked,
        "codex_return_code": completed.returncode,
        "turn_completed": saw_turn_completed(events),
        "commands": commands,
        "usage": token_usage(events),
    }
    output = Path(args.evidence).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(evidence, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (output.parent / "isolation-probe-events.jsonl").write_text(completed.stdout, encoding="utf-8")
    (output.parent / "isolation-probe.stderr.txt").write_text(completed.stderr, encoding="utf-8")
    print(json.dumps(evidence, indent=2, ensure_ascii=False))
    return 0 if passed else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D B0 frozen Codex CLI wrapper")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="run one benchmark Agent trial")
    run.add_argument("--workspace", required=True)
    run.add_argument("--prompt-file", required=True)
    run.add_argument("--lane", choices=("godot.generic", "godot.agent", "trace2d.agent"), required=True)
    run.add_argument("--result-file", required=True)
    run.add_argument("--codex", default=os.environ.get("TRACE2D_BENCH_CODEX_BIN", "codex"))
    run.set_defaults(handler=run_trial)

    probe = subparsers.add_parser("probe-isolation", help="prove workspace-only filesystem visibility")
    probe.add_argument("--workspace", required=True)
    probe.add_argument("--canary", required=True)
    probe.add_argument("--evidence", required=True)
    probe.add_argument("--timeout", type=float, default=90.0)
    probe.add_argument("--codex", default=os.environ.get("TRACE2D_BENCH_CODEX_BIN", "codex"))
    probe.set_defaults(handler=run_isolation_probe)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        return int(args.handler(args))
    except (WrapperError, OSError, subprocess.SubprocessError) as exc:
        print(f"B0 Codex wrapper error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
