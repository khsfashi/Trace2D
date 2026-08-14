#!/usr/bin/env python3
"""Independently verify one preserved Benchmark B1 candidate workspace.

This is scored-harness infrastructure, not candidate authority. It dispatches the
already-frozen B1 verifier identities onto arbitrary preserved trial workspaces:

- Godot uses the exact frozen headless verifier script qualified before scoring.
- Trace2D Sprite/particle use the existing native production-parser verifier.
- Trace2D animation compiles the candidate through the exact same verifier driver
  used for known-good/known-bad qualification, with production Assets/Runtime.

The script never supplies task answers to the candidate Agent and never mutates a
frozen suite, task, verifier registry, or fixture definition.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import benchmark_b1

GODOT_VERIFIER_RELATIVE = Path("benchmarks/b1/qualification/verify_frozen_fixture.gd")
TRACE2D_FILE_BY_TASK = {
    "b1-sprite-normalize-repair": ("sprite", Path("hero.sprite.toml")),
    "b1-particle-budget-repair": ("particle", Path("hit_spark.trace2d.particle.toml")),
}
TRACE2D_ANIMATION_TASK = "b1-animation-exact-event"
TRACE2D_ANIMATION_TARGET = "trace2d_b1_animation_candidate_verify"


class CandidateVerifierError(RuntimeError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise CandidateVerifierError(message)


def _load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CandidateVerifierError(f"failed to read JSON {path}: {exc}") from exc
    _require(isinstance(value, dict), f"expected JSON object: {path}")
    return value


def _require_file(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    _require(resolved.is_file(), f"{label} not found: {resolved}")
    return resolved


def _require_dir(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    _require(resolved.is_dir(), f"{label} not found: {resolved}")
    return resolved


def _run(argv: list[str], *, cwd: Path, timeout: float) -> dict[str, Any]:
    started = time.perf_counter_ns()
    try:
        completed = subprocess.run(
            argv,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        return_code: int | None = completed.returncode
        stdout = completed.stdout
        stderr = completed.stderr
        timed_out = False
    except subprocess.TimeoutExpired as exc:
        return_code = None
        stdout = exc.stdout if isinstance(exc.stdout, str) else ""
        stderr = exc.stderr if isinstance(exc.stderr, str) else ""
        timed_out = True
    return {
        "argv": argv,
        "return_code": return_code,
        "timed_out": timed_out,
        "duration_ms": (time.perf_counter_ns() - started) / 1_000_000.0,
        "stdout": stdout,
        "stderr": stderr,
    }


def _classify_process(process: dict[str, Any], verifier_id: str) -> dict[str, Any]:
    if process["timed_out"]:
        return {
            "status": "error",
            "code": "verifier_timeout",
            "verifier": verifier_id,
            "process": process,
        }
    if process["return_code"] == 0:
        return {"status": "pass", "verifier": verifier_id, "process": process}
    if process["return_code"] == 1:
        return {
            "status": "fail",
            "code": "candidate_rejected",
            "verifier": verifier_id,
            "process": process,
        }
    return {
        "status": "error",
        "code": "verifier_process_error",
        "verifier": verifier_id,
        "process": process,
    }


def _find_task(suite: dict[str, Any], task_id: str) -> dict[str, Any]:
    for task in suite.get("tasks", []):
        if isinstance(task, dict) and task.get("id") == task_id:
            return task
    raise CandidateVerifierError(f"unknown B1 task: {task_id}")


def _verifier_id_for(suite: dict[str, Any], task_id: str, lane_id: str) -> str:
    task = _find_task(suite, task_id)
    lanes = task.get("lanes")
    _require(isinstance(lanes, dict), f"task {task_id} lanes are missing")
    lane = lanes.get(lane_id)
    _require(isinstance(lane, dict), f"task {task_id} does not define lane {lane_id}")
    verifier_id = lane.get("verifier")
    _require(isinstance(verifier_id, str) and verifier_id, "frozen verifier id is missing")
    return verifier_id


def _validate_registry(repo_root: Path, verifier_id: str, task_id: str, lane_id: str) -> None:
    registry = _load_json(repo_root / "benchmarks/b1/verifiers.json")
    entries = registry.get("verifiers")
    _require(isinstance(entries, list), "B1 verifier registry is malformed")
    entry = next(
        (
            value
            for value in entries
            if isinstance(value, dict) and value.get("id") == verifier_id
        ),
        None,
    )
    _require(isinstance(entry, dict), f"frozen verifier not found: {verifier_id}")
    _require(entry.get("task_id") == task_id, f"verifier {verifier_id} task binding changed")
    expected_engine = "godot" if lane_id.startswith("godot.") else "trace2d"
    _require(entry.get("engine") == expected_engine, f"verifier {verifier_id} engine binding changed")
    _require(entry.get("qualification_required") is True, f"verifier {verifier_id} lost qualification gate")


def _resolve_godot_bin(explicit: str | None) -> Path:
    raw = explicit or os.environ.get("TRACE2D_B1_GODOT_BIN") or os.environ.get("TRACE2D_BENCH_GODOT_BIN")
    _require(bool(raw), "Godot verifier requires --godot-bin or TRACE2D_B1_GODOT_BIN")
    return _require_file(Path(str(raw)), "pinned Godot executable")


def _resolve_trace2d_fixture_verifier(explicit: str | None) -> Path:
    raw = explicit or os.environ.get("TRACE2D_B1_FIXTURE_VERIFY_BIN")
    _require(
        bool(raw),
        "Trace2D Sprite/particle verifier requires --trace2d-fixture-verifier or TRACE2D_B1_FIXTURE_VERIFY_BIN",
    )
    return _require_file(Path(str(raw)), "Trace2D B1 fixture verifier")


def verify_godot(
    *,
    repo_root: Path,
    workspace: Path,
    task_id: str,
    verifier_id: str,
    godot_bin: Path,
    timeout: float,
) -> dict[str, Any]:
    verifier_source = _require_file(repo_root / GODOT_VERIFIER_RELATIVE, "frozen Godot B1 verifier")
    injected = workspace / "__trace2d_b1_scored_verify.gd"
    _require(not injected.exists(), f"candidate workspace already contains reserved verifier path: {injected}")
    try:
        shutil.copy2(verifier_source, injected)
        process = _run(
            [
                str(godot_bin),
                "--headless",
                "--path",
                str(workspace),
                "--script",
                "res://__trace2d_b1_scored_verify.gd",
                "--",
                "--task",
                task_id,
            ],
            cwd=workspace,
            timeout=timeout,
        )
    finally:
        try:
            injected.unlink()
        except FileNotFoundError:
            pass
    return _classify_process(process, verifier_id)


def verify_trace2d_file(
    *,
    workspace: Path,
    task_id: str,
    verifier_id: str,
    verifier_bin: Path,
    timeout: float,
) -> dict[str, Any]:
    kind, relative = TRACE2D_FILE_BY_TASK[task_id]
    candidate = _require_file(workspace / relative, f"{task_id} candidate file")
    process = _run(
        [str(verifier_bin), kind, str(candidate)],
        cwd=workspace,
        timeout=timeout,
    )
    return _classify_process(process, verifier_id)


def _candidate_executable(build_dir: Path) -> Path:
    executable_name = TRACE2D_ANIMATION_TARGET + (".exe" if os.name == "nt" else "")
    matches = sorted(
        path
        for path in build_dir.rglob(executable_name)
        if path.is_file()
    )
    _require(matches, f"animation candidate verifier executable was not produced under {build_dir}")
    debug_matches = [path for path in matches if any(part.casefold() == "debug" for part in path.parts)]
    selected = debug_matches[0] if debug_matches else matches[0]
    return selected.resolve()


def verify_trace2d_animation(
    *,
    repo_root: Path,
    workspace: Path,
    verifier_id: str,
    cmake: str,
    configure_preset: str,
    build_dir: Path,
    timeout: float,
) -> dict[str, Any]:
    _require_file(workspace / "animation_case.cpp", "animation candidate source")
    _require_file(workspace / "animation_case.hpp", "animation candidate header")
    build_dir.parent.mkdir(parents=True, exist_ok=True)

    configure = _run(
        [
            cmake,
            "--preset",
            configure_preset,
            "-B",
            str(build_dir),
            f"-DTRACE2D_B1_ANIMATION_CANDIDATE_DIR={workspace}",
        ],
        cwd=repo_root,
        timeout=timeout,
    )
    if configure["timed_out"] or configure["return_code"] != 0:
        return {
            "status": "error",
            "code": "candidate_verifier_configure_failed",
            "verifier": verifier_id,
            "configure": configure,
        }

    build = _run(
        [
            cmake,
            "--build",
            str(build_dir),
            "--config",
            "Debug",
            "--target",
            TRACE2D_ANIMATION_TARGET,
            "--parallel",
        ],
        cwd=repo_root,
        timeout=timeout,
    )
    if build["timed_out"] or build["return_code"] != 0:
        return {
            "status": "fail",
            "code": "candidate_compile_or_link_failed",
            "verifier": verifier_id,
            "configure": configure,
            "build": build,
        }

    executable = _candidate_executable(build_dir)
    process = _run([str(executable)], cwd=workspace, timeout=timeout)
    result = _classify_process(process, verifier_id)
    result["configure"] = configure
    result["build"] = build
    result["candidate_verifier_executable"] = str(executable)
    return result


def verify_candidate(args: argparse.Namespace) -> dict[str, Any]:
    repo_root = Path(args.repo_root).resolve() if args.repo_root else Path(__file__).resolve().parents[1]
    workspace = _require_dir(Path(args.workspace), "candidate workspace")
    suite = benchmark_b1.load_and_validate_suite(repo_root / "benchmarks/b1/suite.json", repo_root)
    _require(args.lane in benchmark_b1.EXPECTED_LANES, f"unknown B1 lane: {args.lane}")
    _require(args.task in benchmark_b1.EXPECTED_TASKS, f"unknown B1 task: {args.task}")

    verifier_id = _verifier_id_for(suite, args.task, args.lane)
    _validate_registry(repo_root, verifier_id, args.task, args.lane)
    timeout = float(args.timeout)

    if args.lane.startswith("godot."):
        verdict = verify_godot(
            repo_root=repo_root,
            workspace=workspace,
            task_id=args.task,
            verifier_id=verifier_id,
            godot_bin=_resolve_godot_bin(args.godot_bin),
            timeout=timeout,
        )
    elif args.task in TRACE2D_FILE_BY_TASK:
        verdict = verify_trace2d_file(
            workspace=workspace,
            task_id=args.task,
            verifier_id=verifier_id,
            verifier_bin=_resolve_trace2d_fixture_verifier(args.trace2d_fixture_verifier),
            timeout=timeout,
        )
    elif args.task == TRACE2D_ANIMATION_TASK:
        build_dir = Path(args.animation_build_dir).expanduser().resolve()
        verdict = verify_trace2d_animation(
            repo_root=repo_root,
            workspace=workspace,
            verifier_id=verifier_id,
            cmake=args.cmake,
            configure_preset=args.configure_preset,
            build_dir=build_dir,
            timeout=timeout,
        )
    else:
        raise CandidateVerifierError(f"no Trace2D verifier dispatch for task: {args.task}")

    return {
        "schema_version": 1,
        "kind": "trace2d_b1_candidate_verification",
        "suite_id": "trace2d-b1",
        "task_id": args.task,
        "lane_id": args.lane,
        "verifier_id": verifier_id,
        "workspace": str(workspace),
        "verdict": verdict,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify one frozen B1 candidate workspace")
    parser.add_argument("--task", required=True, choices=benchmark_b1.EXPECTED_TASKS)
    parser.add_argument("--lane", required=True, choices=benchmark_b1.EXPECTED_LANES)
    parser.add_argument("--workspace", required=True)
    parser.add_argument("--repo-root")
    parser.add_argument("--godot-bin")
    parser.add_argument("--trace2d-fixture-verifier")
    parser.add_argument("--animation-build-dir", default=".trace2d-benchmark-b1/animation-verifier-build")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--configure-preset", default="windows-msvc")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--output")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        result = verify_candidate(args)
    except CandidateVerifierError as exc:
        result = {
            "schema_version": 1,
            "kind": "trace2d_b1_candidate_verification",
            "task_id": args.task,
            "lane_id": args.lane,
            "verdict": {"status": "error", "code": "verifier_setup_error", "message": str(exc)},
        }
    encoded = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        output = Path(args.output).expanduser().resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    status = result.get("verdict", {}).get("status")
    return 0 if status == "pass" else 1 if status == "fail" else 2


if __name__ == "__main__":
    raise SystemExit(main())
