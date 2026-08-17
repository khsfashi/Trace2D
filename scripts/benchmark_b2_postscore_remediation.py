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

GODOT_AI_RESOURCE_PREFIX = "res://addons/godot_ai/"
TRACE2D_GAME_PUBLIC_HEADER = "trace2d/application/Application.hpp"
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
        reason = "verifier_proved_candidate_failure_after_missing_agent_result"

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


def budget_diagnostics(record: dict[str, Any]) -> dict[str, Any] | None:
    """Expose exact budget overshoot without changing the scored status."""
    agent_result = record.get("agent_result")
    if not isinstance(agent_result, dict):
        return None
    budget = agent_result.get("budget")
    if not isinstance(budget, dict):
        return None
    limits = budget.get("limits", {})
    observed = budget.get("observed", {})
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
    return {
        "limits": limits,
        "observed": observed,
        "exceeded": list(budget.get("exceeded", [])),
        "observed_to_limit_ratio": ratios,
    }
