#!/usr/bin/env python3
"""Benchmark B2 owner-local Codex adapter.

B2 reuses the already-qualified B0/B1 Windows ACL isolation implementation but
pins the independently qualified B2 Godot Agent identity and keeps B2 metadata
separate from immutable B1 evidence. The adapter adds only execution mechanics:
the frozen task prompt itself is never edited.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

import benchmark_b1_codex_windows_acl_wrapper as base
import benchmark_b0_codex_wrapper as core
import benchmark_b0_codex_windows_acl_wrapper as windows

GODOT_AI_ID = "hi-godot/godot-ai"
GODOT_AI_VERSION = "3.1.5"
GODOT_AI_COMMIT = "09a1e3311015153d967710fbe6502ac519585a9b"
GODOT_AI_PACKAGE_IDENTITY = "sha256:51863ba177c66299808aa19ef6cd9069768915b2434d7787b9300e40c3620b04"
B2_PERMISSION_PROFILE = f":workspace+{windows.ISOLATION_BACKEND}"


class B2WrapperError(base.B1WrapperError):
    pass


def _alias_env(target: str, source: str) -> None:
    value = os.environ.get(source, "").strip()
    if value:
        os.environ[target] = value
    else:
        os.environ.pop(target, None)


def _execution_mechanics(lane: str) -> str:
    shared = """

---
B2 owner-runner execution handoff (mechanics only; the frozen task above is unchanged):
- Keep all authored game changes inside the provided workspace.
- Retain at least one final presentation capture under `.trace2d-b2-evidence/presentation/`
  as PNG/JPEG/WebP/BMP. Presentation evidence is not deterministic gameplay authority.
- Do not edit benchmark policy, task, verifier, or harness files.
"""
    if lane.startswith("godot."):
        return shared + "- The qualified Godot verifier loads the playable entry scene from `res://main.tscn`.\n"
    return shared + """- The qualified Trace2D verifier links `B2Candidate.cpp` from the workspace.
  That source must expose the normal game through exactly:
  `std::unique_ptr<trace2d::application::Game>
  trace2d::benchmark::b2::CreateCandidate(trace2d::scene::ComponentRegistry&)`.
  This is only the verifier handoff boundary; gameplay must still use normal public engine paths.
"""


def _sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def configure_b2() -> None:
    _alias_env("TRACE2D_B1_GODOT_AI_PYTHON", "TRACE2D_B2_GODOT_AI_PYTHON")
    _alias_env("TRACE2D_B1_GODOT_AI_ADDON_DIR", "TRACE2D_B2_GODOT_AI_ADDON_DIR")
    base.GODOT_AI_ID = GODOT_AI_ID
    base.GODOT_AI_VERSION = GODOT_AI_VERSION
    base.GODOT_AI_COMMIT = GODOT_AI_COMMIT
    base.B1_PERMISSION_PROFILE = B2_PERMISSION_PROFILE
    base.configure()


def run_b2_trial(args: argparse.Namespace) -> int:
    result = Path(args.result_file).expanduser().resolve()
    frozen_prompt_path = Path(args.prompt_file).expanduser().resolve()
    frozen_prompt = frozen_prompt_path.read_text(encoding="utf-8")
    mechanics = _execution_mechanics(args.lane)
    effective_prompt = frozen_prompt + mechanics
    effective_path = result.parent / "effective-agent-prompt.md"
    effective_path.write_text(effective_prompt, encoding="utf-8")
    delegated = argparse.Namespace(**vars(args))
    delegated.prompt_file = str(effective_path)
    code = base.run_b1_trial(delegated)
    if not result.is_file():
        return code

    data = json.loads(result.read_text(encoding="utf-8"))
    wrapper = data.setdefault("wrapper", {})
    adapter = wrapper.pop("b1_environment_adapter", None)
    if not isinstance(adapter, dict):
        raise B2WrapperError("underlying execution adapter did not emit environment metadata")

    godot_agent = adapter.get("godot_agent")
    if args.lane == "godot.agent":
        expected = {"id": GODOT_AI_ID, "version": GODOT_AI_VERSION, "commit": GODOT_AI_COMMIT}
        if not isinstance(godot_agent, dict) or any(godot_agent.get(k) != v for k, v in expected.items()):
            raise B2WrapperError("selected B2 Godot Agent identity drifted")
        godot_agent["package_identity"] = GODOT_AI_PACKAGE_IDENTITY

    wrapper["permission_profile"] = B2_PERMISSION_PROFILE
    wrapper["b2_environment_adapter"] = {
        "lane": args.lane,
        "godot_agent": godot_agent if args.lane == "godot.agent" else None,
        "selected_package_identity": GODOT_AI_PACKAGE_IDENTITY if args.lane == "godot.agent" else None,
        "frozen_task_prompt_sha256": _sha256_text(frozen_prompt),
        "execution_mechanics_sha256": _sha256_text(mechanics),
        "effective_prompt_sha256": _sha256_text(effective_prompt),
        "execution_mechanics_appended": True,
        "starter_solution_injected": False,
        "trace2d_benchmark_only_scene_injected": False,
    }
    data["benchmark_id"] = "trace2d-b2"
    result.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return code


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Trace2D B2 frozen Codex Windows adapter")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="run one B2 Agent trial")
    run.add_argument("--workspace", required=True)
    run.add_argument("--prompt-file", required=True)
    run.add_argument("--lane", choices=("godot.generic", "godot.agent", "trace2d.agent"), required=True)
    run.add_argument("--result-file", required=True)
    run.add_argument("--codex", default=os.environ.get("TRACE2D_BENCH_CODEX_BIN", "codex"))
    run.set_defaults(handler=run_b2_trial)

    probe = subparsers.add_parser("probe-isolation", help="reuse the accepted real-model B0 ACL canary")
    probe.add_argument("--workspace", required=True)
    probe.add_argument("--canary", required=True)
    probe.add_argument("--evidence", required=True)
    probe.add_argument("--timeout", type=float, default=90.0)
    probe.add_argument("--codex", default=os.environ.get("TRACE2D_BENCH_CODEX_BIN", "codex"))
    probe.set_defaults(handler=core.run_isolation_probe)
    return parser


def main() -> int:
    try:
        configure_b2()
        args = build_parser().parse_args()
        return int(args.handler(args))
    except (
        B2WrapperError,
        base.B1WrapperError,
        core.WrapperError,
        OSError,
        subprocess.SubprocessError,
        json.JSONDecodeError,
    ) as exc:
        print(f"B2 Codex wrapper error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
