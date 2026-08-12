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

The scored-suite freeze gate is now open. Future hosted qualification revalidates only the selected pin; the historical two-candidate comparison remains immutable evidence.

## B1 required task classes

The preregistered B1 task taxonomy remains:

1. Sprite import/normalization,
2. trim/pivot/alignment repair,
3. deterministic animation with exact semantic event timing,
4. particle authoring/repair under structural and performance constraints,
5. exact-frame/headless evidence separated from presentation review,
6. diagnosis and repair of at least one intentionally seeded content defect.

The exact next step is to freeze `benchmarks/b1/suite.json`, matched lane mappings, budgets, verifier dispatch, and known-good/known-bad fixtures **before** observing any scored comparative outcome.

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

For the selected-baseline stage:

- `python3 scripts/benchmark_b1.py validate-contract`,
- `python3 -m unittest discover -s scripts -p 'test_benchmark_b1.py'`,
- selected `godot.agent` identity and exact source pin are committed,
- comparison workflow/job IDs and the satellite rejection reason are committed,
- future hosted qualification must re-pass the selected hi-godot content fixture,
- no scored B1 result may be observed until suite/task/budget/verifier/fixture freeze is committed.

## Continuation rule

Keep #103 and `agent/benchmark-b1-content-authoring` active. The next continuation should freeze the **matched scored B1 suite** and verifier dispatch using the selected hi-godot pin, then qualify known-good/known-bad fixtures before any scored cohort run. Do not begin #69 until B1 has comparable multi-run scored evidence and the #103 acceptance gate is complete.
