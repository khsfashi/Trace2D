#!/usr/bin/env python3
"""Produce a non-mutating diagnostic report for retained Benchmark B2 records."""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import benchmark_b0
import benchmark_b2_postscore_remediation as remediation


def build_report(raw_path: Path) -> dict[str, object]:
    records = benchmark_b0.verify_jsonl_chain(raw_path)
    attempts: list[dict[str, object]] = []
    counts: Counter[str] = Counter()
    integrity_failures = 0
    for record in records:
        diagnostic = remediation.diagnostic_classification(record)
        integrity = remediation.integrity_diagnostics(record)
        counts[str(diagnostic["diagnostic_status"])] += 1
        if not integrity["valid"]:
            integrity_failures += 1
        attempts.append(
            {
                "slot": record.get("slot"),
                "lane_id": record.get("lane_id"),
                "trial_id": record.get("trial_id"),
                "record_sha256": record.get("record_sha256"),
                "classification": diagnostic,
                "terminal_evidence": remediation.terminal_evidence(record),
                "budget": remediation.budget_diagnostics(record),
                "integrity": integrity,
            }
        )
    return {
        "schema_version": 1,
        "kind": "trace2d_b2_postscore_diagnostics",
        "raw_path": str(raw_path),
        "raw_records_mutated": False,
        "attempt_count": len(attempts),
        "diagnostic_status_counts": dict(sorted(counts.items())),
        "integrity_failure_count": integrity_failures,
        "attempts": attempts,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = build_report(args.raw.expanduser().resolve())
    encoded = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.expanduser().resolve().write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
