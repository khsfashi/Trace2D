# Trace2D Project Status

Last explanatory handoff update: **2026-08-14**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state through the repository-state tooling. Do not guess a merge or active child from this Markdown file when those sources disagree.

## Current program

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Complete Sprite #59 is merged and closed. Benchmark B1 #103 has now produced its first frozen 27-slot scored cohort on PR #174 / branch `agent/benchmark-b1-content-authoring`.

Frozen Sprite contract continuity retained for repository checks:

- #144 / PR #145 — SA0 deterministic Sprite animation timing/frame/event contract remains frozen and complete.
- #142 / PR #143 — SR8 renderer conformance remains the trusted presentation-GPU authority.
- #152 / PR #153 — SA4 deterministic animation conformance/workload evidence remains frozen.
- #172 / PR #173 — SPERF completed the production Sprite program.

## Benchmark B1 frozen baseline

Exact scored head: `6d6904e99ad7060341861cb3823e04591a579bf7`.

Owner scored workflow run: `31763107941`.

Observed aggregate result:

- Godot Generic: 5/9 successful (55.6%).
- Trace2D Agent: 3/9 successful (33.3%).
- selected Godot Agent: 0/9 successful.
- Trace2D by task: Sprite 1/3, animation 2/3, particle 0/3.
- Godot Generic by task: Sprite 2/3, animation 0/3, particle 3/3.

These values are frozen cohort evidence, not universal product claims. Do not rerun, replace, repair or retrospectively alter B1 scored slots.

The scored workflow completed on the exact approved head and uploaded scored evidence. B1 is therefore preserved as the immutable pre-improvement baseline for future architectural comparisons.

## Immediate analytical gate

New roadmap gate: **#175 — Benchmark B1 postmortem: agent-surface failure taxonomy and B2 entry gate**.

#175 owns the evidence-backed analysis of every unsuccessful Trace2D B1 slot. It must distinguish runtime/engine defects from Agent-facing authoring-surface defects and classify failures such as token/context exhaustion, tool-discovery errors, invalid authored output, verifier rejection, timeout and reasoning failure.

The response to B1 is not to add more tools or longer prompts by default. Prefer reducing Agent-visible state and expressing authoring through smaller deterministic primitives.

#175 also establishes an **Agent Complexity Budget** for future subsystem contracts, measuring at least:

- input/output tokens,
- tool calls,
- distinct exposed concepts/resources,
- revisions/iterations,
- visual-feedback calls,
- human interventions.

Task-specific interpretation:

- Animation 2/3 is a promising architectural signal, not proof of universal superiority. Preserve exact-event semantics, compact inspectable state, deterministic/headless verification and presentation separation where evidence supports them.
- Sprite 1/3 requires failure-level analysis before calling Complete Sprite Agent-optimal.
- Particle 0/3 is treated as a product-level authoring-surface problem until evidence proves otherwise; runtime capability and Agent usability are separate acceptance dimensions.

## B2 rule

B1 remains immutable. B2 must not be a rerun-until-win exercise.

Before B2 scoring:

1. #175 postmortem is complete,
2. evidence-backed authoring-surface improvements are implemented and independently tested,
3. B1 tasks/verifiers/results remain unchanged,
4. B2 uses new held-out tasks or variants frozen before scoring,
5. budgets, verifier authorities, retry/exclusion rules and baseline identities are preregistered again.

## Long-range roadmap additions

### #177 — Source-neutral Asset Intelligence Pipeline and Asset IR

Trace2D should accept generated, imported or hand-authored images through one shared source-neutral boundary:

```text
AssetSource
  -> AssetInput
  -> deterministic preparation / bounded semantic analysis
  -> Trace2D Asset IR
  -> native runtime and optional interoperability
```

ComfyUI, generated-image systems, Aseprite, Photoshop and other tools are adapters/sources only. Core AssetInput/AssetIR must not inherit ComfyUI-specific batch types, output-directory assumptions or Spine dependencies.

Deterministic analysis owns objective questions such as alpha bounds, frame geometry, trim/alignment and schema integrity. Semantic/VLM inference is limited to ambiguous tasks such as part classification, inferred pivot/hierarchy intent and aesthetic audit, and cannot silently become runtime/gameplay authority.

The intended staged direction is:

```text
AI0 Asset input contract
 -> AI1 deterministic preparation
 -> AI2 bounded semantic analysis
 -> AI3 deterministic + visual audit split
 -> AI4 native rig handoff
 -> AI5 optional interoperability
 -> AI6 closed-loop generated/imported asset workflow
```

Heavy segmentation/VLM/export work is offline/setup work and must not leak into ordinary frame execution.

### #176 — Native Deterministic Skeletal Animation

Trace2D will own a compact native skeleton/animation representation instead of making Spine the native architecture.

Fixed umbrella direction:

```text
NS0 Skeleton IR
 -> NS1 independent reference evaluator
 -> NS2 optimized deterministic evaluator
 -> NS3 rigid region attachments
 -> NS4 animation tracks + exact events
 -> NS5 deterministic mixing/transitions
 -> NS6 Agent inspection/headless QA
 -> NS7 weighted Mesh2D skinning
 -> NS8 auto-rig handoff from Asset IR
 -> NS9 generated/imported asset -> rig -> animation -> deterministic QA -> render -> VLM audit E2E
```

The initial MVP stays intentionally small: bones, stable hierarchy, slots, rigid region attachments, translate/rotate/scale tracks, attachment switching and exact events. IK, physics constraints, broad editor-centric state and weighted skinning are not part of the first stage unless later evidence demands them.

Normal animation evaluation should use pre-resolved stable indices and reusable contiguous buffers with no ordinary per-frame allocation, filesystem/JSON work, string lookup or hierarchy discovery. Deterministic semantic pose/event authority stays separate from GPU visual/deformed-geometry conformance.

### #61 — Spine compatibility stays optional

Spine remains an explicit license-gated compatibility target. Native Skeleton is core Trace2D capability; Spine may later be an optional importer/exporter/runtime adapter.

Do not make native authoring semantics depend on proprietary Spine format/editor/runtime behavior and do not reimplement proprietary formats to bypass licensing.

## Roadmap ordering rule

The new #176/#177 umbrellas are explicit long-range plans, not authorization to skip the existing production-foundation sequence. `config/trace2d.core-lane.json` remains the continuation authority.

Current ordering now begins:

```text
B1 frozen baseline
 -> #175 B1 postmortem / Agent-surface gate
 -> #69 external-game application
 -> existing production-foundation sequence
 -> #104 B2 at its registered position, subject to #175 B2 entry requirements
 -> remaining foundation / flagship proof
 -> #60 Mesh2D
 -> #177 Asset Intelligence / Asset IR
 -> #176 Native Deterministic Skeletal Animation
 -> #61 optional Spine compatibility gate
```

Individual #177/#176 stages may only activate when their owning foundations are ready; weighted skinning specifically depends on #60.

## Continuation rule

The following `@GitHub Trace2D 다음 진행해줘` continuation should resolve live state first.

If #103 / PR #174 still needs final presentation/multimodal/human review or merge/closure bookkeeping, finish that without modifying or rerunning scored B1 evidence. Once B1 is accepted/finalized, advance to #175.

#175 should perform the postmortem, create only evidence-backed Sprite/Particle authoring-surface fixes, preserve the successful animation design properties where justified, and keep future B2 held-out and preregistered.
