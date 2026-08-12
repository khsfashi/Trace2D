#!/usr/bin/env python3
"""Derive Trace2D's operational next action from durable lane data + live GitHub state."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

SCHEMA = "trace2d.project-state.v1"


class LiveStateError(RuntimeError):
    pass


def _headers() -> dict[str, str]:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "Trace2D-project-state/1",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return headers


def _get_json(url: str) -> Any:
    request = urllib.request.Request(url, headers=_headers())
    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            return json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, OSError, json.JSONDecodeError) as exc:
        raise LiveStateError(str(exc)) from exc


def _list_paginated(url: str) -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    page = 1
    while True:
        separator = "&" if "?" in url else "?"
        batch = _get_json(f"{url}{separator}per_page=100&page={page}")
        if not isinstance(batch, list):
            raise LiveStateError(f"expected list from {url}")
        items.extend(item for item in batch if isinstance(item, dict))
        if len(batch) < 100:
            return items
        page += 1


def fetch_live_snapshot(repository: str) -> dict[str, Any]:
    encoded_repo = urllib.parse.quote(repository, safe="/")
    base = f"https://api.github.com/repos/{encoded_repo}"
    branch = _get_json(f"{base}/branches/main")
    issues_raw = _list_paginated(f"{base}/issues?state=all")
    pulls = _list_paginated(f"{base}/pulls?state=all&sort=updated&direction=desc")
    issues = [item for item in issues_raw if "pull_request" not in item]
    head_sha = (
        branch.get("commit", {}).get("sha")
        if isinstance(branch, dict)
        else None
    )
    return {"head_sha": head_sha, "issues": issues, "pulls": pulls}


def _token_in_title(token: str, title: str) -> bool:
    return re.search(rf"(?<![A-Za-z0-9]){re.escape(token)}(?![A-Za-z0-9])", title, re.IGNORECASE) is not None


def _issue_index(issues: list[dict[str, Any]]) -> dict[int, dict[str, Any]]:
    return {
        int(issue["number"]): issue
        for issue in issues
        if isinstance(issue.get("number"), int)
    }


def _find_stage_issue(
    stage: dict[str, Any],
    issues: list[dict[str, Any]],
    indexed: dict[int, dict[str, Any]],
) -> dict[str, Any] | None:
    explicit = stage.get("issue")
    if isinstance(explicit, int):
        return indexed.get(explicit)

    token = str(stage["id"])
    matches = [
        issue
        for issue in issues
        if _token_in_title(token, str(issue.get("title", "")))
    ]
    if not matches:
        return None

    matches.sort(
        key=lambda issue: (
            str(issue.get("state", "")).lower() == "open",
            int(issue.get("number", 0)),
        ),
        reverse=True,
    )
    return matches[0]


def _body_references_issue(body: str, issue_number: int) -> bool:
    pattern = rf"(?<!\d)#{issue_number}(?!\d)"
    return re.search(pattern, body) is not None


def _related_pulls(
    stage_id: str,
    issue_number: int | None,
    pulls: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    related: list[dict[str, Any]] = []
    for pull in pulls:
        title = str(pull.get("title", ""))
        body = str(pull.get("body") or "")
        if _token_in_title(stage_id, title):
            related.append(pull)
            continue
        if issue_number is not None and _body_references_issue(body, issue_number):
            related.append(pull)
    related.sort(key=lambda pull: int(pull.get("number", 0)), reverse=True)
    return related


def _pull_summary(pull: dict[str, Any] | None) -> dict[str, Any] | None:
    if pull is None:
        return None
    return {
        "number": pull.get("number"),
        "state": pull.get("state"),
        "draft": bool(pull.get("draft", False)),
        "merged": pull.get("merged_at") is not None,
        "head_ref": (pull.get("head") or {}).get("ref"),
        "head_sha": (pull.get("head") or {}).get("sha"),
    }


def _stage_status(
    stage: dict[str, Any],
    issues: list[dict[str, Any]],
    indexed: dict[int, dict[str, Any]],
    pulls: list[dict[str, Any]],
) -> dict[str, Any]:
    issue = _find_stage_issue(stage, issues, indexed)
    issue_number = int(issue["number"]) if issue and isinstance(issue.get("number"), int) else None
    related = _related_pulls(str(stage["id"]), issue_number, pulls)
    open_pull = next((pull for pull in related if str(pull.get("state", "")).lower() == "open"), None)
    merged_pull = next((pull for pull in related if pull.get("merged_at") is not None), None)

    if issue is None:
        state = "ready"
    elif str(issue.get("state", "")).lower() == "closed":
        state = "complete"
    elif open_pull is not None:
        state = "active_draft" if bool(open_pull.get("draft", False)) else "active"
    else:
        state = "ready"

    return {
        "id": stage["id"],
        "state": state,
        "issue": issue_number,
        "pull_request": _pull_summary(open_pull or merged_pull),
    }


def _detour_status(
    detour: dict[str, Any],
    indexed: dict[int, dict[str, Any]],
    pulls: list[dict[str, Any]],
) -> dict[str, Any]:
    issue_number = int(detour["issue"])
    issue = indexed.get(issue_number)
    related = _related_pulls(str(detour["id"]), issue_number, pulls)
    open_pull = next((pull for pull in related if str(pull.get("state", "")).lower() == "open"), None)
    merged_pull = next((pull for pull in related if pull.get("merged_at") is not None), None)

    if issue is None:
        state = "unknown"
    elif str(issue.get("state", "")).lower() == "closed":
        state = "complete"
    elif open_pull is not None:
        state = "active_draft" if bool(open_pull.get("draft", False)) else "active"
    else:
        state = "ready"

    return {
        "id": detour["id"],
        "kind": "owner_detour",
        "reason": detour.get("reason"),
        "state": state,
        "issue": issue_number,
        "pull_request": _pull_summary(open_pull or merged_pull),
    }


def _next_action(item: dict[str, Any], kind: str) -> dict[str, Any]:
    state = item["state"]
    if state in {"active", "active_draft"} and item.get("pull_request"):
        return {
            "kind": "continue_pull_request",
            "target_kind": kind,
            "id": item["id"],
            "issue": item.get("issue"),
            "pull_request": item["pull_request"]["number"],
        }
    if state == "ready":
        return {
            "kind": "implement_issue" if item.get("issue") is not None else "create_issue",
            "target_kind": kind,
            "id": item["id"],
            "issue": item.get("issue"),
        }
    return {
        "kind": "inspect_live_github",
        "target_kind": kind,
        "id": item.get("id"),
        "issue": item.get("issue"),
    }


def derive_state(config: dict[str, Any], snapshot: dict[str, Any]) -> dict[str, Any]:
    issues = list(snapshot.get("issues") or [])
    pulls = list(snapshot.get("pulls") or [])
    indexed = _issue_index(issues)

    detours = [
        _detour_status(detour, indexed, pulls)
        for detour in config.get("owner_detours", [])
    ]
    active_detour = next((detour for detour in detours if detour["state"] != "complete"), None)

    program = config["core_program"]
    stages = [
        _stage_status(stage, issues, indexed, pulls)
        for stage in program["stages"]
    ]
    current_stage = next((stage for stage in stages if stage["state"] != "complete"), None)
    current_index = stages.index(current_stage) if current_stage is not None else len(stages)
    previous = stages[current_index - 1] if current_index > 0 else None

    if active_detour is not None:
        current = active_detour
        action = _next_action(active_detour, "owner_detour")
    elif current_stage is not None:
        current = {**current_stage, "kind": "core_stage"}
        action = _next_action(current_stage, "core_stage")
    else:
        after_core = []
        for task in config.get("after_core", []):
            issue = indexed.get(int(task["issue"]))
            state = "unknown" if issue is None else ("complete" if issue.get("state") == "closed" else "ready")
            after_core.append({"id": task["id"], "kind": "after_core", "issue": task["issue"], "state": state})
        current = next((task for task in after_core if task["state"] != "complete"), None)
        action = (
            _next_action(current, "after_core")
            if current is not None
            else {"kind": "complete", "target_kind": "repository"}
        )

    return {
        "schema": SCHEMA,
        "repository": {
            "name": config["repository"],
            "head_sha": snapshot.get("head_sha"),
        },
        "live": {
            "available": True,
            "source": snapshot.get("source", "github"),
        },
        "owner_detours": detours,
        "core": {
            "program": program["id"],
            "issue": program["issue"],
            "current": current,
            "previous_core_stage": previous,
        },
        "next_action": action,
        "blockers": [],
    }


def unavailable_state(config: dict[str, Any], reason: str) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "repository": {
            "name": config["repository"],
            "head_sha": None,
        },
        "live": {
            "available": False,
            "source": "github",
            "reason": reason,
        },
        "owner_detours": [],
        "core": {
            "program": config["core_program"]["id"],
            "issue": config["core_program"]["issue"],
            "current": None,
            "previous_core_stage": None,
        },
        "next_action": {
            "kind": "inspect_live_github",
            "target_kind": "repository",
            "id": None,
            "issue": None,
        },
        "blockers": ["live_github_unavailable"],
    }


def _load_json(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain one JSON object")
    return data


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        default="config/trace2d.core-lane.json",
        help="durable core-lane JSON contract",
    )
    parser.add_argument("--repository", help="override owner/name repository")
    parser.add_argument("--snapshot", help="offline deterministic GitHub snapshot fixture")
    parser.add_argument("--json", action="store_true", help="emit JSON only")
    args = parser.parse_args(argv)

    try:
        config = _load_json(Path(args.config))
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"project_state: failed to load config: {exc}", file=sys.stderr)
        return 2

    if args.repository:
        config["repository"] = args.repository

    try:
        if args.snapshot:
            snapshot = _load_json(Path(args.snapshot))
            snapshot["source"] = "snapshot"
        else:
            snapshot = fetch_live_snapshot(config["repository"])
        state = derive_state(config, snapshot)
    except (LiveStateError, OSError, ValueError, json.JSONDecodeError) as exc:
        state = unavailable_state(config, str(exc))

    if args.json:
        print(json.dumps(state, sort_keys=True, separators=(",", ":")))
    else:
        print(json.dumps(state, indent=2, sort_keys=True))

    return 0 if state["live"]["available"] else 3


if __name__ == "__main__":
    raise SystemExit(main())
