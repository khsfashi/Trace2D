# Trace2D Project Status

Last explanatory handoff update: **2026-08-14**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state through the repository-state tooling. Do not guess a merge or active child from this Markdown file when those sources disagree.

## Current program

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Complete Sprite #59 and Benchmark B1 #103 are merged and closed. PR #174 merged B1 to `main` as `1ba74562000425cde2a5eab342edf8f0315a6092`.

#175 B1 postmortem and PR #181's Agent-complexity product rule are also merged. PR #182 carries the current #69 E0 external Game/Application boundary implementation. While PR #182 is open, #70 remains blocked. After PR #182 merges green, #69 is complete and the exact next core-lane item is #70.

Frozen Sprite contract continuity retained for repository checks:

- #144 / PR #145 — SA0 deterministic Sprite animation timing/frame/event contract remains frozen and complete.
- #142 / PR #143 — SR8 renderer conformance remains the trusted presentation-GPU authority.
- #152 / PR #153 — SA4 deterministic animation conformance/workload evidence remains frozen.
- #172 / PR #173 — SPERF completed the production Sprite program.

## Benchmark B1 — immutable baseline

Exact scored head: `6d6904e99ad7060341861cb3823e04591a579bf7`.

Owner scored workflow run: `31763107941`.

Scored artifact:

- id `9206626314`,
- `benchmark-b1-scored-6d6904e99ad7060341861cb3823e04591a579bf7`,
- SHA-256 `74ab53220927f557621c96ee7b8df7395010e60c191d3959705ab7ba09f8d4d6`.

Frozen aggregate result:

- Godot Generic: 5/9 successful (55.6%).
- Trace2D Agent: 3/9 successful (33.3%).
- selected Godot Agent: 0/9 successful.
- Trace2D by task: Sprite 1/3, animation 2/3, particle 0/3.
- Godot Generic by task: Sprite 2/3, animation 0/3, particle 3/3.

These are cohort observations, not universal product claims. Do not rerun, replace, repair or retrospectively alter scored B1 slots.

## #175 postmortem finding

`docs/BENCHMARK_B1_POSTMORTEM.md` and `benchmarks/b1/postmortem-v1.json` freeze the evidence-backed interpretation.

The key distinction is:

- Trace2D scored success: **3/9**,
- final independent verifier acceptance: **9/9**,
- unsuccessful scored slots: **6/6 final verifier pass**, all classified `budget_exceeded` because input tokens exceeded the frozen 100k ceiling,
- output-token exhaustion: 0,
- tool-call exhaustion: 0,
- timeout: 0,
- deterministic verifier rejection: 0,
- human intervention: 0,
- engine-native authoring operations observed in the nine Trace2D traces: 0.

B1 therefore demonstrates an Agent-facing authoring-surface efficiency/discoverability defect for these cases, not a deterministic correctness defect in the six final resources.

The repeated context amplifiers were raw TOML/C++ editing, duplicate/removed-key repair, rereading, ad-hoc validation, non-Git workspace `git diff` help output, and in r3 secondary sandbox/helper noise. Do not solve this by simply enlarging prompts/budgets or proliferating benchmark-specific MCP tools.

## Agent Complexity Budget

Future authoring contracts record at least:

- input/output tokens,
- tool calls,
- distinct exposed concepts/resources,
- revisions,
- visual-feedback calls,
- human interventions.

For the demonstrated single-resource deterministic repair class, the product-surface target is:

- one discoverable public `trace2d` authoring root,
- raw text editing not required,
- Git metadata not required,
- at most one primary semantic mutation,
- at most one deterministic validation call after mutation,
- expected authoring revision count one,
- zero visual-feedback calls required for deterministic acceptance,
- compact structured output rather than full-resource echo by default.

Animation remains the strongest Trace2D B1 task at 2/3. Preserve exact-event semantics, compact deterministic state, headless exact-time inspection and semantic/presentation separation. B1 alone does not justify a separate animation-fix issue.

## #69 E0 external Game/Application boundary

PR #182 implements the first external game-production stage with one source-level `Trace2D::Application` boundary and an external `ExampleGame` under `examples/`, outside `engine/`.

The E0 contract intentionally keeps ownership narrow:

```text
Application owns Runtime + Input + Scene + UI
 -> external Game receives GameContext
 -> Application alone advances Input frame + fixed Runtime frame
 -> Game gets one OnFixedUpdate per authoritative fixed step
 -> optional host presentation reads the same canonical state
```

Key constraints carried by PR #182:

- game code cannot step Runtime or advance Input frames,
- host input enters only through bounded `ApplyInput` / `ScheduleInput`,
- headless and windowed hosts use the same external Game class,
- SDL/render/MCP/backend types stay out of GameContext,
- existing WorkSpec/WorkResult/VerificationRecord and Agent inspection are reused over canonical application-owned state,
- no binary plugin ABI, generic service locator, reflection/ECS/event framework, or Agent-only game database,
- no required per-fixed-step heap allocation/filesystem/parsing/report/GPU-readback work is added by Application orchestration.

`docs/GAME_APPLICATION_E0.md` records the engine-facing lifecycle, the five exposed E0 concepts in the representative lifecycle + discovery path, bounded public operations, and why the projection does not create a second authority.

Do not start #70 from this branch merely because #182 exists. #70 becomes current only after #182 is merged green and #69 is closed by live repository state.

## Evidence-backed implementation follow-ups

Only two dedicated implementation issues are justified directly by B1:

- **#178 — transactional Sprite resource mutation and deterministic validation**,
- **#179 — transactional Particle constraint mutation and deterministic validation**.

Both must use the existing production parser/serializer/validator/compiler authorities, mutate typed state transactionally, preserve unspecified intent, validate before atomic commit, emit bounded machine-readable diagnostics, and add no normal-frame parsing/filesystem/report/GPU cost.

They are B2 prerequisites, but they do **not** jump ahead of the external-game foundation. The core lane is intentionally:

```text
#175 postmortem
 -> #69 -> #70 -> #71 -> #86 -> #87 -> #88
 -> #72 -> #73 -> #74 -> #75
 -> #178 Sprite Agent authoring
 -> #179 Particle Agent authoring
 -> #104 B2
 -> remaining production foundation
```

## B2 entry gate

B2 remains blocked until:

1. #175 postmortem is merged,
2. #178 is implemented and independently tested on non-B1 fixtures,
3. #179 is implemented and independently tested on non-B1 fixtures,
4. B1 tasks/verifiers/results/artifact identity remain unchanged,
5. B2 uses new held-out tasks or variants frozen before scoring,
6. budgets, verifier authorities, retry/exclusion rules and baseline identities are preregistered again.

B2 tests generalization after architectural improvement; it is not a rerun-until-win exercise.

## Long-range roadmap additions

### #177 — Source-neutral Asset Intelligence Pipeline and Asset IR

Generated, imported and hand-authored images enter one source-neutral boundary:

```text
AssetSource
 -> AssetInput
 -> deterministic preparation / bounded semantic analysis
 -> Trace2D Asset IR
 -> native runtime and optional interoperability
```

ComfyUI and other creation tools remain optional adapters/sources. Deterministic analysis owns objective geometry/schema facts; semantic/VLM inference is bounded to ambiguous or aesthetic questions. Heavy analysis stays offline/setup-side.

### #176 — Native Deterministic Skeletal Animation

Trace2D owns a compact native skeleton/animation representation rather than making Spine the native architecture:

```text
NS0 Skeleton IR
 -> NS1 independent reference evaluator
 -> NS2 optimized deterministic evaluator
 -> NS3 rigid region attachments
 -> NS4 animation tracks + exact events
 -> NS5 deterministic mixing
 -> NS6 Agent/headless QA
 -> NS7 weighted Mesh2D skinning
 -> NS8 Asset IR auto-rig handoff
 -> NS9 generative E2E
```

Normal animation evaluation uses pre-resolved stable indices and reusable contiguous buffers with no ordinary per-frame allocation, filesystem/JSON work, string lookup or hierarchy discovery.

### #61 — Spine compatibility stays optional

Native Skeleton is core capability. Spine remains an explicit license-gated optional compatibility/import/export/runtime adapter and must not define Trace2D's native semantics.

## Continuation rule

The next `@GitHub Trace2D 다음 진행해줘` must resolve live state first.

- If PR #182 is still open, continue #69 only: inspect exact-head CI/review state and fix E0 if needed. Do **not** start #70.
- If PR #182 is merged and #69 is closed, the exact next core implementation item is **#70 — project manifest + external consumer build/install/package**.
- #178/#179 remain registered before #104 and must not be pulled forward unless the repository owner explicitly changes the fixed lane.
