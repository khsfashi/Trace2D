#!/usr/bin/env python3
"""Trace2D C0 content evidence / Fact Pack tooling.

This module deliberately stops at structured evidence. It never generates article,
social, translation, or publication prose and never performs network publication.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

SCHEMA_VERSION = 1
KNOWN_TRAILERS = {
    "Issue",
    "Area",
    "Decision",
    "Constraint",
    "Rejected",
    "Directive",
    "Tested",
    "Not-tested",
    "Gate",
    "Reference",
    "Related",
    "Supersedes",
    "Content",
    "Agent",
    "Model",
}
SIGNIFICANCE = {"none", "candidate", "major", "release"}
LIFECYCLE_STATES = {"discovered", "reviewed", "selected", "parked", "discarded"}
PLATFORM_STATES = {"considered", "selected", "drafted", "authored", "published", "skipped"}
SOURCE_EVENTS = {"merge", "release", "benchmark", "milestone"}
FACT_KEY_MAP = {
    "Decision": "decisions",
    "Constraint": "constraints",
    "Rejected": "rejected_alternatives",
    "Directive": "directives",
    "Tested": "tested",
    "Not-tested": "not_tested",
    "Gate": "gates",
    "Supersedes": "supersedes",
}
FACT_COLLECTIONS = (
    "decisions",
    "constraints",
    "rejected_alternatives",
    "directives",
    "implemented",
    "tested",
    "not_tested",
    "gates",
    "limitations",
    "benchmark_metrics",
    "supersedes",
)


class FactPackError(RuntimeError):
    pass


@dataclass(frozen=True)
class CommitMetadata:
    sha: str
    authored_at: str
    subject: str
    author_name: str
    message: str
    repository: str | None
    commit_url: str | None


def run_git(repo_root: Path, *args: str, stdin: str | None = None) -> str:
    command = ["git", "-C", str(repo_root), *args]
    try:
        result = subprocess.run(
            command,
            input=stdin,
            text=True,
            capture_output=True,
            check=True,
        )
    except subprocess.CalledProcessError as exc:
        detail = exc.stderr.strip() or exc.stdout.strip() or "git command failed"
        raise FactPackError(f"{' '.join(command)}: {detail}") from exc
    return result.stdout


def normalize_github_repository(remote: str) -> str | None:
    remote = remote.strip()
    patterns = (
        r"^https?://github\.com/([^/]+/[^/]+?)(?:\.git)?$",
        r"^git@github\.com:([^/]+/[^/]+?)(?:\.git)?$",
        r"^ssh://git@github\.com/([^/]+/[^/]+?)(?:\.git)?$",
    )
    for pattern in patterns:
        match = re.match(pattern, remote)
        if match:
            return match.group(1)
    return None


def get_commit_metadata(repo_root: Path, commit: str) -> CommitMetadata:
    sha = run_git(repo_root, "rev-parse", f"{commit}^{{commit}}").strip()
    raw = run_git(repo_root, "show", "-s", "--format=%H%x00%aI%x00%s%x00%an%x00%B", sha)
    parts = raw.split("\x00", 4)
    if len(parts) != 5:
        raise FactPackError(f"Unable to read metadata for commit {sha}")
    commit_sha, authored_at, subject, author_name, message = (part.rstrip("\n") for part in parts)

    repository = None
    commit_url = None
    try:
        remote = run_git(repo_root, "config", "--get", "remote.origin.url").strip()
    except FactPackError:
        remote = ""
    if remote:
        repository = normalize_github_repository(remote)
        if repository:
            commit_url = f"https://github.com/{repository}/commit/{commit_sha}"

    return CommitMetadata(
        sha=commit_sha,
        authored_at=authored_at,
        subject=subject,
        author_name=author_name,
        message=message,
        repository=repository,
        commit_url=commit_url,
    )


def parse_trailers(repo_root: Path, message: str) -> list[tuple[str, str]]:
    parsed = run_git(repo_root, "interpret-trailers", "--parse", stdin=message)
    trailers: list[tuple[str, str]] = []
    for raw_line in parsed.splitlines():
        if ":" not in raw_line:
            continue
        key, value = raw_line.split(":", 1)
        key = key.strip()
        value = value.strip()
        if key in KNOWN_TRAILERS and value:
            trailers.append((key, value))
    return trailers


def trailer_values(trailers: Iterable[tuple[str, str]], key: str) -> list[str]:
    return [value for trailer_key, value in trailers if trailer_key == key]


def discover_significance(trailers: list[tuple[str, str]], source_event: str) -> tuple[str, str]:
    content_values = trailer_values(trailers, "Content")
    if content_values:
        value = content_values[-1].strip().lower()
        if value not in SIGNIFICANCE:
            raise FactPackError(
                f"Unsupported Content trailer '{content_values[-1]}'; expected one of {sorted(SIGNIFICANCE)}"
            )
        return value, "content-trailer"

    if source_event == "release":
        return "release", "source-event"

    keys = {key for key, _ in trailers}
    if {"Decision", "Rejected", "Gate", "Reference"} & keys:
        return "candidate", "structured-knowledge"
    if {"Issue", "Area", "Tested"}.issubset(keys):
        return "candidate", "structured-knowledge"
    if source_event in {"benchmark", "milestone"} and (keys & {"Tested", "Decision", "Issue"}):
        return "candidate", "source-event"
    return "none", "no-signal"


def slug_event_id(event_id: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._-]+", "-", event_id.strip()).strip("-.")
    if not cleaned:
        raise FactPackError("event_id must contain at least one stable filename-safe character")
    if len(cleaned) > 120:
        import hashlib

        digest = hashlib.sha256(event_id.encode("utf-8")).hexdigest()[:16]
        cleaned = f"{cleaned[:96]}-{digest}"
    return cleaned


def candidate_identity(source_event: str, event_id: str) -> tuple[str, str]:
    slug = slug_event_id(event_id)
    return f"trace2d-{source_event}-{slug}", f"{source_event}-{slug}.json"


def evidence_ref(meta: CommitMetadata, trailer: str, ordinal: int) -> dict[str, Any]:
    ref: dict[str, Any] = {
        "source_type": "commit_trailer",
        "commit": meta.sha,
        "trailer": trailer,
        "ordinal": ordinal,
    }
    if meta.commit_url:
        ref["url"] = meta.commit_url
    return ref


def build_fact_items(meta: CommitMetadata, trailers: list[tuple[str, str]]) -> dict[str, list[dict[str, Any]]]:
    facts = {name: [] for name in FACT_COLLECTIONS}
    ordinals: dict[str, int] = {}
    for key, value in trailers:
        target = FACT_KEY_MAP.get(key)
        if target is None:
            continue
        ordinals[key] = ordinals.get(key, 0) + 1
        item: dict[str, Any] = {
            "id": f"{target.replace('_', '-')}-{ordinals[key]:03d}",
            "text": value,
            "evidence": [evidence_ref(meta, key, ordinals[key])],
        }
        if key == "Rejected" and " | " in value:
            alternative, reason = value.split(" | ", 1)
            item["alternative"] = alternative.strip()
            item["reason"] = reason.strip()
        elif key == "Gate" and " | " in value:
            gate, status = value.split(" | ", 1)
            item["gate"] = gate.strip()
            item["status"] = status.strip()
        elif key == "Supersedes" and " | " in value:
            superseded, replacement = value.split(" | ", 1)
            item["superseded"] = superseded.strip()
            item["replacement"] = replacement.strip()
        facts[target].append(item)
    return facts


def build_sources(meta: CommitMetadata, trailers: list[tuple[str, str]]) -> dict[str, Any]:
    issues: list[dict[str, Any]] = []
    pull_requests: list[dict[str, Any]] = []
    seen_issues: set[str] = set()
    seen_prs: set[int] = set()

    for value in trailer_values(trailers, "Issue"):
        match = re.fullmatch(r"#(\d+)", value.strip())
        entry: dict[str, Any] = {"id": value.strip()}
        if match and meta.repository:
            number = int(match.group(1))
            entry["url"] = f"https://github.com/{meta.repository}/issues/{number}"
        key = json.dumps(entry, sort_keys=True)
        if key not in seen_issues:
            issues.append(entry)
            seen_issues.add(key)

    for value in trailer_values(trailers, "Related"):
        match = re.fullmatch(r"PR\s+#(\d+)", value.strip(), re.IGNORECASE)
        if not match:
            continue
        number = int(match.group(1))
        if number in seen_prs:
            continue
        entry = {"number": number}
        if meta.repository:
            entry["url"] = f"https://github.com/{meta.repository}/pull/{number}"
        pull_requests.append(entry)
        seen_prs.add(number)

    commit_entry: dict[str, Any] = {
        "sha": meta.sha,
        "authored_at": meta.authored_at,
        "subject": meta.subject,
        "author_name": meta.author_name,
    }
    if meta.repository:
        commit_entry["repository"] = meta.repository
    if meta.commit_url:
        commit_entry["url"] = meta.commit_url

    return {
        "issues": issues,
        "pull_requests": pull_requests,
        "commits": [commit_entry],
        "tags": [],
    }


def build_references(meta: CommitMetadata, trailers: list[tuple[str, str]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for ordinal, value in enumerate(trailer_values(trailers, "Reference"), start=1):
        result.append(
            {
                "id": f"reference-{ordinal:03d}",
                "text": value,
                "evidence": [evidence_ref(meta, "Reference", ordinal)],
            }
        )
    return result


def preserved_fields(existing: dict[str, Any] | None) -> dict[str, Any]:
    if not existing:
        return {
            "lifecycle": "discovered",
            "significance_override": None,
            "topics": [],
            "artifacts": {
                "captures": [],
                "videos": [],
                "diagrams": [],
                "benchmark_reports": [],
                "repro_cases": [],
            },
            "related_candidates": [],
            "platform_records": [],
            "notes": None,
        }

    lifecycle = existing.get("lifecycle", "discovered")
    if lifecycle not in LIFECYCLE_STATES:
        raise FactPackError(f"Existing candidate has invalid lifecycle '{lifecycle}'")
    override = existing.get("significance_override")
    if override is not None and override not in SIGNIFICANCE:
        raise FactPackError(f"Existing candidate has invalid significance_override '{override}'")

    return {
        "lifecycle": lifecycle,
        "significance_override": override,
        "topics": existing.get("topics", []),
        "artifacts": existing.get(
            "artifacts",
            {
                "captures": [],
                "videos": [],
                "diagrams": [],
                "benchmark_reports": [],
                "repro_cases": [],
            },
        ),
        "related_candidates": existing.get("related_candidates", []),
        "platform_records": existing.get("platform_records", []),
        "notes": existing.get("notes"),
    }


def load_json(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise FactPackError(f"Unable to read JSON from {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise FactPackError(f"Expected JSON object in {path}")
    return data


def write_json_if_changed(path: Path, data: dict[str, Any]) -> str:
    text = json.dumps(data, indent=2, ensure_ascii=False) + "\n"
    existed = path.exists()
    if existed and path.read_text(encoding="utf-8") == text:
        return "unchanged"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    return "updated" if existed else "created"


def extract_candidate(
    repo_root: Path,
    commit: str,
    source_event: str,
    event_id: str,
    output_dir: Path,
) -> tuple[str, Path | None, dict[str, Any] | None]:
    if source_event not in SOURCE_EVENTS:
        raise FactPackError(f"Unsupported source_event '{source_event}'")
    meta = get_commit_metadata(repo_root, commit)
    trailers = parse_trailers(repo_root, meta.message)
    significance, significance_source = discover_significance(trailers, source_event)
    if significance == "none":
        return "none", None, None

    candidate_id, filename = candidate_identity(source_event, event_id)
    path = output_dir / filename
    existing = load_json(path) if path.exists() else None
    if existing and existing.get("candidate_id") != candidate_id:
        raise FactPackError(f"Candidate identity mismatch in existing file {path}")
    preserved = preserved_fields(existing)

    areas = list(dict.fromkeys(trailer_values(trailers, "Area")))
    related = trailer_values(trailers, "Related")
    candidate: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "candidate_id": candidate_id,
        "source_event": source_event,
        "event_id": event_id,
        "significance": significance,
        "significance_source": significance_source,
        "significance_override": preserved["significance_override"],
        "lifecycle": preserved["lifecycle"],
        "sources": build_sources(meta, trailers),
        "areas": areas,
        "topics": preserved["topics"],
        "facts": build_fact_items(meta, trailers),
        "artifacts": preserved["artifacts"],
        "references": build_references(meta, trailers),
        "related": related,
        "related_candidates": preserved["related_candidates"],
        "platform_records": preserved["platform_records"],
        "notes": preserved["notes"],
    }
    status = write_json_if_changed(path, candidate)
    return status, path, candidate


def iter_fact_ids(candidate: dict[str, Any]) -> set[str]:
    ids: set[str] = set()
    facts = candidate.get("facts", {})
    if not isinstance(facts, dict):
        return ids
    for items in facts.values():
        if not isinstance(items, list):
            continue
        for item in items:
            if isinstance(item, dict) and isinstance(item.get("id"), str):
                ids.add(item["id"])
    return ids


def load_platform_registry(path: Path) -> dict[str, dict[str, Any]]:
    data = load_json(path)
    if data.get("schema_version") != SCHEMA_VERSION:
        raise FactPackError(f"Unsupported platform registry schema in {path}")
    platforms = data.get("platforms")
    if not isinstance(platforms, list):
        raise FactPackError(f"Platform registry {path} must contain a platforms array")
    result: dict[str, dict[str, Any]] = {}
    for entry in platforms:
        if not isinstance(entry, dict) or not isinstance(entry.get("id"), str):
            raise FactPackError(f"Platform registry {path} contains an invalid entry")
        platform_id = entry["id"]
        if platform_id in result:
            raise FactPackError(f"Duplicate platform id '{platform_id}' in {path}")
        if entry.get("publication_mode") != "manual":
            raise FactPackError(
                f"Platform '{platform_id}' must use publication_mode='manual' in C0"
            )
        result[platform_id] = entry
    return result


def resolve_candidate_path(candidate: str, output_dir: Path) -> Path:
    direct = Path(candidate)
    if direct.exists() or direct.suffix == ".json" or "/" in candidate or "\\" in candidate:
        return direct
    matches = list(output_dir.glob(f"*{candidate}*.json"))
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise FactPackError(f"No candidate matching '{candidate}' under {output_dir}")
    raise FactPackError(f"Candidate selector '{candidate}' is ambiguous under {output_dir}")


def set_platform_record(
    candidate_path: Path,
    registry_path: Path,
    platform_id: str,
    state: str,
    language: str | None,
    audience: str | None,
    angle_tags: list[str],
    source_fact_ids: list[str],
    published_url: str | None,
    published_at: str | None,
    notes: str | None,
) -> str:
    if state not in PLATFORM_STATES:
        raise FactPackError(f"Unsupported platform state '{state}'")
    registry = load_platform_registry(registry_path)
    if platform_id not in registry:
        raise FactPackError(f"Unknown platform id '{platform_id}' in {registry_path}")
    candidate = load_json(candidate_path)
    valid_fact_ids = iter_fact_ids(candidate)
    unknown_fact_ids = [fact_id for fact_id in source_fact_ids if fact_id not in valid_fact_ids]
    if unknown_fact_ids:
        raise FactPackError(f"Unknown source fact ids: {', '.join(unknown_fact_ids)}")

    platform = registry[platform_id]
    record = {
        "platform_id": platform_id,
        "state": state,
        "language": language or platform.get("default_language"),
        "audience": audience or platform.get("audience"),
        "angle_tags": list(dict.fromkeys(angle_tags)),
        "source_fact_ids": list(dict.fromkeys(source_fact_ids)),
        "published_url": published_url,
        "published_at": published_at,
        "notes": notes,
    }
    records = candidate.get("platform_records", [])
    if not isinstance(records, list):
        raise FactPackError(f"Candidate {candidate_path} has invalid platform_records")
    replaced = False
    new_records: list[dict[str, Any]] = []
    for existing in records:
        if isinstance(existing, dict) and existing.get("platform_id") == platform_id:
            new_records.append(record)
            replaced = True
        else:
            new_records.append(existing)
    if not replaced:
        new_records.append(record)
    new_records.sort(key=lambda item: str(item.get("platform_id", "")))
    candidate["platform_records"] = new_records
    return write_json_if_changed(candidate_path, candidate)


def review_candidate(
    candidate_path: Path,
    state: str | None,
    significance_override: str | None,
    clear_significance_override: bool,
) -> str:
    candidate = load_json(candidate_path)
    if state is not None:
        if state not in LIFECYCLE_STATES:
            raise FactPackError(f"Unsupported lifecycle state '{state}'")
        candidate["lifecycle"] = state
    if clear_significance_override:
        candidate["significance_override"] = None
    elif significance_override is not None:
        if significance_override not in SIGNIFICANCE:
            raise FactPackError(f"Unsupported significance override '{significance_override}'")
        candidate["significance_override"] = significance_override
    return write_json_if_changed(candidate_path, candidate)


def rebuild_candidates(
    repo_root: Path,
    rev_range: str,
    output_dir: Path,
    source_event: str,
) -> dict[str, int]:
    commits = [
        line.strip()
        for line in run_git(repo_root, "rev-list", "--reverse", rev_range).splitlines()
        if line.strip()
    ]
    counts = {"created_or_updated": 0, "unchanged": 0, "none": 0}
    for sha in commits:
        status, _, _ = extract_candidate(repo_root, sha, source_event, sha, output_dir)
        if status == "none":
            counts["none"] += 1
        elif status == "unchanged":
            counts["unchanged"] += 1
        else:
            counts["created_or_updated"] += 1
    return counts


def default_repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_output_dir(repo_root: Path) -> Path:
    return repo_root / "content" / "candidates"


def default_registry_path(repo_root: Path) -> Path:
    return repo_root / "content" / "platforms.json"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Derive Trace2D C0 Fact Packs from durable repository evidence")
    parser.add_argument("--repo-root", type=Path, default=default_repo_root())
    subparsers = parser.add_subparsers(dest="command", required=True)

    extract = subparsers.add_parser("extract", help="derive/reconcile one candidate from a commit")
    extract.add_argument("--commit", default="HEAD")
    extract.add_argument("--source-event", choices=sorted(SOURCE_EVENTS), default="merge")
    extract.add_argument("--event-id")
    extract.add_argument("--output-dir", type=Path)

    rebuild = subparsers.add_parser("rebuild", help="rebuild/reconcile candidates over a Git revision range")
    rebuild.add_argument("--rev-range", required=True)
    rebuild.add_argument("--source-event", choices=sorted(SOURCE_EVENTS), default="merge")
    rebuild.add_argument("--output-dir", type=Path)

    platform = subparsers.add_parser("platform", help="add/update manual platform metadata on a candidate")
    platform.add_argument("--candidate", required=True)
    platform.add_argument("--platform", required=True)
    platform.add_argument("--state", choices=sorted(PLATFORM_STATES), default="considered")
    platform.add_argument("--language")
    platform.add_argument("--audience")
    platform.add_argument("--angle-tag", action="append", default=[])
    platform.add_argument("--source-fact-id", action="append", default=[])
    platform.add_argument("--published-url")
    platform.add_argument("--published-at")
    platform.add_argument("--notes")
    platform.add_argument("--registry", type=Path)
    platform.add_argument("--output-dir", type=Path)

    review = subparsers.add_parser("review", help="update maintainer-owned candidate lifecycle metadata")
    review.add_argument("--candidate", required=True)
    review.add_argument("--state", choices=sorted(LIFECYCLE_STATES))
    review.add_argument("--significance-override", choices=sorted(SIGNIFICANCE))
    review.add_argument("--clear-significance-override", action="store_true")
    review.add_argument("--output-dir", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    repo_root = args.repo_root.resolve()
    try:
        if args.command == "extract":
            output_dir = (args.output_dir or default_output_dir(repo_root)).resolve()
            meta = get_commit_metadata(repo_root, args.commit)
            event_id = args.event_id or meta.sha
            status, path, candidate = extract_candidate(
                repo_root, meta.sha, args.source_event, event_id, output_dir
            )
            payload = {
                "status": status,
                "candidate_path": str(path) if path else None,
                "candidate_id": candidate.get("candidate_id") if candidate else None,
            }
            print(json.dumps(payload, ensure_ascii=False))
            return 0

        if args.command == "rebuild":
            output_dir = (args.output_dir or default_output_dir(repo_root)).resolve()
            counts = rebuild_candidates(repo_root, args.rev_range, output_dir, args.source_event)
            print(json.dumps({"status": "ok", **counts}))
            return 0

        if args.command == "platform":
            output_dir = (args.output_dir or default_output_dir(repo_root)).resolve()
            registry = (args.registry or default_registry_path(repo_root)).resolve()
            candidate_path = resolve_candidate_path(args.candidate, output_dir)
            status = set_platform_record(
                candidate_path,
                registry,
                args.platform,
                args.state,
                args.language,
                args.audience,
                args.angle_tag,
                args.source_fact_id,
                args.published_url,
                args.published_at,
                args.notes,
            )
            print(json.dumps({"status": status, "candidate_path": str(candidate_path)}))
            return 0

        if args.command == "review":
            output_dir = (args.output_dir or default_output_dir(repo_root)).resolve()
            candidate_path = resolve_candidate_path(args.candidate, output_dir)
            status = review_candidate(
                candidate_path,
                args.state,
                args.significance_override,
                args.clear_significance_override,
            )
            print(json.dumps({"status": status, "candidate_path": str(candidate_path)}))
            return 0

        raise FactPackError(f"Unsupported command {args.command}")
    except FactPackError as exc:
        print(json.dumps({"status": "error", "error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
