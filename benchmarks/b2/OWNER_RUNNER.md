# Benchmark B2 owner-local scored runner

This document is execution plumbing for the already-frozen B2 cohort. It does not
change the task, starter bytes, schedule, budget, retry policy, selected baseline,
or deterministic verifier authority.

## Integrity rules

- Run from a clean checkout of merged `main` after the runner PR is green.
- Keep the scored result root outside the repository.
- `raw.jsonl` is append-only. Do not edit, truncate, delete, or replace a failed slot.
- Run exactly the next slot reported by the harness. A skip or rerun is rejected.
- There are zero automatic retries and zero replacement trials.
- The frozen task prompt is copied byte-for-byte into each trial record. The adapter
  appends only explicit execution handoff mechanics needed to retain presentation
  evidence and expose the already-qualified engine-specific verifier entry point.
- Deterministic gameplay pass/fail is owned only by the qualified verifier.
  Presentation evidence is retained separately and cannot repair a deterministic failure.
- `preflight-slot` validates the current slot's required local engine/tool identity
  before a scored trial directory exists, so a simple owner setup mistake cannot consume
  a frozen slot. Once the Agent turn starts, failures are recorded and are not rerolled.

The execution handoff asks every lane to retain at least one final capture under
`.trace2d-b2-evidence/presentation/`. For Godot, the qualified verifier loads
`res://main.tscn`. For Trace2D, the verifier links workspace `B2Candidate.cpp`
and calls the normal-game factory:

```cpp
std::unique_ptr<trace2d::application::Game>
trace2d::benchmark::b2::CreateCandidate(trace2d::scene::ComponentRegistry&);
```

That factory is a verifier handoff boundary only. It does not authorize direct
state assignment or benchmark-specific gameplay shortcuts.

## Required owner environment

Common:

- the frozen Codex CLI/profile already used by the benchmark harness,
- `TRACE2D_BENCH_CODEX_BIN` when `codex` is not on `PATH`,
- official Godot `4.7.1.stable.official.a13da4feb` via
  `TRACE2D_B2_GODOT_BIN` or `TRACE2D_BENCH_GODOT_BIN` for Godot slots.

`godot.agent` additionally uses the frozen selected baseline
`hi-godot/godot-ai==3.1.5`, source commit
`09a1e3311015153d967710fbe6502ac519585a9b`, package identity
`sha256:51863ba177c66299808aa19ef6cd9069768915b2434d7787b9300e40c3620b04`.
Provide its qualified Python executable and addon source through
`TRACE2D_B2_GODOT_AI_PYTHON` and `TRACE2D_B2_GODOT_AI_ADDON_DIR`.

`trace2d.agent` uses the ordinary public Trace2D CLI/filesystem path already
accepted by the benchmark ACL adapter. Provide `TRACE2D_BENCH_TRACE2D_BIN`.
The independent verifier builds from the repository through CMake; override
`TRACE2D_B2_VERIFY_BUILD_ROOT` only when the default local build cache is unsuitable.

## Preferred self-hosted owner automation

The repository already has an owner Windows runner lane with labels
`[self-hosted, windows, x64, trace2d-gpu]`. B2 reuses that trusted owner runner;
a second runner registration is not required when that runner is online.

`.github/workflows/benchmark-b2-owner-scored.yml` is intentionally
`workflow_dispatch`-only and additionally refuses to run unless all of these are true:

- repository is exactly `khsfashi/Trace2D`,
- actor is exactly `khsfashi`,
- dispatched ref is exactly `refs/heads/main`,
- this is workflow attempt 1 (Actions rerun cannot accidentally consume the next slot).

This matters because Trace2D is public and a persistent self-hosted runner must never
execute arbitrary pull-request code merely because a fork can open a PR.

The workflow automatically discovers the next frozen slot from the durable result root:

```text
%LOCALAPPDATA%\Trace2D\benchmark-b2-scored-v1
```

It then prepares only the toolchain required by that lane. The bootstrap script
`scripts/prepare_benchmark_b2_owner_windows.ps1` uses a dedicated cache under
`%ProgramData%\Trace2D\b2-owner-tools` and does **not** replace the machine-global
Codex installation. It verifies/sets:

- isolated Codex CLI `0.144.6` plus the existing Windows ACL-safe shell shim,
- official Godot `4.7.1-stable`, verified against the release `SHA512-SUMS.txt`,
  with runtime identity `4.7.1.stable.official.a13da4feb`,
- for `godot.agent`, exact `godot-ai==3.1.5`, frozen source commit and package SHA-256,
- for `trace2d.agent`, the pinned vcpkg baseline and a freshly built public `trace2d.exe`
  from the exact dispatched `main` checkout.

The local ChatGPT-managed Codex auth file remains local to the runner account. The
workflow does not copy it into the repository, an Actions artifact, or a log.

From GitHub Actions choose **Benchmark B2 Owner Scored Runner** and use:

- `preflight` to prepare the exact next lane and stop after zero-score preflight,
- `run-next` to prepare, preflight, then execute exactly one next frozen slot.

There is no slot-number input on purpose. The append-only result root is the authority
for which slot is next. `run-next` never loops into a second slot. If the started Agent
attempt fails, retain that record and dispatch a new workflow later for the next slot.

## Commands

Validate the frozen pre-score contract without executing a scored Agent turn:

```powershell
python scripts/benchmark_b2_scored_harness.py validate
```

Choose one durable result root outside the checkout and keep using it for the
whole initial cohort. For example:

```powershell
$Runs = Join-Path $env:LOCALAPPDATA "Trace2D\benchmark-b2-scored-v1"
python scripts/benchmark_b2_scored_harness.py next-slot --runs-root $Runs
```

Before every scored slot, run the non-scored local environment preflight. It checks
the exact Godot identity for Godot lanes, the selected `godot-ai` package version
for `godot.agent`, and the public Trace2D CLI path for `trace2d.agent`:

```powershell
python scripts/benchmark_b2_scored_harness.py preflight-slot --runs-root $Runs
```

The first result must report slot 1, repetition 1, lane `godot.generic`. Execute
only that slot:

```powershell
python scripts/benchmark_b2_scored_harness.py run-slot --runs-root $Runs --slot 1
```

`run-slot` repeats the environment preflight before creating the trial directory.
After the Agent turn starts and the attempt is appended, run `next-slot` again.
Even if slot 1 fails due to implementation or infrastructure, do not rerun it:
the next scheduled slot is slot 2. Preserve the entire result root for publication
and later feedback/revision evidence.

The single blinded human-feedback event and deterministic post-feedback
re-verification are a later B2 phase. They must not be applied during these
initial scored construction slots.
