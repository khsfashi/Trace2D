#!/usr/bin/env python3
"""Qualify the independent Godot B2 verifier against committed good/bad fixtures."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from typing import Any

PASS_MARKER = "b2-godot-verifier-pass"
FAIL_MARKER = "b2-godot-verifier-fail"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        relative = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(8, "big"))
        digest.update(relative)
        data = path.read_bytes()
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def run_case(godot: Path, fixture: Path, verifier: Path, runner: Path) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory(prefix="trace2d-b2-godot-verifier-") as temp_dir:
        project = Path(temp_dir) / "project"
        shutil.copytree(fixture, project)
        shutil.copy2(verifier, project / "__trace2d_b2_verifier.gd")
        shutil.copy2(runner, project / "__trace2d_b2_runner.gd")
        return subprocess.run(
            [
                str(godot),
                "--headless",
                "--path",
                str(project),
                "--fixed-fps",
                "60",
                "--script",
                "res://__trace2d_b2_runner.gd",
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=45,
            check=False,
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument(
        "--qualification-root",
        type=Path,
        default=Path("benchmarks/b2/qualification/godot_verifier"),
    )
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()

    root = args.qualification_root.resolve()
    godot = args.godot.resolve()
    verifier = root / "B2GodotVerifier.gd"
    runner = root / "B2GodotVerifierRunner.gd"
    good_fixture = root / "known_good"
    bad_fixture = root / "known_bad_cooldown"
    require(godot.is_file(), f"Godot binary does not exist: {godot}")
    for path in (verifier, runner, good_fixture / "project.godot", bad_fixture / "project.godot"):
        require(path.exists(), f"qualification input is missing: {path}")

    good_hash_before = tree_sha256(good_fixture)
    bad_hash_before = tree_sha256(bad_fixture)
    good = run_case(godot, good_fixture, verifier, runner)
    bad = run_case(godot, bad_fixture, verifier, runner)
    require(
        good.returncode == 0 and PASS_MARKER in good.stdout,
        f"known-good fixture was not accepted by the Godot verifier:\n{good.stdout}",
    )
    require(
        bad.returncode != 0 and FAIL_MARKER in bad.stdout,
        f"known-bad cooldown fixture was not rejected by the same Godot verifier:\n{bad.stdout}",
    )
    require(tree_sha256(good_fixture) == good_hash_before, "known-good source fixture mutated during qualification")
    require(tree_sha256(bad_fixture) == bad_hash_before, "known-bad source fixture mutated during qualification")

    version = subprocess.run(
        [str(godot), "--version"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=15,
        check=False,
    )
    require(version.returncode == 0, f"Godot version probe failed: {version.stdout}")

    args.evidence_dir.mkdir(parents=True, exist_ok=True)
    (args.evidence_dir / "known-good.log").write_text(good.stdout, encoding="utf-8")
    (args.evidence_dir / "known-bad-cooldown.log").write_text(bad.stdout, encoding="utf-8")
    evidence: dict[str, Any] = {
        "schema_version": 1,
        "benchmark_id": "trace2d-b2",
        "task_id": "b2-topdown-combat-v1",
        "lane_family": "godot",
        "scored": False,
        "qualified": True,
        "verifier": "benchmarks/b2/qualification/godot_verifier/B2GodotVerifier.gd",
        "verifier_sha256": sha256_file(verifier),
        "runner_sha256": sha256_file(runner),
        "engine": {"id": "godot", "reported_version": version.stdout.strip()},
        "known_good": {
            "accepted": True,
            "exit_code": good.returncode,
            "fixture_tree_sha256": good_hash_before,
            "marker": PASS_MARKER,
        },
        "known_bad_cooldown": {
            "accepted": False,
            "exit_code": bad.returncode,
            "fixture_tree_sha256": bad_hash_before,
            "mutation": "ATTACK_COOLDOWN_STEPS 6 -> 5",
            "marker": FAIL_MARKER,
        },
        "scoring_gate_opened": False,
    }
    (args.evidence_dir / "godot-summary.json").write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(evidence, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"B2 Godot verifier qualification failed: {exc}")
        raise SystemExit(1)
