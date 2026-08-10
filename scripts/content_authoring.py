#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any, Iterable

SCHEMA_VERSION = 1
REQUEST_KIND = "trace2d-authoring-request"
DRAFT_KIND = "trace2d-authoring-draft"
ARTICLE_MODES = {
    "engineering-thesis",
    "practical-technical-explanation",
    "development-log",
}
BOUNDARY_CATEGORIES = {"not_tested", "gates", "limitations", "benchmark_metrics"}
DEFAULT_STYLE_PATH = "docs/CONTENT_AUTHOR_STYLE.md"
DEFAULT_PLATFORM_PATH = "content/platforms.json"


class AuthoringError(RuntimeError):
    pass


def canonical_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise AuthoringError(f"Missing JSON file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise AuthoringError(f"Invalid JSON in {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise AuthoringError(f"Expected a JSON object in {path}")
    return value


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def write_json(path: Path, value: dict[str, Any]) -> None:
    write_text(path, json.dumps(value, ensure_ascii=False, indent=2, sort_keys=False) + "\n")


def resolve_candidate_path(repo_root: Path, value: str) -> Path:
    direct = Path(value)
    if not direct.is_absolute():
        direct = repo_root / direct
    if direct.is_file():
        return direct

    candidate_dir = repo_root / "content" / "candidates"
    matches: list[Path] = []
    if candidate_dir.is_dir():
        for path in candidate_dir.glob("*.json"):
            try:
                data = load_json(path)
            except AuthoringError:
                continue
            if data.get("candidate_id") == value:
                matches.append(path)
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        raise AuthoringError(f"Candidate id '{value}' is ambiguous")
    raise AuthoringError(f"Candidate not found: {value}")


def validate_candidate(path: Path, data: dict[str, Any]) -> None:
    if data.get("schema_version") != SCHEMA_VERSION:
        raise AuthoringError(f"Unsupported candidate schema in {path}")
    candidate_id = data.get("candidate_id")
    if not isinstance(candidate_id, str) or not candidate_id.strip():
        raise AuthoringError(f"Candidate {path} is missing candidate_id")
    if not isinstance(data.get("facts"), dict):
        raise AuthoringError(f"Candidate {path} is missing facts object")


def load_candidates(repo_root: Path, values: Iterable[str]) -> list[tuple[Path, dict[str, Any]]]:
    result: list[tuple[Path, dict[str, Any]]] = []
    seen: set[str] = set()
    for value in values:
        path = resolve_candidate_path(repo_root, value)
        data = load_json(path)
        validate_candidate(path, data)
        candidate_id = data["candidate_id"]
        if candidate_id in seen:
            continue
        seen.add(candidate_id)
        result.append((path, data))
    if not result:
        raise AuthoringError("At least one Fact Pack candidate is required")
    return result


def load_platform_profile(repo_root: Path, platform_id: str | None, registry_path: str) -> dict[str, Any] | None:
    if not platform_id:
        return None
    path = repo_root / registry_path
    data = load_json(path)
    if data.get("schema_version") != SCHEMA_VERSION:
        raise AuthoringError(f"Unsupported platform registry schema in {path}")
    platforms = data.get("platforms")
    if not isinstance(platforms, list):
        raise AuthoringError(f"Platform registry {path} must contain a platforms array")
    for item in platforms:
        if isinstance(item, dict) and item.get("id") == platform_id:
            if item.get("publication_mode") != "manual":
                raise AuthoringError(
                    f"C1 only accepts manual publication platforms; '{platform_id}' is {item.get('publication_mode')!r}"
                )
            return dict(item)
    raise AuthoringError(f"Unknown platform id '{platform_id}' in {path}")


def load_style_contract(repo_root: Path, relative_path: str) -> dict[str, Any]:
    path = repo_root / relative_path
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError as exc:
        raise AuthoringError(f"Missing author style contract: {path}") from exc

    corpus: list[str] = []
    for url in re.findall(r"https?://[^\s)>]+", text):
        clean = url.rstrip(".,")
        if clean not in corpus:
            corpus.append(clean)

    return {
        "contract_path": relative_path.replace("\\", "/"),
        "contract_sha256": sha256_text(text),
        "approved_reference_corpus": corpus,
        "contract_text": text,
    }


def flatten_facts(
    candidates: list[tuple[Path, dict[str, Any]]], repo_root: Path
) -> tuple[list[dict[str, Any]], list[str]]:
    facts: list[dict[str, Any]] = []
    boundaries: list[str] = []
    for path, candidate in candidates:
        candidate_id = candidate["candidate_id"]
        for category, items in candidate.get("facts", {}).items():
            if not isinstance(items, list):
                continue
            for index, item in enumerate(items, start=1):
                if isinstance(item, dict):
                    fact_id = item.get("id")
                    text = item.get("text")
                    evidence = item.get("evidence", [])
                else:
                    fact_id = None
                    text = item
                    evidence = []
                if not isinstance(text, str) or not text.strip():
                    continue
                if not isinstance(fact_id, str) or not fact_id.strip():
                    fact_id = f"{category}-{index:03d}"
                ref = f"{candidate_id}#{fact_id}"
                facts.append(
                    {
                        "ref": ref,
                        "candidate_id": candidate_id,
                        "fact_id": fact_id,
                        "category": category,
                        "text": text,
                        "evidence": evidence if isinstance(evidence, list) else [],
                        "source_file": path.relative_to(repo_root).as_posix()
                        if path.is_relative_to(repo_root)
                        else str(path),
                    }
                )
                if category in BOUNDARY_CATEGORIES:
                    boundaries.append(ref)
    return facts, boundaries


def collect_candidate_platform_hints(
    candidates: list[tuple[Path, dict[str, Any]]], platform_id: str | None
) -> list[dict[str, Any]]:
    if not platform_id:
        return []
    hints: list[dict[str, Any]] = []
    for _, candidate in candidates:
        candidate_id = candidate["candidate_id"]
        records = candidate.get("platform_records", [])
        if not isinstance(records, list):
            continue
        for record in records:
            if not isinstance(record, dict) or record.get("platform_id") != platform_id:
                continue
            hints.append(
                {
                    "candidate_id": candidate_id,
                    "platform_id": platform_id,
                    "state": record.get("state"),
                    "language": record.get("language"),
                    "audience": record.get("audience"),
                    "angle_tags": [x for x in record.get("angle_tags", []) if isinstance(x, str)],
                    "source_fact_refs": [
                        f"{candidate_id}#{fact_id}"
                        for fact_id in record.get("source_fact_ids", [])
                        if isinstance(fact_id, str)
                    ],
                    "notes": record.get("notes"),
                }
            )
    return hints


def normalize_list(values: Iterable[str] | None) -> list[str]:
    result: list[str] = []
    for value in values or []:
        value = value.strip()
        if value and value not in result:
            result.append(value)
    return result


def build_editorial_brief(
    *,
    maintainer_request: str,
    goal: str | None,
    audiences: Iterable[str] | None,
    platform_id: str | None,
    angles: Iterable[str] | None,
    emphasize: Iterable[str] | None,
    deemphasize: Iterable[str] | None,
    required_limitations: Iterable[str] | None,
    language: str | None,
    length: str | None,
    mode: str,
    platform_profile: dict[str, Any] | None,
    requires_current_research: bool,
) -> dict[str, Any]:
    request = maintainer_request.strip()
    if not request:
        raise AuthoringError("C1 requires a non-empty explicit maintainer request")
    if mode not in ARTICLE_MODES:
        raise AuthoringError(f"Unsupported article mode '{mode}'")

    resolved_audiences = normalize_list(audiences)
    if not resolved_audiences and platform_profile and isinstance(platform_profile.get("audience"), str):
        resolved_audiences = [platform_profile["audience"]]
    resolved_language = language or (
        platform_profile.get("default_language") if platform_profile else None
    ) or "ko"

    return {
        "maintainer_request": request,
        "goal": (goal or request).strip(),
        "audience": resolved_audiences,
        "platform_id": platform_id,
        "angle": normalize_list(angles),
        "emphasize": normalize_list(emphasize),
        "deemphasize": normalize_list(deemphasize),
        "required_limitations": normalize_list(required_limitations),
        "language": resolved_language,
        "length": length or "medium",
        "mode": mode,
        "requires_current_external_research": bool(requires_current_research),
    }


def prepare_request(
    *,
    repo_root: Path,
    candidate_values: Iterable[str],
    maintainer_request: str,
    goal: str | None = None,
    audiences: Iterable[str] | None = None,
    platform_id: str | None = None,
    angles: Iterable[str] | None = None,
    emphasize: Iterable[str] | None = None,
    deemphasize: Iterable[str] | None = None,
    required_limitations: Iterable[str] | None = None,
    language: str | None = None,
    length: str | None = None,
    mode: str = "development-log",
    requires_current_research: bool = False,
    selected_fact_refs: Iterable[str] | None = None,
    style_path: str = DEFAULT_STYLE_PATH,
    platform_registry_path: str = DEFAULT_PLATFORM_PATH,
) -> dict[str, Any]:
    repo_root = repo_root.resolve()
    candidates = load_candidates(repo_root, candidate_values)
    platform_profile = load_platform_profile(repo_root, platform_id, platform_registry_path)
    style = load_style_contract(repo_root, style_path)
    facts, boundaries = flatten_facts(candidates, repo_root)
    fact_refs = {fact["ref"] for fact in facts}

    selected_refs = normalize_list(selected_fact_refs)
    unknown = [ref for ref in selected_refs if ref not in fact_refs]
    if unknown:
        raise AuthoringError(f"Unknown selected fact reference(s): {', '.join(unknown)}")

    brief = build_editorial_brief(
        maintainer_request=maintainer_request,
        goal=goal,
        audiences=audiences,
        platform_id=platform_id,
        angles=angles,
        emphasize=emphasize,
        deemphasize=deemphasize,
        required_limitations=required_limitations,
        language=language,
        length=length,
        mode=mode,
        platform_profile=platform_profile,
        requires_current_research=requires_current_research,
    )

    candidate_sources: list[dict[str, Any]] = []
    for path, candidate in candidates:
        raw = path.read_text(encoding="utf-8")
        candidate_sources.append(
            {
                "candidate_id": candidate["candidate_id"],
                "source_file": path.relative_to(repo_root).as_posix()
                if path.is_relative_to(repo_root)
                else str(path),
                "source_sha256": sha256_text(raw),
                "source_event": candidate.get("source_event"),
                "significance": candidate.get("significance"),
                "areas": candidate.get("areas", []),
                "topics": candidate.get("topics", []),
                "sources": candidate.get("sources", {}),
                "artifacts": candidate.get("artifacts", {}),
                "references": candidate.get("references", []),
            }
        )

    request_identity = {
        "candidate_ids": [item["candidate_id"] for item in candidate_sources],
        "candidate_hashes": [item["source_sha256"] for item in candidate_sources],
        "editorial_brief": brief,
        "selected_fact_refs": selected_refs,
        "style_contract_sha256": style["contract_sha256"],
        "platform_profile": platform_profile,
        "schema_version": SCHEMA_VERSION,
    }
    request_id = f"trace2d-draft-{sha256_text(canonical_json(request_identity))[:20]}"

    return {
        "schema_version": SCHEMA_VERSION,
        "kind": REQUEST_KIND,
        "request_id": request_id,
        "trigger": {
            "type": "explicit-maintainer-request",
            "request_text": brief["maintainer_request"],
        },
        "editorial_brief": brief,
        "candidate_sources": candidate_sources,
        "factual_authority": {
            "selected_fact_refs": selected_refs,
            "facts": facts,
            "mandatory_truth_boundary_refs": boundaries,
            "boundary_categories": sorted(BOUNDARY_CATEGORIES),
        },
        "platform_profile": platform_profile,
        "candidate_platform_hints": collect_candidate_platform_hints(candidates, platform_id),
        "style": style,
        "external_research": {
            "required": bool(requires_current_research),
            "protocol": "docs/EXTERNAL_REFERENCE_PROTOCOL.md",
            "rule": "Current external facts must be refreshed from current attributable sources when required by the requested piece.",
        },
        "authoring_rules": [
            "Generate prose only for this explicit maintainer request; never treat a merge/release/benchmark event as an authoring trigger.",
            "Treat the selected Fact Packs and current repository evidence as factual authority for Trace2D claims.",
            "Do not turn implemented into tested, hosted CI into real-GPU evidence, benchmark design into benchmark result, or Agent self-report into independent verification.",
            "Do not silently drop Not-tested, gates, limitations, or benchmark evidence when they materially change a claim.",
            "Follow the Editorial Brief for angle, emphasis, audience, language, length, and mode; do not replace it with engagement optimization or promotional framing.",
            "Use the style contract for voice, pacing, and explanatory structure only; never fabricate personal anecdotes, biography, employment history, feelings, or motives.",
            "Platform profile and candidate platform hints are advisory formatting/audience metadata, not factual authority.",
            "If current external facts are needed, research them under docs/EXTERNAL_REFERENCE_PROTOCOL.md and keep attribution separate from Trace2D Fact Pack evidence.",
            "Return a clearly marked editable DRAFT. Publication is manual and outside this authoring layer.",
        ],
    }


def parse_not_material(values: Iterable[str] | None) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in values or []:
        if "|" not in value:
            raise AuthoringError("--not-material-boundary must use '<fact-ref>|<reason>'")
        ref, reason = value.split("|", 1)
        ref = ref.strip()
        reason = reason.strip()
        if not ref or not reason:
            raise AuthoringError("--not-material-boundary requires both fact ref and reason")
        result[ref] = reason
    return result


def wrap_draft(
    *,
    request_packet: dict[str, Any],
    draft_body: str,
    used_fact_refs: Iterable[str] | None = None,
    acknowledged_boundaries: Iterable[str] | None = None,
    not_material_boundaries: dict[str, str] | None = None,
) -> tuple[str, dict[str, Any]]:
    if request_packet.get("schema_version") != SCHEMA_VERSION or request_packet.get("kind") != REQUEST_KIND:
        raise AuthoringError("Input is not a supported Trace2D authoring request packet")
    body = draft_body.strip()
    if not body:
        raise AuthoringError("Draft body must not be empty")

    authority = request_packet.get("factual_authority", {})
    facts = authority.get("facts", []) if isinstance(authority, dict) else []
    valid_refs = {
        fact.get("ref")
        for fact in facts
        if isinstance(fact, dict) and isinstance(fact.get("ref"), str)
    }
    mandatory = authority.get("mandatory_truth_boundary_refs", []) if isinstance(authority, dict) else []
    mandatory_refs = [ref for ref in mandatory if isinstance(ref, str)]

    used = normalize_list(used_fact_refs)
    unknown_used = [ref for ref in used if ref not in valid_refs]
    if unknown_used:
        raise AuthoringError(f"Unknown used fact reference(s): {', '.join(unknown_used)}")

    acknowledged = set(normalize_list(acknowledged_boundaries))
    not_material = dict(not_material_boundaries or {})
    unknown_boundary = [ref for ref in acknowledged | set(not_material) if ref not in mandatory_refs]
    if unknown_boundary:
        raise AuthoringError(f"Unknown/non-boundary fact reference(s): {', '.join(sorted(unknown_boundary))}")

    missing = [ref for ref in mandatory_refs if ref not in acknowledged and ref not in not_material]
    if missing:
        raise AuthoringError(
            "Every truth-boundary fact requires an explicit disposition before storing a draft; missing: "
            + ", ".join(missing)
        )

    dispositions = []
    for ref in mandatory_refs:
        if ref in acknowledged:
            dispositions.append(
                {"fact_ref": ref, "disposition": "included-or-addressed", "reason": None}
            )
        else:
            dispositions.append(
                {"fact_ref": ref, "disposition": "not-material", "reason": not_material[ref]}
            )

    request_id = request_packet.get("request_id")
    metadata = {
        "schema_version": SCHEMA_VERSION,
        "kind": DRAFT_KIND,
        "status": "draft",
        "publication_mode": "manual",
        "request_id": request_id,
        "used_fact_refs": used,
        "truth_boundary_dispositions": dispositions,
        "editorial_brief": request_packet.get("editorial_brief", {}),
        "candidate_ids": [
            item.get("candidate_id")
            for item in request_packet.get("candidate_sources", [])
            if isinstance(item, dict) and isinstance(item.get("candidate_id"), str)
        ],
        "style_contract_sha256": request_packet.get("style", {}).get("contract_sha256")
        if isinstance(request_packet.get("style"), dict)
        else None,
        "note": "Generated output remains an editable draft until the maintainer manually edits/approves/publishes it.",
    }
    banner = (
        "> **DRAFT — maintainer review required. Not published.**\n"
        f"> Authoring request: `{request_id}`\n\n"
    )
    return banner + body + "\n", metadata


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D explicit C1 authoring request tooling")
    parser.add_argument("--repo-root", default=".", help="Trace2D repository root")
    sub = parser.add_subparsers(dest="command", required=True)

    prepare = sub.add_parser(
        "prepare", help="Build an authoring packet from explicit maintainer intent + Fact Packs"
    )
    prepare.add_argument("--candidate", action="append", required=True, help="Candidate path or candidate_id; repeatable")
    prepare.add_argument("--maintainer-request", required=True, help="Exact explicit request that authorizes this one draft")
    prepare.add_argument("--goal")
    prepare.add_argument("--audience", action="append")
    prepare.add_argument("--platform")
    prepare.add_argument("--angle", action="append")
    prepare.add_argument("--emphasize", action="append")
    prepare.add_argument("--deemphasize", action="append")
    prepare.add_argument("--required-limitation", action="append")
    prepare.add_argument("--language")
    prepare.add_argument("--length", choices=["short", "medium", "long"])
    prepare.add_argument("--mode", choices=sorted(ARTICLE_MODES), default="development-log")
    prepare.add_argument("--requires-current-research", action="store_true")
    prepare.add_argument("--selected-fact-ref", action="append")
    prepare.add_argument("--style-path", default=DEFAULT_STYLE_PATH)
    prepare.add_argument("--platform-registry", default=DEFAULT_PLATFORM_PATH)
    prepare.add_argument("--output", help="Optional packet JSON output; otherwise stdout")

    wrap = sub.add_parser(
        "wrap-draft", help="Mark an Agent-authored draft and record truth-boundary dispositions"
    )
    wrap.add_argument("--request", required=True, help="Authoring request packet JSON")
    wrap.add_argument("--draft", required=True, help="Editable Markdown body produced for the explicit request")
    wrap.add_argument("--used-fact-ref", action="append")
    wrap.add_argument("--ack-boundary", action="append", help="Boundary fact explicitly included/addressed in the draft")
    wrap.add_argument(
        "--not-material-boundary",
        action="append",
        help="Boundary disposition '<fact-ref>|<reason>' when genuinely not material to this piece",
    )
    wrap.add_argument("--output", help="Optional wrapped Markdown output; otherwise stdout")
    wrap.add_argument("--metadata-output", help="Optional draft metadata JSON")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    repo_root = Path(args.repo_root).resolve()
    try:
        if args.command == "prepare":
            packet = prepare_request(
                repo_root=repo_root,
                candidate_values=args.candidate,
                maintainer_request=args.maintainer_request,
                goal=args.goal,
                audiences=args.audience,
                platform_id=args.platform,
                angles=args.angle,
                emphasize=args.emphasize,
                deemphasize=args.deemphasize,
                required_limitations=args.required_limitation,
                language=args.language,
                length=args.length,
                mode=args.mode,
                requires_current_research=args.requires_current_research,
                selected_fact_refs=args.selected_fact_ref,
                style_path=args.style_path,
                platform_registry_path=args.platform_registry,
            )
            text = json.dumps(packet, ensure_ascii=False, indent=2) + "\n"
            if args.output:
                output = Path(args.output)
                write_text(repo_root / output if not output.is_absolute() else output, text)
            else:
                sys.stdout.write(text)
            return 0

        if args.command == "wrap-draft":
            request_path = Path(args.request)
            draft_path = Path(args.draft)
            if not request_path.is_absolute():
                request_path = repo_root / request_path
            if not draft_path.is_absolute():
                draft_path = repo_root / draft_path
            packet = load_json(request_path)
            try:
                body = draft_path.read_text(encoding="utf-8")
            except FileNotFoundError as exc:
                raise AuthoringError(f"Missing draft file: {draft_path}") from exc
            wrapped, metadata = wrap_draft(
                request_packet=packet,
                draft_body=body,
                used_fact_refs=args.used_fact_ref,
                acknowledged_boundaries=args.ack_boundary,
                not_material_boundaries=parse_not_material(args.not_material_boundary),
            )
            if args.output:
                output = Path(args.output)
                write_text(repo_root / output if not output.is_absolute() else output, wrapped)
            else:
                sys.stdout.write(wrapped)
            if args.metadata_output:
                meta = Path(args.metadata_output)
                write_json(repo_root / meta if not meta.is_absolute() else meta, metadata)
            return 0

        raise AuthoringError(f"Unknown command {args.command}")
    except AuthoringError as exc:
        print(f"content authoring error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
