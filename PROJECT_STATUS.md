# Trace2D Project Status

Last explanatory handoff update: **2026-08-13**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state through the repository-state tooling. Do not guess a merge or active child from this Markdown file when those sources disagree.

## Current program

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Complete Sprite #59 is merged and closed through final SPERF PR #173 / main commit `31712ca419efb232d292680661caea51d8a318e4`.

Active core item: **#103 Benchmark B1 — matched Sprite, animation and particle authoring tasks**.  
Active branch: `agent/benchmark-b1-content-authoring`.  
Active PR: **#174**, intentionally draft until scored multi-run evidence is reviewable.

Exact next core item after B1 completes with reviewable multi-run evidence: **#69 Scene Asset Format v1**.

Repository-state authority/rationale: [`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md).  
Benchmark program contract: [`docs/AUTONOMOUS_BENCHMARK.md`](docs/AUTONOMOUS_BENCHMARK.md).  
B1 working contract: [`benchmarks/b1/README.md`](benchmarks/b1/README.md).

## Frozen predecessors

The B1 work starts from two completed foundations:

- #59 Complete Sprite: S0/S1, SR0-SR8, SA0-SA4, SPP0-SPP5, SE2E and SPERF are frozen.
- #102 Benchmark B0: matched lane schema, trial isolation, append-only traces, independent verification, known-good/known-bad qualification, retry/exclusion policy and raw metric vocabulary are frozen methodology to reuse rather than fork.

Frozen Sprite milestone references retained for contract continuity:

- #144 / PR #145 — SA0 deterministic Sprite animation timing/frame/event contract, frozen and complete.
- #142 / PR #143 / `2108122dad5ac2dcbb964f7ada0e80f7afa21003` — SR8 renderer conformance and trusted presentation-GPU evidence.
- #152 / PR #153 / `c5952c0e905c46816b0a182b7d91143bf54b188b` — SA4 deterministic animation conformance and workload runner.
- #168 / PR #169 / `c3bcac89ca8c7ca21a9130b1b16cf7ece9e31c1a` — SPP5 provider-neutral generation orchestration.
- #170 / PR #171 / `41536c6045b8c89831a2026f090b5be7889599f3` — SE2E generated/imported Sprite proof.
- #172 / PR #173 / `31712ca419efb232d292680661caea51d8a318e4` — SPERF final reproducible Sprite performance evidence.

Relevant Sprite production authorities already exist:

```text
canonical .sprite.toml CPU truth
 + SpriteAnimationClip2D / SpriteAnimator2D exact ns timing and events
 + .trace2d.particle.toml structural particle definitions
 -> existing Agent / MCP inspection and explicit QA surfaces
```

B1 consumes those public contracts. It does not create a benchmark-only asset model or hidden answer API.

## B1 strongest Godot baseline — selected and frozen

#100 required the strongest credible pinned Godot Agent workflow to be refreshed and qualified on a **non-scored** fixture before B1 scored task membership could be frozen. That gate is complete.

Frozen comparison evidence: workflow `31622618958`.

- `hi-godot/godot-ai` at `v3.0.6@f3d99dfbd38c9e095edf1467f85bee507ace2c3a` completed the matched animation/particle authoring fixture, save/readback, presentation-capture handoff, independent known-good acceptance and known-bad rejection in job `94200960755`.
- `@satelliteoflove/godot-mcp@4.1.0` installed, connected, and reached method-event authoring in job `94200960753`, but its pinned Godot addon invokes nonexistent `Animation.method_track_add_key()` on Godot 4.7.1 and cannot complete the preregistered exact-event task through that public animation authoring path.
- `godot-mcp-runtime@3.2.1` remains an unselected fallback, not the B1 strongest-agent lane.

Selected B1 `godot.agent`: **`hi-godot/godot-ai v3.0.6` at exact commit `f3d99dfbd38c9e095edf1467f85bee507ace2c3a`**.

Machine-readable decision: [`benchmarks/b1/baseline-qualification.json`](benchmarks/b1/baseline-qualification.json).  
Comparison rationale: [`benchmarks/b1/qualification/SELECTION.md`](benchmarks/b1/qualification/SELECTION.md).

## B1 scored suite — frozen before scoring

`benchmarks/b1/suite.json` freezes the B1 task membership, budgets, matched lane mappings and fixture paths. `benchmarks/b1/verifiers.json` freezes verifier identities, authority seams and deterministic check sets.

The three scored scenarios cover every preregistered #103 class exactly once:

1. `b1-sprite-normalize-repair` — Sprite import/normalization + trim/pivot/alignment repair,
2. `b1-animation-exact-event` — exact deterministic animation event + exact-frame/headless/presentation evidence separation,
3. `b1-particle-budget-repair` — particle structural/performance budget + seeded-defect diagnosis/repair.

The B0 Agent budget is reused without expansion for every task: 300 seconds, 80 tool calls, 100k input tokens, 20k output tokens and zero human interventions.

`godot.generic` and `godot.agent` are locked to identical Godot starter/known-good/known-bad fixture paths and the same independent verifier identity. The selected hi-godot bridge changes only the interaction adapter. `trace2d.agent` uses semantically matched native Trace2D fixtures and public production authorities.

No scored comparative outcome has been observed on this branch yet.

## B1 frozen fixture qualification — passed

The prerequisite fixture/verifier discrimination gate is complete and recorded in [`benchmarks/b1/fixture-qualification.json`](benchmarks/b1/fixture-qualification.json). The frozen suite and verifier registry were not changed to obtain the result.

Qualification source head: `557c7edf9ee30fd9dca0cc33379731887e79f29a`.

- Godot official `4.7.1.stable.official.a13da4feb`: all three known-good fixtures accepted and all three seeded known-bad fixtures rejected in workflow `31651157113` / job `94295573573`. The evidence artifact is `benchmark-b1-godot-fixture-qualification` / artifact `9162609754`.
- Trace2D native dispatch: six `benchmark_b1_fixture_qualification` CTests passed in workflow `31651157103` / job `94295573606`, exercising the real Sprite parser, animation runtime types and particle parser/compiler.
- The same frozen Godot fixture/verifier dispatch qualifies both `godot.generic` and `godot.agent`; Agent bridge behavior itself remains part of scored execution rather than fixture qualification.

Fixture qualification enables scoring; it is not a scored result.

## B1 scored cohort policy and harness — prepared before scoring

`benchmarks/b1/scored-cohort-v1.json` now preregisters the complete scored cohort **before any scored B1 result is observed**:

- 3 frozen tasks × 3 frozen lanes × 3 repetitions = **27 exact scheduled attempts**,
- B0's frozen lane rotation reused exactly,
- deterministic rotating task order across repetitions,
- zero automatic retries,
- zero infrastructure replacement trials,
- no early stopping,
- every scheduled outcome preserved,
- no best-of-N or weighted composite result.

The selected `hi-godot/godot-ai` Python dependency graph from qualification workflow `31622618958` / artifact `9151863240` is frozen in [`benchmarks/b1/godot-ai-python-freeze.txt`](benchmarks/b1/godot-ai-python-freeze.txt), so scored execution cannot silently resolve the same source tag against newer dependencies.

Owner-local scored infrastructure is implemented by:

- `scripts/benchmark_b1_codex_windows_acl_wrapper.py`,
- `scripts/benchmark_b1_scored_harness.py`,
- `scripts/verify_benchmark_b1_candidate.py`,
- `scripts/package_benchmark_b1_evidence.py`,
- `scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py`.

The harness retains the B0 Codex 0.144.6 / `gpt-5.5` / Windows NTFS ACL / raw usage-accounting semantics. The `godot.agent` adapter reproduces the selected `hi-godot/godot-ai` loopback streamable-HTTP MCP + editor-addon lifecycle. The `trace2d.agent` lane exposes frozen public Trace2D authoring/analysis binaries and deliberately does not inject a benchmark-only scene merely to fit the scene-bound Trace2D MCP executable.

Trace2D animation candidate verification now links an arbitrary preserved candidate through the same already-qualified verifier driver and production `Assets` / `Runtime` authority; held-out verifier behavior is not exposed to the Agent as an authoring shortcut.

The scoring Agent wall budget remains exactly **300 seconds**. A separate **600-second orchestration envelope** only allows the post-turn held-out verifier/evidence path to complete and is not a model/scoring budget expansion.

## Performance / fairness boundary

B1 must not add normal-frame production cost or benchmark-shaped production authority.

Forbidden as a benchmark shortcut:

- Trace2D-only task-specific answer APIs,
- benchmark-detection/input-specific fast paths,
- a second Sprite/animation/particle truth model,
- mandatory per-frame reporting, JSON/string formatting, filesystem work or screenshot capture,
- changing competitor selection, task membership, budgets, retry policy or known-bad mutations after observing which lane wins.

Independent verifier evidence remains authoritative for deterministic acceptance. Presentation captures, multimodal review and final human judgment remain separate evidence layers.

## Current validation gate

Current GitHub validation on PR #174 has passed the B1-specific prerequisites required before owner-local scoring:

- Benchmark B1 Scored Policy,
- Benchmark B1 Contract,
- Benchmark B1 Fixture Qualification,
- Benchmark B1 Selected Godot Agent Qualification,
- B0 Codex Wrapper and B0 Godot Agent Oracle reuse gates,
- repository contract / benchmark contract / full-history Public Alpha release-audit checks.

The scored-policy gate runs:

- `python3 scripts/benchmark_b1.py validate-contract`,
- `python3 scripts/benchmark_b1.py validate-suite`,
- `python3 scripts/benchmark_b1_scored_policy.py --print-schedule`,
- B1 scored-policy / candidate-dispatch / Codex-adapter / scored-harness regression tests,
- Python syntax checks for the complete owner-local scored tooling.

General CI Windows build and clean-clone quick-start may still be running independently; they do not constitute a scored B1 result.

## Continuation rule

Keep #103 and PR #174 / `agent/benchmark-b1-content-authoring` active. Do not begin #69.

The **exact next external gate** is owner-local preflight only:

```powershell
python scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py --prepare-only
```

`--prepare-only` starts **zero scored slots**. Before any score is observed it re-proves the Codex Windows ACL boundary, official Godot identity, selected `hi-godot/godot-ai` source/dependency environment, a real Codex → Godot MCP call, the detached frozen Trace2D toolchain build, and all held-out known-good/known-bad verifier dispatches.

If this preflight fails, fix only the environment/preflight defect and run it again; no scored slot has started.

Only after preflight is positive, run the same script without `--prepare-only` to execute the preregistered 27-slot cohort. Once slot 1 starts, **never rerun or replace a failed scored slot**. Preserve the generated evidence ZIP, independently reverify every preserved workspace, aggregate deterministic results, then perform separate presentation/multimodal/human review before considering B1 complete or advancing to #69.
