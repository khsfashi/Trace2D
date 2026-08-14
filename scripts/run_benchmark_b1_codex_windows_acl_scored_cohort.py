#!/usr/bin/env python3
"""Owner-Windows entry point for the frozen B1 scored cohort."""
from __future__ import annotations

import os
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


_ALLOWED_PREFLIGHT_BASE_ERRORS = {
    "Godot Agent Codex/MCP preflight did not complete",
    "Godot Agent Codex/MCP preflight status is budget_exceeded",
    "Godot Agent transport preflight modified the non-scored fixture",
}
_ALLOWED_GODOT_BOOTSTRAP_SETTINGS = {
    ("application", "config/features"): 'PackedStringArray("4.7")',
    ("autoload", "_mcp_game_helper"): '"*res://addons/godot_ai/runtime/game_helper.gd"',
}


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


def _clean_provider_turn(result: dict[str, Any]) -> bool:
    wrapper = result.get("wrapper", {})
    return (
        int(result.get("human_interventions", -1)) == 0
        and int(wrapper.get("process_return_code", -1)) == 0
        and wrapper.get("turn_completed") is True
    )


def _is_unscored_transport_input_budget_only(result: dict[str, Any]) -> bool:
    """Accept only a clean provider turn whose sole overage is input tokens."""
    return (
        result.get("status") == "budget_exceeded"
        and _clean_provider_turn(result)
        and result.get("budget", {}).get("exceeded") == ["input_tokens"]
    )


def _is_unscored_transport_completed(result: dict[str, Any]) -> bool:
    return (
        result.get("status") == "completed"
        and _clean_provider_turn(result)
        and result.get("budget", {}).get("exceeded") == []
    )


def _project_settings(path: Path) -> dict[tuple[str, str], str]:
    section = ""
    settings: dict[tuple[str, str], str] = {}
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip()
            continue
        if "=" not in line:
            raise base.ScoredCohortError(f"unexpected project.godot line during transport preflight: {raw!r}")
        key, value = line.split("=", 1)
        identity = (section, key.strip())
        if identity in settings:
            raise base.ScoredCohortError(f"duplicate project.godot setting during transport preflight: {identity}")
        settings[identity] = value.strip()
    return settings


def _workspace_files(root: Path) -> dict[str, Path]:
    files: dict[str, Path] = {}
    for current, directories, names in os.walk(root, topdown=True, followlinks=False):
        directories[:] = sorted(name for name in directories if name.casefold() != ".godot")
        current_path = Path(current)
        for name in sorted(names):
            path = current_path / name
            if path.is_symlink() or not path.is_file():
                continue
            files[path.relative_to(root).as_posix()] = path
    return files


def _verify_unscored_transport_workspace(fixture: Path, workspace: Path) -> dict[str, Any]:
    """Reject authored changes while allowing only deterministic Godot/MCP bootstrap metadata."""
    expected_files = _workspace_files(fixture)
    observed_files = _workspace_files(workspace)

    missing = sorted(set(expected_files) - set(observed_files))
    if missing:
        raise base.ScoredCohortError(f"Godot Agent transport preflight removed authored files: {missing}")

    changed: list[str] = []
    for relative, expected_path in expected_files.items():
        if relative == "project.godot":
            continue
        if base.sha256_file(expected_path) != base.sha256_file(observed_files[relative]):
            changed.append(relative)
    if changed:
        raise base.ScoredCohortError(f"Godot Agent transport preflight changed authored files: {changed}")

    generated_metadata: list[str] = []
    unexpected_extra: list[str] = []
    for relative in sorted(set(observed_files) - set(expected_files)):
        source_relative = relative[:-4] if relative.endswith(".uid") else ""
        source_path = expected_files.get(source_relative)
        if relative.endswith(".gd.uid") and source_path is not None and source_relative.endswith(".gd"):
            generated_metadata.append(relative)
        else:
            unexpected_extra.append(relative)
    if unexpected_extra:
        raise base.ScoredCohortError(
            f"Godot Agent transport preflight created unexpected authored files: {unexpected_extra}"
        )

    expected_project = _project_settings(expected_files["project.godot"])
    observed_project = _project_settings(observed_files["project.godot"])
    ignored_settings: list[str] = []
    for identity, allowed_value in _ALLOWED_GODOT_BOOTSTRAP_SETTINGS.items():
        if identity not in expected_project and observed_project.get(identity) == allowed_value:
            observed_project.pop(identity)
            ignored_settings.append("/".join(identity))
    if observed_project != expected_project:
        expected_keys = set(expected_project)
        observed_keys = set(observed_project)
        added = sorted("/".join(key) for key in observed_keys - expected_keys)
        removed = sorted("/".join(key) for key in expected_keys - observed_keys)
        modified = sorted(
            "/".join(key)
            for key in expected_keys & observed_keys
            if expected_project[key] != observed_project[key]
        )
        raise base.ScoredCohortError(
            "Godot Agent transport preflight changed project settings; "
            f"added={added}; removed={removed}; modified={modified}"
        )

    return {
        "authored_fixture_sha256": base.benchmark_b0_stable_harness.stable_tree_hash(fixture),
        "raw_workspace_sha256": base.benchmark_b0_stable_harness.stable_tree_hash(workspace),
        "generated_metadata_paths": generated_metadata,
        "ignored_adapter_project_settings": ignored_settings,
    }


def run_owner_godot_agent_preflight(
    *,
    repo_root: Path,
    run_root: Path,
    env: dict[str, str],
) -> dict[str, Any]:
    """Preserve scored budgets while separating adapter bootstrap from authored preflight edits."""
    fallback_reason = ""
    try:
        return _QUALIFIED_RUN_GODOT_AGENT_PREFLIGHT(
            repo_root=repo_root,
            run_root=run_root,
            env=env,
        )
    except base.ScoredCohortError as exc:
        fallback_reason = str(exc)
        if fallback_reason not in _ALLOWED_PREFLIGHT_BASE_ERRORS:
            raise

    root = run_root / "godot-agent-preflight"
    result = base.load_json(root / "agent-result.json")
    if not (_is_unscored_transport_completed(result) or _is_unscored_transport_input_budget_only(result)):
        raise base.ScoredCohortError(
            "Godot Agent unscored transport preflight did not complete as a clean provider turn"
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
            "Godot Agent preflight finished without an observed successful Godot MCP result"
        )

    fixture = repo_root / "benchmarks/b1/qualification/godot_content_fixture"
    workspace = root / "workspace"
    workspace_integrity = _verify_unscored_transport_workspace(fixture, workspace)

    budget_disposition = (
        "within_scored_budget"
        if result.get("status") == "completed"
        else "input_tokens_over_scored_budget_allowed_for_unscored_probe_only"
    )
    evidence = {
        "passed": True,
        "tool_names": tool_names,
        "agent_result": result,
        "workspace_integrity": workspace_integrity,
        "base_preflight_fallback_reason": fallback_reason,
        "transport_budget_disposition": budget_disposition,
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
