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

Relevant Sprite production authorities already exist:

```text
canonical .sprite.toml CPU truth
 + SpriteAnimationClip2D / SpriteAnimator2D exact ns timing and events
 + .trace2d.particle.toml structural particle definitions
 -> existing Agent / MCP inspection and explicit QA surfaces
```

B1 consumes those public contracts. It does not create a benchmark-only asset model or hidden answer API.

## Current B1 gate — strongest Godot baseline before task freeze

#100 requires the strongest credible pinned Godot Agent workflow to be refreshed and qualified on a **non-scored** fixture before B1 task membership is frozen.

The current primary-source refresh found:

- B0-selected `@satelliteoflove/godot-mcp@4.1.0` remains a current candidate with its previously qualified deterministic runtime/control strengths.
- `hi-godot/godot-ai` has materially advanced to reviewed release `v3.0.6` and exposes dedicated animation/particle authoring relevant to B1.
- `godot-mcp-runtime@3.2.1` remains a carry-forward fallback candidate.

Therefore B1 intentionally starts in `baseline_qualification`, not `scored`.

Machine-readable gate: [`benchmarks/b1/baseline-qualification.json`](benchmarks/b1/baseline-qualification.json).  
Selection rationale: [`benchmarks/b1/BASELINES.md`](benchmarks/b1/BASELINES.md).

The gate forbids committing `benchmarks/b1/suite.json` until non-scored qualification evidence selects and pins the B1 `godot.agent` lane.

## B1 required task classes

The preregistered B1 task taxonomy remains:

1. Sprite import/normalization,
2. trim/pivot/alignment repair,
3. deterministic animation with exact semantic event timing,
4. particle authoring/repair under structural and performance constraints,
5. exact-frame/headless evidence separated from presentation review,
6. diagnosis and repair of at least one intentionally seeded content defect.

After the Godot Agent baseline is selected, freeze matched lane mappings, budgets, known-good/known-bad fixtures and verifier dispatch **before** observing scored comparative outcomes.

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

For the baseline-qualification stage:

- `python3 scripts/benchmark_b1.py validate-contract`,
- `python3 -m unittest discover -s scripts -p 'test_benchmark_b1.py'`,
- exact external candidate pins/source rationale are committed,
- no scored B1 suite exists before qualification,
- the eventual non-scored fixture proves install/start, content authoring, save/readback, headless/runtime verification, capture handoff, known-good acceptance and known-bad rejection.

Once that evidence is committed, select the strongest credible B1 Godot Agent lane and only then freeze the scored suite.

## Continuation rule

Keep #103 and `agent/benchmark-b1-content-authoring` active. The next continuation should implement/run the **non-scored B1 Godot Agent content-capability qualification** and record the selected baseline. Do not begin #69 until B1 has comparable multi-run scored evidence and the #103 acceptance gate is complete.
