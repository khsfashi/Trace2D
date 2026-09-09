#!/usr/bin/env python3
"""Deterministic descriptor-first Trace2D Agent Skill discovery."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys
from typing import Any

REGISTRY_KIND = "trace2d-agent-skill-registry"
TOKEN_RE = re.compile(r"[a-z0-9]+", re.IGNORECASE)


class SkillContractError(RuntimeError):
    pass


def _default_registry() -> Path:
    override = os.environ.get("TRACE2D_AGENT_SKILL_REGISTRY")
    if override:
        return Path(override)

    script = Path(__file__).resolve()
    candidates = [
        script.parent.parent / "docs" / "agent" / "skills" / "registry-v1.json",
        script.parent.parent / "agent" / "skills" / "registry-v1.json",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return candidates[0]


def _read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise SkillContractError(f"registry_not_found: {path}") from exc
    except (OSError, json.JSONDecodeError) as exc:
        raise SkillContractError(f"registry_invalid_json: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SkillContractError("registry_root_must_be_object")
    return value


def _frontmatter(text: str) -> dict[str, str]:
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        raise SkillContractError("skill_body_missing_frontmatter")
    result: dict[str, str] = {}
    for line in lines[1:]:
        if line.strip() == "---":
            return result
        if ":" not in line:
            raise SkillContractError("skill_body_invalid_frontmatter")
        key, value = line.split(":", 1)
        result[key.strip()] = value.strip()
    raise SkillContractError("skill_body_unterminated_frontmatter")


def _descriptor(skill: dict[str, Any]) -> dict[str, Any]:
    return {key: value for key, value in skill.items() if key != "body_content"}


def validate_registry(path: Path) -> dict[str, Any]:
    registry = _read_json(path)
    if registry.get("schema_version") != 1:
        raise SkillContractError("unsupported_registry_schema_version")
    if registry.get("kind") != REGISTRY_KIND:
        raise SkillContractError("unexpected_registry_kind")
    skills = registry.get("skills")
    if not isinstance(skills, list) or not skills:
        raise SkillContractError("registry_requires_skills")

    seen: set[str] = set()
    root = path.resolve().parent
    for skill in skills:
        if not isinstance(skill, dict):
            raise SkillContractError("skill_descriptor_must_be_object")
        skill_id = skill.get("id")
        description = skill.get("description")
        body = skill.get("body")
        if not isinstance(skill_id, str) or not skill_id:
            raise SkillContractError("skill_id_required")
        if skill_id in seen:
            raise SkillContractError(f"duplicate_skill_id: {skill_id}")
        seen.add(skill_id)
        if skill.get("version") != 1:
            raise SkillContractError(f"unsupported_skill_version: {skill_id}")
        if not isinstance(description, str) or not description:
            raise SkillContractError(f"skill_description_required: {skill_id}")
        if not isinstance(body, str) or not body:
            raise SkillContractError(f"skill_body_required: {skill_id}")
        if "body_content" in skill:
            raise SkillContractError(f"descriptor_embeds_body: {skill_id}")
        for list_key in ("match", "required_capabilities", "related_capabilities", "surfaces"):
            value = skill.get(list_key)
            if not isinstance(value, list) or not all(isinstance(item, str) and item for item in value):
                raise SkillContractError(f"skill_{list_key}_must_be_string_list: {skill_id}")

        candidate = (root / body).resolve()
        try:
            candidate.relative_to(root)
        except ValueError as exc:
            raise SkillContractError(f"skill_body_escapes_registry: {skill_id}") from exc
        try:
            body_text = candidate.read_text(encoding="utf-8")
        except OSError as exc:
            raise SkillContractError(f"skill_body_unreadable: {skill_id}: {candidate}") from exc
        if len(body_text.encode("utf-8")) > 8192:
            raise SkillContractError(f"skill_body_too_large: {skill_id}")
        meta = _frontmatter(body_text)
        if meta.get("name") != skill_id:
            raise SkillContractError(f"skill_frontmatter_name_mismatch: {skill_id}")
        if meta.get("description") != description:
            raise SkillContractError(f"skill_frontmatter_description_mismatch: {skill_id}")

    return registry


def _tokens(text: str) -> set[str]:
    return {match.group(0).lower() for match in TOKEN_RE.finditer(text)}


def search_skills(registry: dict[str, Any], query: str) -> list[dict[str, Any]]:
    normalized = query.casefold().strip()
    query_tokens = _tokens(normalized)
    scored: list[tuple[int, str, dict[str, Any]]] = []
    for skill in registry["skills"]:
        score = 0
        skill_id = skill["id"]
        if normalized == skill_id.casefold():
            score += 100
        score += 4 * len(query_tokens & _tokens(skill_id))
        score += len(query_tokens & _tokens(skill["description"].casefold()))
        for term in skill["match"]:
            lowered = term.casefold()
            if lowered in normalized:
                score += 12
            score += 2 * len(query_tokens & _tokens(lowered))
        if score > 0:
            scored.append((score, skill_id, _descriptor(skill)))

    scored.sort(key=lambda item: (-item[0], item[1]))
    return [descriptor for _, _, descriptor in scored]


def get_skill(registry: dict[str, Any], path: Path, skill_id: str) -> dict[str, Any]:
    for skill in registry["skills"]:
        if skill["id"] == skill_id:
            body_path = path.resolve().parent / skill["body"]
            body = body_path.read_text(encoding="utf-8")
            return {"descriptor": _descriptor(skill), "body": body}
    raise SkillContractError(f"skill_not_found: {skill_id}")


def _emit(value: dict[str, Any]) -> None:
    print(json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path, default=_default_registry())
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("validate")
    subparsers.add_parser("list")
    search = subparsers.add_parser("search")
    search.add_argument("query")
    get = subparsers.add_parser("get")
    get.add_argument("skill_id")
    args = parser.parse_args(argv)

    try:
        registry = validate_registry(args.registry)
        if args.command == "validate":
            _emit({"status": "ok", "registry": str(args.registry), "skill_count": len(registry["skills"])})
        elif args.command == "list":
            _emit({"status": "ok", "skills": [_descriptor(skill) for skill in registry["skills"]]})
        elif args.command == "search":
            matches = search_skills(registry, args.query)
            _emit({"status": "ok", "query": args.query, "matches": matches})
        elif args.command == "get":
            _emit({"status": "ok", "skill": get_skill(registry, args.registry, args.skill_id)})
        return 0
    except (SkillContractError, OSError) as exc:
        _emit({"status": "error", "error": {"code": "skill_contract_error", "message": str(exc)}})
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
