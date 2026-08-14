#!/usr/bin/env python3
"""Owner-Windows entry point for the frozen B1 scored cohort."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

import run_benchmark_b1_codex_windows_acl_scored_cohort_base as base

GODOT_AI_WINDOWS_RUNTIME_FREEZE = Path("benchmarks/b1/godot-ai-python-freeze.windows-cp312-x64.txt")
GODOT_AI_WINDOWS_RUNTIME_FREEZE_SHA256 = "7869c1f9ee7d505696d5d2f181cb62a5156c88ceb01f4c462a615720c3d7be41"
GODOT_AI_ENV_RECIPE = "qualified-freeze-no-deps-v1+win-cp312-x64-pywin32-312-7869c1f9"

_QUALIFIED_EXPECTED_PYTHON_FREEZE = base.expected_python_freeze
_QUALIFIED_ENSURE_GODOT_AI = base.ensure_godot_ai
_QUALIFIED_RUN_GODOT_AGENT_PREFLIGHT = base.run_godot_agent_preflight


def _distribution_name(line: str) -> str:
    if " @ " in line:
        return line.split(" @ ", 1)[0].casefold()
    return line.split("==", 1)[0].casefold()


def windows_runtime_freeze(repo_root: Path) -> list[str]:
    path = repo_root / GODOT_AI_WINDOWS_RUNTIME_FREEZE
    if not path.is_file():
        raise base.ScoredCohortError(f"Godot Agent Windows runtime freeze not found: {path}")
    observed_hash = base.sha256_file(path)
    if observed_hash != GODOT_AI_WINDOWS_RUNTIME_FREEZE_SHA256:
        raise base.ScoredCohortError(
            "Godot Agent Windows runtime freeze SHA-256 mismatch: "
            f"expected {GODOT_AI_WINDOWS_RUNTIME_FREEZE_SHA256}, got {observed_hash}"
        )
    lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if lines != ["pywin32==312"]:
        raise base.ScoredCohortError("Godot Agent Windows runtime freeze changed; expected exactly pywin32==312")
    return lines


def expected_owner_python_freeze(repo_root: Path) -> list[str]:
    dependencies = list(_QUALIFIED_EXPECTED_PYTHON_FREEZE(repo_root))
    dependencies.extend(windows_runtime_freeze(repo_root))
    names = [_distribution_name(line) for line in dependencies]
    if len(names) != len(set(names)):
        raise base.ScoredCohortError("duplicate package in combined Godot Agent dependency freeze")
    return sorted(dependencies, key=_distribution_name)


def ensure_owner_godot_ai(*, repo_root: Path, local_base: Path, git: str) -> dict[str, Any]:
    result = _QUALIFIED_ENSURE_GODOT_AI(repo_root=repo_root, local_base=local_base, git=git)
    result["python_windows_runtime_freeze"] = str(GODOT_AI_WINDOWS_RUNTIME_FREEZE)
    result["python_windows_runtime_freeze_sha256"] = GODOT_AI_WINDOWS_RUNTIME_FREEZE_SHA256
    result["python_windows_runtime_packages"] = windows_runtime_freeze(repo_root)
    result["python_environment_recipe"] = GODOT_AI_ENV_RECIPE
    return result


def _is_unscored_transport_input_budget_only(result: dict[str, Any]) -> bool:
    """Accept only the observed MCP-schema input overage for the unscored transport probe."""
    return (
        result.get("status") == "budget_exceeded"
        and result.get("return_code") == 0
        and result.get("timed_out") is False
        and int(result.get("human_interventions", -1)) == 0
        and result.get("budget", {}).get("exceeded") == ["input_tokens"]
        and int(result.get("tool_metrics", {}).get("tool_failures", -1)) == 0
    )


def run_owner_godot_agent_preflight(
    *,
    repo_root: Path,
    run_root: Path,
    env: dict[str, str],
) -> dict[str, Any]:
    """Keep the scored budget frozen while treating a completed transport probe as transport success."""
    try:
        return _QUALIFIED_RUN_GODOT_AGENT_PREFLIGHT(
            repo_root=repo_root,
            run_root=run_root,
            env=env,
        )
    except base.ScoredCohortError as exc:
        if str(exc) != "Godot Agent Codex/MCP preflight status is budget_exceeded":
            raise

    root = run_root / "godot-agent-preflight"
    result = base.load_json(root / "agent-result.json")
    if not _is_unscored_transport_input_budget_only(result):
        raise base.ScoredCohortError(
            "Godot Agent unscored transport preflight exceeded a non-input budget "
            "or did not complete cleanly"
        )

    events_path = root / "codex-events.jsonl"
    events = (
        base.codex_core.parse_jsonl(events_path.read_text(encoding="utf-8"))
        if events_path.is_file()
        else []
    )
    tool_names = base.b1_wrapper.completed_mcp_tool_names(events)
    if not tool_names or not any(
        "godot" in name.casefold() or "editor" in name.casefold() for name in tool_names
    ):
        raise base.ScoredCohortError(
            "Godot Agent preflight completed over the scored input budget "
            "without an observed Godot MCP tool call"
        )

    fixture = repo_root / "benchmarks/b1/qualification/godot_content_fixture"
    workspace = root / "workspace"
    expected_tree = base.benchmark_b0_stable_harness.stable_tree_hash(fixture)
    observed_tree = base.benchmark_b0_stable_harness.stable_tree_hash(workspace)
    if observed_tree != expected_tree:
        raise base.ScoredCohortError(
            "Godot Agent transport preflight modified the non-scored fixture"
        )

    evidence = {
        "passed": True,
        "tool_names": tool_names,
        "workspace_sha256": observed_tree,
        "agent_result": result,
        "transport_budget_disposition": "input_tokens_over_scored_budget_allowed_for_unscored_probe_only",
        "scored_budget_unchanged": True,
    }
    base.write_json(root / "preflight.json", evidence)
    return evidence


def apply_owner_windows_runtime_patch() -> None:
    base.GODOT_AI_ENV_RECIPE = GODOT_AI_ENV_RECIPE
    base.expected_python_freeze = expected_owner_python_freeze
    base.ensure_godot_ai = ensure_owner_godot_ai
    base.run_godot_agent_preflight = run_owner_godot_agent_preflight


apply_owner_windows_runtime_patch()


if __name__ == "__main__":
    try:
        raise SystemExit(base.main())
    except base.ScoredCohortError as exc:
        print(f"B1 scored cohort error: {exc}", file=sys.stderr)
        raise SystemExit(2)
