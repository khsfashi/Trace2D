#!/usr/bin/env python3
"""Post-score Benchmark B2 remediation helpers.

These helpers are deliberately outside the frozen B2 task/verifier inputs. They
repair execution/reporting mechanics discovered by the immutable nine-slot
cohort without rewriting or reinterpreting the scored raw records in place.
"""
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[1]
GODOT_AI_RESOURCE_PREFIX = "res://addons/godot_ai/"
PUBLIC_API_INDEX_PATH = Path("docs/agent-public-api-v1.json")
_IMPLEMENTATION_VERIFIER_CODES = {
    "candidate_compile_or_link_failed",
    "candidate_rejected",
}


def _rewrite_editor_plugins_line(line: str) -> str | None:
    """Remove only injected godot_ai paths from editor plugin PackedStringArray."""
    if GODOT_AI_RESOURCE_PREFIX not in line:
        return line
    key, separator, value = line.partition("=")
    if not separator or key.strip() != "enabled" or "PackedStringArray" not in value:
        return None
    items = re.findall(r'"((?:\\.|[^"\\])*)"', value)
    kept = [item for item in items if not item.startswith(GODOT_AI_RESOURCE_PREFIX)]
    if not kept:
        return None
    encoded = ", ".join(json.dumps(item, ensure_ascii=False) for item in kept)
    return f"{key}=PackedStringArray({encoded})"


def remove_injected_godot_ai_plugin(project: Path) -> None:
    """Remove harness-owned Godot AI references while preserving authored settings.

    hi-godot/godot-ai can register an autoload while the editor is running. The
    old cleanup removed only the injected editor plugin line, so deleting the
    addon left a dangling ``res://addons/godot_ai/...`` autoload in project.godot.
    """
    if not project.is_file():
        return
    original = project.read_text(encoding="utf-8")
    section = ""
    output: list[str] = []
    for line in original.splitlines():
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped[1:-1].strip().casefold()
            output.append(line)
            continue
        if section == "autoload" and GODOT_AI_RESOURCE_PREFIX in line:
            continue
        if section == "editor_plugins" and GODOT_AI_RESOURCE_PREFIX in line:
            rewritten = _rewrite_editor_plugins_line(line)
            if rewritten is not None:
                output.append(rewritten)
            continue
        output.append(line)

    cleaned = "\n".join(output).rstrip() + "\n"
    if cleaned != original:
        project.write_text(cleaned, encoding="utf-8")


def load_public_api_index(repo_root: Path = REPO_ROOT) -> dict[str, Any]:
    path = repo_root / PUBLIC_API_INDEX_PATH
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("kind") != "trace2d_agent_public_api_index":
        raise ValueError(f"unexpected public API index kind: {data.get('kind')!r}")
    symbols = data.get("symbols")
    if not isinstance(symbols, list):
        raise ValueError("public API index symbols must be a list")
    return data


def public_api_symbol(qualified_name: str, repo_root: Path = REPO_ROOT) -> dict[str, Any]:
    data = load_public_api_index(repo_root)
    for symbol in data["symbols"]:
        if isinstance(symbol, dict) and symbol.get("qualified_name") == qualified_name:
            return symbol
    raise KeyError(f"public API symbol not indexed: {qualified_name}")


def public_api_include(qualified_name: str, repo_root: Path = REPO_ROOT) -> str:
    symbol = public_api_symbol(qualified_name, repo_root)
    include = symbol.get("include")
    if not isinstance(include, str) or not include:
        raise ValueError(f"public API symbol has no include: {qualified_name}")
    return include


def diagnostic_classification(record: dict[str, Any]) -> dict[str, Any]:
    """Return a non-mutating diagnostic classification for a retained B2 record."""
    raw_status = str(record.get("status", "unknown"))
    raw_domain = str(record.get("failure_domain", "unknown"))
    status = raw_status
    domain = raw_domain
    reason = "raw_classification_retained"

    verifier = record.get("deterministic_verifier")
    verdict = verifier.get("verdict", {}) if isinstance(verifier, dict) else {}
    verifier_status = verdict.get("status")
    verifier_code = verdict.get("code")

    if (
        raw_status == "agent_setup_failure"
        and verifier_status == "fail"
        and verifier_code in _IMPLEMENTATION_VERIFIER_CODES
    ):
        status = "deterministic_failure"
        domain = "implementation"
        reason = "verifier_proved_candidate_failure_after_missing_or_invalid_agent_result"

    return {
        "raw_status": raw_status,
        "raw_failure_domain": raw_domain,
        "diagnostic_status": status,
        "diagnostic_failure_domain": domain,
        "reason": reason,
        "verifier_status": verifier_status,
        "verifier_code": verifier_code,
        "raw_record_unchanged": True,
    }


def _optional_json(path_text: Any) -> dict[str, Any]:
    result: dict[str, Any] = {
        "path": str(path_text) if isinstance(path_text, str) else None,
        "present": False,
        "parse_status": "missing",
        "value": None,
    }
    if not isinstance(path_text, str) or not path_text:
        return result
    path = Path(path_text)
    if not path.is_file() or path.is_symlink():
        return result
    result["present"] = True
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        result["parse_status"] = "invalid_json"
        result["parse_error"] = f"{type(exc).__name__}: {exc}"
        return result
    if not isinstance(value, dict):
        result["parse_status"] = "non_object_json"
        return result
    result["parse_status"] = "parsed"
    result["value"] = value
    return result


def terminal_evidence(record: dict[str, Any]) -> dict[str, Any]:
    """Separate process/result/verifier evidence without mutating historical scores."""
    artifacts = record.get("artifacts") if isinstance(record.get("artifacts"), dict) else {}
    process_file = _optional_json(artifacts.get("agent_process"))
    result_file = _optional_json(artifacts.get("agent_result"))
    process_value = process_file.get("value") if isinstance(process_file.get("value"), dict) else {}

    agent_result = record.get("agent_result")
    wrapper = agent_result.get("wrapper", {}) if isinstance(agent_result, dict) else {}
    process_return_code = process_value.get("return_code", wrapper.get("process_return_code"))
    process_timed_out = process_value.get("timed_out")
    if process_timed_out is None:
        process_timed_out = record.get("status") == "timeout"

    verifier = record.get("deterministic_verifier")
    verdict = verifier.get("verdict", {}) if isinstance(verifier, dict) else {}
    verifier_process: dict[str, Any] = {}
    if isinstance(verdict, dict):
        for key in ("process", "build", "configure"):
            candidate = verdict.get(key)
            if isinstance(candidate, dict) and (
                "return_code" in candidate or "timed_out" in candidate or "stderr" in candidate
            ):
                verifier_process = candidate
                break
    stderr = verifier_process.get("stderr")
    if not isinstance(stderr, str):
        stderr = ""

    return {
        "agent_process": {
            "checkpoint_present": process_file["present"],
            "checkpoint_parse_status": process_file["parse_status"],
            "return_code": process_return_code,
            "timed_out": bool(process_timed_out),
            "duration_ms": process_value.get("duration_ms", record.get("metrics", {}).get("wall_ms") if isinstance(record.get("metrics"), dict) else None),
        },
        "structured_agent_result": {
            "artifact_present": result_file["present"],
            "artifact_parse_status": result_file["parse_status"],
            "record_embedded": isinstance(agent_result, dict),
            "identity_ok": record.get("agent_identity_ok"),
            "status": agent_result.get("status") if isinstance(agent_result, dict) else None,
        },
        "verifier": {
            "result_embedded": isinstance(verifier, dict),
            "status": verdict.get("status") if isinstance(verdict, dict) else None,
            "code": verdict.get("code") if isinstance(verdict, dict) else None,
            "process_return_code": verifier_process.get("return_code"),
            "timed_out": verifier_process.get("timed_out"),
            "stderr": stderr[:4000],
        },
    }


def budget_diagnostics(record: dict[str, Any]) -> dict[str, Any] | None:
    """Expose exact budget overshoot and token accounting without changing score."""
    agent_result = record.get("agent_result")
    if not isinstance(agent_result, dict):
        return None
    budget = agent_result.get("budget")
    if not isinstance(budget, dict):
        return None
    limits = budget.get("limits", {})
    observed = budget.get("observed", {})
    metrics = agent_result.get("metrics", {}) if isinstance(agent_result.get("metrics"), dict) else {}
    ratios: dict[str, float] = {}
    mapping = {
        "tool_calls": "max_tool_calls",
        "input_tokens": "max_input_tokens",
        "output_tokens": "max_output_tokens",
    }
    for observed_key, limit_key in mapping.items():
        value = observed.get(observed_key)
        limit = limits.get(limit_key)
        if isinstance(value, (int, float)) and isinstance(limit, (int, float)) and limit > 0:
            ratios[observed_key] = float(value) / float(limit)

    input_tokens = metrics.get("input_tokens", observed.get("input_tokens"))
    cached_input_tokens = metrics.get("cached_input_tokens")
    uncached_input_tokens = None
    if isinstance(input_tokens, int) and isinstance(cached_input_tokens, int):
        uncached_input_tokens = max(0, input_tokens - cached_input_tokens)
    return {
        "limits": limits,
        "observed": observed,
        "exceeded": list(budget.get("exceeded", [])),
        "observed_to_limit_ratio": ratios,
        "token_accounting": {
            "input_tokens": input_tokens,
            "cached_input_tokens": cached_input_tokens,
            "uncached_input_tokens": uncached_input_tokens,
            "output_tokens": metrics.get("output_tokens", observed.get("output_tokens")),
            "reasoning_output_tokens": metrics.get("reasoning_output_tokens"),
        },
    }


def integrity_diagnostics(record: dict[str, Any]) -> dict[str, Any]:
    integrity = record.get("integrity") if isinstance(record.get("integrity"), dict) else {}
    checks = {
        "schedule_prefix_valid": integrity.get("schedule_prefix_valid") is True,
        "automatic_retries_zero": integrity.get("automatic_retries") == 0,
        "replacement_trials_zero": integrity.get("replacement_trials") == 0,
        "repo_freeze_not_violated": integrity.get("repo_freeze_unchanged_after_agent") is not False,
    }
    failed = [name for name, passed in checks.items() if not passed]
    return {
        "valid": not failed,
        "checks": checks,
        "failed": failed,
        "raw_record_unchanged": True,
    }
