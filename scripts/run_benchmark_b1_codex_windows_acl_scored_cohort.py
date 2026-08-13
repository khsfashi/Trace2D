#!/usr/bin/env python3
"""Run the preregistered 27-slot Benchmark B1 scored cohort on owner Windows.

Everything capable of invalidating a comparison is proved before slot 1:

- frozen B1 policy/suite/fixture qualification/dependency lock,
- Codex CLI 0.144.6 login and real Windows ACL isolation,
- official Godot 4.7.1 identity,
- exact hi-godot/godot-ai source + qualification dependency graph,
- real Codex -> streamable-HTTP Godot MCP call on a non-scored fixture,
- exact Trace2D frozen production commit built in a detached local clone,
- B1 native candidate verifier built from the benchmark branch,
- owner-local known-good/known-bad verifier dispatch for all six authorities.

After scored execution begins there are no retries, replacement slots or early
stopping. Every scheduled attempt is preserved. Failure still packages evidence;
it is never a reason to reroll a slot.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import uuid
import zipfile
from pathlib import Path
from typing import Any

import benchmark_b0
import benchmark_b0_codex_wrapper as codex_core
import benchmark_b0_codex_windows_acl_wrapper as windows
import benchmark_b0_stable_harness
import benchmark_b1
import benchmark_b1_codex_windows_acl_wrapper as b1_wrapper
import benchmark_b1_scored_policy

CODEX_VERSION = "0.144.6"
GODOT_VERSION = "4.7.1-stable"
FROZEN_TRACE2D_COMMIT = "31712ca419efb232d292680661caea51d8a318e4"
GODOT_AI_COMMIT = "f3d99dfbd38c9e095edf1467f85bee507ace2c3a"
GODOT_AI_VERSION = "3.0.6"
GODOT_RELEASE_BASE = f"https://github.com/godotengine/godot-builds/releases/download/{GODOT_VERSION}"
GODOT_ARCHIVE_NAME = f"Godot_v{GODOT_VERSION}_win64.exe.zip"
GODOT_EXE_NAME = f"Godot_v{GODOT_VERSION}_win64.exe"
GODOT_AI_REPOSITORY = "https://github.com/hi-godot/godot-ai.git"
WRAPPER_MODULE = "benchmark_b1_codex_windows_acl_wrapper"
HARNESS = Path(__file__).resolve().with_name("benchmark_b1_scored_harness.py")
PACKAGER = Path(__file__).resolve().with_name("package_benchmark_b1_evidence.py")
VERIFIER = Path(__file__).resolve().with_name("verify_benchmark_b1_candidate.py")


class ScoredCohortError(RuntimeError):
    pass


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ScoredCohortError(f"failed to read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ScoredCohortError(f"expected JSON object: {path}")
    return value


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def require_file(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise ScoredCohortError(f"{label} not found: {resolved}")
    return resolved


def require_dir(path: Path, label: str) -> Path:
    resolved = path.expanduser().resolve()
    if not resolved.is_dir():
        raise ScoredCohortError(f"{label} not found: {resolved}")
    return resolved


def run(
    argv: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    check: bool = True,
    timeout: float | None = None,
) -> subprocess.CompletedProcess[str]:
    process_env = os.environ.copy()
    if env:
        process_env.update(env)
    completed = subprocess.run(
        argv,
        cwd=cwd,
        env=process_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=timeout,
    )
    if check and completed.returncode != 0:
        raise ScoredCohortError(
            f"command failed ({completed.returncode}): {subprocess.list2cmdline(argv)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed


def preserve_process(root: Path, stem: str, completed: subprocess.CompletedProcess[str]) -> None:
    root.mkdir(parents=True, exist_ok=True)
    (root / f"{stem}.stdout.txt").write_text(completed.stdout or "", encoding="utf-8")
    (root / f"{stem}.stderr.txt").write_text(completed.stderr or "", encoding="utf-8")
    write_json(
        root / f"{stem}.process.json",
        {"return_code": completed.returncode},
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha512_file(path: Path) -> str:
    digest = hashlib.sha512()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.unlink(missing_ok=True)
    try:
        with urllib.request.urlopen(url, timeout=60) as response, temporary.open("wb") as stream:
            shutil.copyfileobj(response, stream)
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)


def find_executable(root: Path, stem: str) -> Path:
    names = {stem.casefold(), f"{stem}.exe".casefold()}
    matches = sorted(path for path in root.rglob("*") if path.is_file() and path.name.casefold() in names)
    if not matches:
        raise ScoredCohortError(f"built executable {stem} not found under {root}")
    release = [path for path in matches if any(part.casefold() == "release" for part in path.parts)]
    debug = [path for path in matches if any(part.casefold() == "debug" for part in path.parts)]
    return (release or debug or matches)[0].resolve()


def git_head(git: str, root: Path) -> str:
    return run([git, "rev-parse", "HEAD"], cwd=root).stdout.strip()


def validate_production_source_frozen(git: str, repo_root: Path) -> None:
    changed = run(
        [
            git,
            "diff",
            "--name-only",
            FROZEN_TRACE2D_COMMIT,
            "--",
            "engine",
            "tools",
            "CMakeLists.txt",
            "cmake",
            "vcpkg.json",
            "CMakePresets.json",
        ],
        cwd=repo_root,
    ).stdout.splitlines()
    allowed_benchmark_tools = {
        "tools/trace2d/CMakeLists.txt",  # not expected; kept explicit if Git reports path normalization
    }
    unexpected = [path for path in changed if path.replace("\\", "/") not in allowed_benchmark_tools]
    if unexpected:
        raise ScoredCohortError(
            "current B1 branch changed production source relative to frozen Trace2D commit: "
            + ", ".join(unexpected)
        )


def ensure_official_godot(local_base: Path) -> dict[str, Any]:
    root = local_base / "tools" / f"godot-{GODOT_VERSION}"
    archive = root / GODOT_ARCHIVE_NAME
    checksums = root / "SHA512-SUMS.txt"
    extracted = root / "extracted"
    executable = extracted / GODOT_EXE_NAME
    if not archive.is_file():
        download(f"{GODOT_RELEASE_BASE}/{GODOT_ARCHIVE_NAME}", archive)
    if not checksums.is_file():
        download(f"{GODOT_RELEASE_BASE}/SHA512-SUMS.txt", checksums)

    expected = None
    for line in checksums.read_text(encoding="utf-8").splitlines():
        if line.rstrip().endswith("  " + GODOT_ARCHIVE_NAME):
            expected = line.split()[0].lower()
            break
    if not expected:
        raise ScoredCohortError(f"official checksum entry missing for {GODOT_ARCHIVE_NAME}")
    observed = sha512_file(archive)
    if observed != expected:
        raise ScoredCohortError(f"Godot archive SHA-512 mismatch: expected {expected}, got {observed}")
    if not executable.is_file():
        extracted.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as bundle:
            bundle.extractall(extracted)
    version = run([str(require_file(executable, "Godot executable")), "--version"], cwd=root).stdout.strip()
    if not version.startswith("4.7.1.stable"):
        raise ScoredCohortError(f"Godot version mismatch: {version}")
    return {
        "version": GODOT_VERSION,
        "observed_version": version,
        "archive": str(archive),
        "archive_sha512": observed,
        "executable": str(executable.resolve()),
    }


def python312_command() -> list[str]:
    if sys.version_info[:2] == (3, 12):
        return [sys.executable]
    py = shutil.which("py")
    if py:
        probe = run([py, "-3.12", "-c", "import sys; print(sys.executable)"], cwd=Path.cwd(), check=False)
        if probe.returncode == 0 and probe.stdout.strip():
            return [py, "-3.12"]
    raise ScoredCohortError("Python 3.12 is required to reproduce the selected Godot Agent qualification environment")


def expected_python_freeze(repo_root: Path) -> list[str]:
    lines = [
        line.strip()
        for line in (repo_root / "benchmarks/b1/godot-ai-python-freeze.txt").read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    return [line for line in lines if not line.casefold().startswith("godot-ai @ file://")]


def ensure_godot_ai(
    *,
    repo_root: Path,
    local_base: Path,
    git: str,
) -> dict[str, Any]:
    tools = local_base / "tools"
    source = tools / f"godot-ai-{GODOT_AI_COMMIT}"
    if not source.is_dir():
        run([git, "clone", GODOT_AI_REPOSITORY, str(source)], cwd=tools)
    current = git_head(git, source)
    if current != GODOT_AI_COMMIT:
        run([git, "fetch", "--tags", "origin"], cwd=source)
        run([git, "checkout", "--detach", GODOT_AI_COMMIT], cwd=source)
    if git_head(git, source) != GODOT_AI_COMMIT:
        raise ScoredCohortError("selected godot-ai source commit could not be established")

    venv = tools / f"godot-ai-venv-{GODOT_AI_VERSION}"
    python = venv / "Scripts" / "python.exe"
    marker = venv / ".trace2d-b1-freeze-sha256"
    expected_hash = benchmark_b1_scored_policy.EXPECTED_GODOT_AI_FREEZE_SHA256
    if not python.is_file() or not marker.is_file() or marker.read_text(encoding="utf-8").strip() != expected_hash:
        shutil.rmtree(venv, ignore_errors=True)
        run(python312_command() + ["-m", "venv", str(venv)], cwd=tools)
        python = require_file(python, "Godot Agent Python")
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=".txt", delete=False) as handle:
            requirements = Path(handle.name)
            handle.write("\n".join(expected_python_freeze(repo_root)) + "\n")
        try:
            run([str(python), "-m", "pip", "install", "--disable-pip-version-check", "-r", str(requirements)], cwd=tools)
            run([str(python), "-m", "pip", "install", "--disable-pip-version-check", "--no-deps", str(source)], cwd=tools)
        finally:
            requirements.unlink(missing_ok=True)
        marker.write_text(expected_hash + "\n", encoding="utf-8")

    freeze = run([str(python), "-m", "pip", "freeze"], cwd=tools).stdout.splitlines()
    observed_deps = [line.strip() for line in freeze if line.strip() and not line.casefold().startswith("godot-ai @ file://")]
    if observed_deps != expected_python_freeze(repo_root):
        raise ScoredCohortError("local Godot Agent Python dependency graph differs from qualification freeze")
    version_probe = run(
        [str(python), "-c", "import godot_ai; print(getattr(godot_ai, '__version__', 'unknown'))"],
        cwd=source,
    ).stdout.strip()
    if version_probe not in {GODOT_AI_VERSION, "unknown"}:
        raise ScoredCohortError(f"godot-ai Python package version mismatch: {version_probe}")
    addon = require_dir(source / "plugin" / "addons" / "godot_ai", "selected godot-ai addon")
    return {
        "id": "hi-godot/godot-ai",
        "version": GODOT_AI_VERSION,
        "source_commit": GODOT_AI_COMMIT,
        "source": str(source.resolve()),
        "python": str(python.resolve()),
        "addon": str(addon.resolve()),
        "python_freeze_sha256": benchmark_b1_scored_policy.EXPECTED_GODOT_AI_FREEZE_SHA256,
    }


def configure_and_build(
    *,
    source: Path,
    build: Path,
    targets: list[str],
) -> None:
    cmake = shutil.which("cmake")
    if not cmake:
        raise ScoredCohortError("cmake executable not found")
    run([cmake, "--preset", "windows-msvc", "-B", str(build)], cwd=source)
    run([cmake, "--build", str(build), "--config", "Debug", "--target", *targets, "--parallel"], cwd=source)


def ensure_frozen_trace2d(
    *,
    repo_root: Path,
    local_base: Path,
    git: str,
) -> dict[str, Any]:
    root = local_base / "tools" / f"trace2d-{FROZEN_TRACE2D_COMMIT}"
    source = root / "source"
    build = root / "build"
    if not source.is_dir():
        root.mkdir(parents=True, exist_ok=True)
        run([git, "clone", "--no-hardlinks", "--no-checkout", str(repo_root), str(source)], cwd=root)
        run([git, "checkout", "--detach", FROZEN_TRACE2D_COMMIT], cwd=source)
    if git_head(git, source) != FROZEN_TRACE2D_COMMIT:
        raise ScoredCohortError(f"cached frozen Trace2D source is not {FROZEN_TRACE2D_COMMIT}")
    configure_and_build(
        source=source,
        build=build,
        targets=["trace2d", "trace2d_sprite_process", "trace2d_particle_analyze"],
    )
    trace2d = find_executable(build, "trace2d")
    sprite = find_executable(build, "trace2d_sprite_process")
    particle = find_executable(build, "trace2d_particle_analyze")
    doctor = run([str(trace2d), "doctor", "--json"], cwd=source)
    return {
        "commit": FROZEN_TRACE2D_COMMIT,
        "source": str(source.resolve()),
        "build": str(build.resolve()),
        "trace2d": str(trace2d),
        "sprite_process": str(sprite),
        "particle_analyze": str(particle),
        "doctor": doctor.stdout.strip(),
    }


def ensure_b1_native_verifier(*, repo_root: Path, local_base: Path) -> dict[str, Any]:
    build = local_base / "tools" / "b1-native-verifier-build"
    configure_and_build(source=repo_root, build=build, targets=["trace2d_b1_fixture_verify"])
    executable = find_executable(build, "trace2d_b1_fixture_verify")
    return {"build": str(build.resolve()), "executable": str(executable)}


def build_environment(
    *,
    repo_root: Path,
    local_base: Path,
    profile: dict[str, Any],
    godot: dict[str, Any],
    godot_ai: dict[str, Any],
    trace2d: dict[str, Any],
    native_verifier: dict[str, Any],
) -> dict[str, str]:
    auth = require_file(Path.home() / ".codex" / "auth.json", "file-backed Codex auth")
    extra_roots = [
        str(Path(trace2d["sprite_process"]).parent),
        str(Path(trace2d["particle_analyze"]).parent),
    ]
    env = os.environ.copy()
    env["PYTHONPATH"] = str(repo_root / "scripts")
    env["TRACE2D_BENCH_CODEX_AUTH_FILE"] = str(auth)
    env["TRACE2D_BENCH_GODOT_BIN"] = godot["executable"]
    env["TRACE2D_B1_GODOT_BIN"] = godot["executable"]
    env["TRACE2D_BENCH_TRACE2D_BIN"] = trace2d["trace2d"]
    env["TRACE2D_B1_GODOT_AI_PYTHON"] = godot_ai["python"]
    env["TRACE2D_B1_GODOT_AI_ADDON_DIR"] = godot_ai["addon"]
    env["TRACE2D_B1_FIXTURE_VERIFY_BIN"] = native_verifier["executable"]
    env["TRACE2D_B1_ANIMATION_VERIFY_BUILD_DIR"] = str(local_base / "tools" / "b1-animation-verifier-build")
    env["TRACE2D_BENCH_CODEX_READ_ROOTS"] = os.pathsep.join(extra_roots)
    env["TRACE2D_BENCH_WRAPPER_TIMEOUT"] = "285"
    budget = profile["budget"]
    env["TRACE2D_BENCH_MAX_TOOL_CALLS"] = str(budget["max_tool_calls"])
    env["TRACE2D_BENCH_MAX_INPUT_TOKENS"] = str(budget["max_input_tokens"])
    env["TRACE2D_BENCH_MAX_OUTPUT_TOKENS"] = str(budget["max_output_tokens"])
    return env


def run_isolation_preflight(
    *,
    repo_root: Path,
    run_root: Path,
    env: dict[str, str],
) -> dict[str, Any]:
    root = run_root / "isolation-preflight"
    workspace = root / "workspace"
    workspace.mkdir(parents=True)
    canary = repo_root / "benchmarks" / "b1" / f".codex-isolation-canary-{uuid.uuid4().hex}.txt"
    evidence = root / "isolation.json"
    try:
        canary.write_text(f"TRACE2D-B1-DENY-{uuid.uuid4().hex}", encoding="utf-8")
        completed = run(
            [
                sys.executable,
                "-m",
                WRAPPER_MODULE,
                "probe-isolation",
                "--workspace",
                str(workspace),
                "--canary",
                str(canary),
                "--evidence",
                str(evidence),
                "--timeout",
                str(windows.ISOLATION_TIMEOUT_SECONDS),
            ],
            cwd=repo_root,
            env=env,
            check=False,
            timeout=120,
        )
        preserve_process(root, "wrapper", completed)
    finally:
        canary.unlink(missing_ok=True)
    if completed.returncode != 0 or not evidence.is_file():
        raise ScoredCohortError("real-model Windows ACL isolation preflight failed")
    result = load_json(evidence)
    if result.get("passed") is not True:
        raise ScoredCohortError("real-model Windows ACL isolation verdict is not positive")
    return result


def run_godot_agent_preflight(
    *,
    repo_root: Path,
    run_root: Path,
    env: dict[str, str],
) -> dict[str, Any]:
    root = run_root / "godot-agent-preflight"
    workspace = root / "workspace"
    fixture = repo_root / "benchmarks/b1/qualification/godot_content_fixture"
    shutil.copytree(fixture, workspace)
    before = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    prompt = root / "prompt.md"
    prompt.write_text(
        "This is an unscored transport preflight, not a benchmark task. "
        "Use the connected Godot MCP tools to inspect the current editor state at least once. "
        "Do not edit project files, do not use shell commands, and do not infer success without an MCP tool result. "
        "After one successful Godot MCP inspection, reply with DONE.\n",
        encoding="utf-8",
    )
    result_file = root / "agent-result.json"
    completed = run(
        [
            sys.executable,
            "-m",
            WRAPPER_MODULE,
            "run",
            "--workspace",
            str(workspace),
            "--prompt-file",
            str(prompt),
            "--lane",
            "godot.agent",
            "--result-file",
            str(result_file),
        ],
        cwd=repo_root,
        env=env,
        check=False,
        timeout=300,
    )
    preserve_process(root, "wrapper", completed)
    if completed.returncode != 0 or not result_file.is_file():
        raise ScoredCohortError("Godot Agent Codex/MCP preflight did not complete")
    result = load_json(result_file)
    if result.get("status") != "completed":
        raise ScoredCohortError(f"Godot Agent Codex/MCP preflight status is {result.get('status')}")
    events_path = root / "codex-events.jsonl"
    events = codex_core.parse_jsonl(events_path.read_text(encoding="utf-8")) if events_path.is_file() else []
    tool_names = b1_wrapper.completed_mcp_tool_names(events)
    if not tool_names or not any("godot" in name.casefold() or "editor" in name.casefold() for name in tool_names):
        raise ScoredCohortError("Godot Agent preflight completed without an observed Godot MCP tool call")
    after = benchmark_b0_stable_harness.stable_tree_hash(workspace)
    if after != before:
        raise ScoredCohortError("Godot Agent transport preflight modified the non-scored fixture")
    evidence = {
        "passed": True,
        "tool_names": tool_names,
        "workspace_sha256": after,
        "agent_result": result,
    }
    write_json(root / "preflight.json", evidence)
    return evidence


def local_verifier_preflight(
    *,
    repo_root: Path,
    run_root: Path,
    env: dict[str, str],
) -> dict[str, Any]:
    root = run_root / "verifier-preflight"
    root.mkdir(parents=True)
    cases: list[dict[str, Any]] = []
    for task_id in benchmark_b1.EXPECTED_TASKS:
        for lane_id in ("godot.generic", "trace2d.agent"):
            engine_dir = "godot" if lane_id.startswith("godot.") else "trace2d"
            task_root = repo_root / "benchmarks/b1/tasks" / task_id / engine_dir
            for fixture_name, expected_exit in (("known_good", 0), ("known_bad_seeded_defect", 1)):
                workspace = root / task_id / lane_id.replace(".", "-") / fixture_name
                shutil.copytree(task_root / fixture_name, workspace)
                output = workspace.parent / f"{fixture_name}.verifier.json"
                command = [
                    sys.executable,
                    str(VERIFIER),
                    "--task",
                    task_id,
                    "--lane",
                    lane_id,
                    "--workspace",
                    str(workspace),
                    "--repo-root",
                    str(repo_root),
                    "--animation-build-dir",
                    env["TRACE2D_B1_ANIMATION_VERIFY_BUILD_DIR"],
                    "--godot-bin",
                    env["TRACE2D_B1_GODOT_BIN"],
                    "--trace2d-fixture-verifier",
                    env["TRACE2D_B1_FIXTURE_VERIFY_BIN"],
                    "--output",
                    str(output),
                ]
                completed = run(command, cwd=repo_root, env=env, check=False, timeout=240)
                expected_status = "pass" if expected_exit == 0 else "fail"
                evidence = load_json(output) if output.is_file() else {}
                observed_status = evidence.get("verdict", {}).get("status")
                passed = completed.returncode == expected_exit and observed_status == expected_status
                cases.append(
                    {
                        "task_id": task_id,
                        "lane_id": lane_id,
                        "fixture": fixture_name,
                        "expected_exit": expected_exit,
                        "return_code": completed.returncode,
                        "expected_status": expected_status,
                        "observed_status": observed_status,
                        "passed": passed,
                    }
                )
                if not passed:
                    write_json(root / "summary.json", {"passed": False, "cases": cases})
                    raise ScoredCohortError(
                        f"local verifier preflight failed: {task_id}/{lane_id}/{fixture_name}"
                    )
    summary = {"passed": True, "cases": cases}
    write_json(root / "summary.json", summary)
    return summary


def validate_raw_against_schedule(
    *,
    raw_path: Path,
    schedule: list[dict[str, Any]],
    profile_hash: str,
) -> list[dict[str, Any]]:
    records = benchmark_b0.verify_jsonl_chain(raw_path)
    if len(records) != len(schedule):
        raise ScoredCohortError(f"expected {len(schedule)} scored records, got {len(records)}")
    for index, (record, slot) in enumerate(zip(records, schedule), start=1):
        if record.get("scored") is not True:
            raise ScoredCohortError(f"slot {index} is not marked scored")
        if record.get("task_id") != slot["task_id"] or record.get("lane_id") != slot["lane_id"]:
            raise ScoredCohortError(f"slot {index} does not match preregistered task/lane order")
        if record.get("agent_profile_sha256") != profile_hash:
            raise ScoredCohortError(f"slot {index} Agent profile hash mismatch")
        if int(record.get("metrics", {}).get("human_interventions", -1)) != 0:
            raise ScoredCohortError(f"slot {index} recorded human intervention")
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description="Trace2D B1 preregistered 27-slot scored cohort")
    parser.add_argument("--runs-root")
    parser.add_argument("--prepare-only", action="store_true", help="run all unscored setup/preflights but do not start slot 1")
    args = parser.parse_args()

    if os.name != "nt":
        raise ScoredCohortError("B1 scored cohort requires the qualified native Windows ACL host")

    repo_root = Path(__file__).resolve().parents[1]
    policy, schedule = benchmark_b1_scored_policy.load_and_validate_policy(
        repo_root / "benchmarks/b1/scored-cohort-v1.json", repo_root
    )
    suite = benchmark_b1.load_and_validate_suite(repo_root / "benchmarks/b1/suite.json", repo_root)
    profile = load_json(repo_root / str(policy["agent_profile"]))
    profile_hash = benchmark_b0.sha256_json(profile)
    if profile_hash != policy["agent_profile_canonical_sha256"]:
        raise ScoredCohortError("frozen Agent profile canonical hash mismatch")
    if suite.get("frozen_source", {}).get("trace2d_commit") != FROZEN_TRACE2D_COMMIT:
        raise ScoredCohortError("B1 frozen Trace2D source commit changed")

    git = shutil.which("git")
    if not git:
        raise ScoredCohortError("git executable not found")
    validate_production_source_frozen(git, repo_root)
    repository_head = git_head(git, repo_root)

    codex = codex_core.resolve_codex("codex")
    actual_codex = codex_core.verify_codex_version(codex, repo_root)
    if actual_codex != CODEX_VERSION:
        raise ScoredCohortError(f"Codex mismatch: expected {CODEX_VERSION}, got {actual_codex}")
    login = run([codex, "login", "status"], cwd=repo_root, check=False)
    if login.returncode != 0:
        raise ScoredCohortError(f"codex login status failed: {login.stdout}\n{login.stderr}")

    local_appdata = os.environ.get("LOCALAPPDATA")
    if not local_appdata:
        raise ScoredCohortError("LOCALAPPDATA is required on the Windows owner-local host")
    local_base = Path(local_appdata) / "Trace2D" / "b1"
    runs_root = Path(args.runs_root).expanduser().resolve() if args.runs_root else local_base / "runs"
    runs_root.mkdir(parents=True, exist_ok=True)

    stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime())
    run_root = runs_root / f"codex-chatgpt-b1-scored-{stamp}-{uuid.uuid4().hex[:8]}"
    scored_root = run_root / "scored"
    logs_root = run_root / "orchestration-logs"
    replay_path = run_root / "replay.jsonl"
    run_root.mkdir(parents=True)
    scored_root.mkdir()
    zip_path = run_root.with_suffix(".zip")

    primary_error: Exception | None = None
    stage = "toolchain"
    completed_all = False
    scored_started = False
    env: dict[str, str] = os.environ.copy()
    try:
        godot = ensure_official_godot(local_base)
        godot_ai = ensure_godot_ai(repo_root=repo_root, local_base=local_base, git=git)
        trace2d = ensure_frozen_trace2d(repo_root=repo_root, local_base=local_base, git=git)
        native_verifier = ensure_b1_native_verifier(repo_root=repo_root, local_base=local_base)
        env = build_environment(
            repo_root=repo_root,
            local_base=local_base,
            profile=profile,
            godot=godot,
            godot_ai=godot_ai,
            trace2d=trace2d,
            native_verifier=native_verifier,
        )
        write_json(
            run_root / "toolchain.json",
            {
                "codex_version": actual_codex,
                "repository_head": repository_head,
                "frozen_trace2d": trace2d,
                "godot": godot,
                "godot_ai": godot_ai,
                "native_verifier": native_verifier,
                "agent_profile_canonical_sha256": profile_hash,
            },
        )

        stage = "isolation_preflight"
        isolation = run_isolation_preflight(repo_root=repo_root, run_root=run_root, env=env)
        stage = "godot_agent_preflight"
        godot_preflight = run_godot_agent_preflight(repo_root=repo_root, run_root=run_root, env=env)
        stage = "verifier_preflight"
        verifier_preflight = local_verifier_preflight(repo_root=repo_root, run_root=run_root, env=env)

        write_json(
            run_root / "cohort-manifest.json",
            {
                "schema_version": 1,
                "kind": "trace2d_b1_scored_cohort_run",
                "cohort_id": policy["cohort_id"],
                "repository_head": repository_head,
                "frozen_trace2d_commit": FROZEN_TRACE2D_COMMIT,
                "agent_profile_canonical_sha256": profile_hash,
                "selected_godot_agent_commit": GODOT_AI_COMMIT,
                "selected_godot_agent_python_freeze_sha256": benchmark_b1_scored_policy.EXPECTED_GODOT_AI_FREEZE_SHA256,
                "schedule": schedule,
                "retry_policy": policy["retry_policy"],
                "preflight": {
                    "isolation_passed": isolation.get("passed") is True,
                    "godot_agent_mcp_passed": godot_preflight.get("passed") is True,
                    "verifier_dispatch_passed": verifier_preflight.get("passed") is True,
                },
                "scored": True,
            },
        )

        if args.prepare_only:
            write_json(run_root / "prepare-only.json", {"passed": True, "scored_slots_started": 0})
            print(f"B1 prepare-only evidence: {run_root}")
            return 0

        stage = "scored_trials"
        scored_started = True
        exits: list[dict[str, Any]] = []
        for slot in schedule:
            slot_number = int(slot["slot"])
            repetition = int(slot["repetition"])
            task_id = str(slot["task_id"])
            lane_id = str(slot["lane_id"])
            lane_env = env.copy()
            lane_env["TRACE2D_BENCH_ENGINE_VERSION"] = (
                GODOT_VERSION if lane_id.startswith("godot.") else f"trace2d@{FROZEN_TRACE2D_COMMIT}"
            )
            trial_id = f"{task_id}-{lane_id}-scored-r{repetition}"
            print(f"Running scored B1 slot {slot_number}/27: r{repetition} {task_id} / {lane_id}")
            attempt = run(
                [
                    sys.executable,
                    str(HARNESS),
                    "run-trial",
                    "--task",
                    task_id,
                    "--lane",
                    lane_id,
                    "--runs-root",
                    str(scored_root),
                    "--trial-id",
                    trial_id,
                    "--scored",
                ],
                cwd=repo_root,
                env=lane_env,
                check=False,
                timeout=360,
            )
            preserve_process(logs_root, f"slot-{slot_number:02d}-{task_id}-{lane_id.replace('.', '-')}", attempt)
            exits.append(
                {
                    "slot": slot_number,
                    "repetition": repetition,
                    "task_id": task_id,
                    "lane_id": lane_id,
                    "trial_id": trial_id,
                    "return_code": attempt.returncode,
                }
            )
            windows.scrub_transient_codex_state(scored_root)
        write_json(run_root / "scored-exit-codes.json", exits)

        raw_path = require_file(scored_root / "raw.jsonl", "scored raw records")
        records = validate_raw_against_schedule(raw_path=raw_path, schedule=schedule, profile_hash=profile_hash)

        stage = "aggregate_report"
        report = run(
            [sys.executable, str(HARNESS), "report", "--records", str(raw_path)],
            cwd=repo_root,
            env=env,
        )
        preserve_process(logs_root, "report", report)
        (run_root / "scored-report.json").write_text(report.stdout, encoding="utf-8")
        report_json = json.loads(report.stdout)
        if int(report_json.get("record_count", -1)) != len(schedule):
            raise ScoredCohortError("B1 scored report count mismatch")
        if report_json.get("integrity", {}).get("same_agent_profile_per_task") is not True:
            raise ScoredCohortError("B1 scored cohort mixed Agent profile hashes")

        stage = "independent_reverify"
        replay_exits: list[dict[str, Any]] = []
        for index, record in enumerate(records, start=1):
            trial_id = str(record["trial_id"])
            replay = run(
                [
                    sys.executable,
                    str(HARNESS),
                    "reverify",
                    "--trial-id",
                    trial_id,
                    "--records",
                    str(raw_path),
                    "--replay-records",
                    str(replay_path),
                ],
                cwd=repo_root,
                env=env,
                check=False,
                timeout=240,
            )
            preserve_process(logs_root, f"reverify-{index:02d}", replay)
            replay_exits.append({"slot": index, "trial_id": trial_id, "return_code": replay.returncode})
        write_json(run_root / "reverify-exit-codes.json", replay_exits)
        if any(item["return_code"] != 0 for item in replay_exits):
            raise ScoredCohortError("one or more independent B1 reverifications disagreed with preserved evidence")
        replay_records = benchmark_b0.verify_jsonl_chain(replay_path)
        if len(replay_records) != len(schedule):
            raise ScoredCohortError("B1 independent reverify record count mismatch")

        completed_all = True
    except Exception as exc:
        primary_error = exc
        write_json(
            run_root / "failure.json",
            {
                "schema_version": 1,
                "kind": "trace2d_b1_scored_cohort_failure",
                "stage": stage,
                "exception_type": type(exc).__name__,
                "message": str(exc),
                "scored_started": scored_started,
                "replacement_or_retry_allowed": False,
            },
        )
    finally:
        windows.scrub_transient_codex_state(run_root)
        package = run(
            [sys.executable, str(PACKAGER), "--run-root", str(run_root), "--output", str(zip_path)],
            cwd=repo_root,
            env=env,
            check=False,
        )
        if package.returncode != 0:
            raise ScoredCohortError(
                f"B1 evidence packager failed ({package.returncode}): {package.stdout}\n{package.stderr}"
            )

    print(f"Evidence ZIP: {zip_path}")
    if primary_error is not None:
        suffix = (
            " Scored execution had started; do not rerun or replace any failed slot. Upload this ZIP for review."
            if scored_started
            else " Failure occurred before slot 1, so no scored result was observed; fix only the preflight/toolchain issue before trying again."
        )
        raise ScoredCohortError(f"B1 scored cohort did not complete: {primary_error}.{suffix}") from primary_error
    if not completed_all:
        raise ScoredCohortError("B1 scored cohort did not complete")
    print("B1 preregistered scored cohort completed with exactly 27 scheduled attempts and 27 reverifications.")
    print("Upload the evidence ZIP for aggregate/presentation review; do not rerun any slot.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ScoredCohortError as exc:
        print(f"B1 scored cohort error: {exc}", file=sys.stderr)
        raise SystemExit(2)
