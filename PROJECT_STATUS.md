# Trace2D Project Status

Last explanatory handoff update: **2026-08-14**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state through the repository-state tooling. Do not guess a merge or active child from this Markdown file when those sources disagree.

## Current program

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

The production Sprite program #59, Benchmark B1 #103, and B1 postmortem #175 are complete. The external-game production foundation has now advanced through the following fixed core lane:

```text
#69 external Game/Application
 -> #70 external project / SDK package
 -> #71 deterministic Scene hierarchy + typed authored components
 -> #86 unified typed resource lifecycle
 -> #87 deterministic scene templates + world lifecycle
 -> #88 Camera2D + Viewport2D + deterministic presentation mapping
```

All six stages above are merged and closed.

Recent exact merges:

- #86 — PR #186, merge `399632be2070b7762d2cac72f260b58386251c97`
- #87 — PR #187, merge `207af413cdf3b180fdb68285a5a00afcb0110581`
- #88 — PR #188, merge `146d56c00459fa5b98384a6c9e1e4984839ee9fe`

**The exact next core-lane item is #72 — I0 input/action mapping and deterministic simulation delivery.** Do not reopen #88 optional scope or jump to #73 unless live repository state or an explicit owner direction changes the committed lane.

## Current foundation contracts

The authoritative detailed contracts live in dedicated documents rather than being duplicated here:

- #69 E0: `docs/GAME_APPLICATION_E0.md`
- #70 E1: `docs/EXTERNAL_PROJECT_E1.md`
- #71 E2: `docs/SCENE_COMPOSITION_E2.md` and `docs/SCENE_FORMAT.md`
- #86 R0: `docs/RESOURCE_LIFECYCLE_R0.md`
- #87 W0: `docs/WORLD_LIFECYCLE_W0.md`
- #88 C0: `docs/CAMERA_VIEWPORT_2D.md`

The ownership chain remains one engine authority rather than parallel gameplay/Agent databases:

```text
Application
 -> WorldLifecycle / canonical Scene
 -> typed authored components + typed resources
 -> deterministic fixed-step state
 -> derived presentation / Agent inspection
```

Normal fixed-step work must not acquire new filesystem parsing, semantic string lookup, generic reflection/property-bag dispatch, tracing GC, hidden shared-ownership churn, or mandatory GPU readback costs.

## #88 C0 handoff

PR #188 completed the camera/presentation foundation required before input routing:

- authored typed `trace2d.camera2d` with canonical `vertical_size`, `enabled`, `priority`, and stable target viewport identity,
- one deterministic Scene-owned active-camera selection rule with generation-safe identity and explicit no-camera behavior,
- logical `Viewport2D` dimensions separated from OS/capture target dimensions,
- centered `fit`, `fill`, and `stretch` presentation mapping,
- frozen target dimensions so stale resize coefficients fail explicitly,
- backend-independent world ↔ logical viewport ↔ presentation conversion with a single top-left pixel-edge convention,
- allocation-free checked inverse conversions that explicitly reject invalid/non-invertible state and non-finite input,
- authoritative-current exact-frame sampling separated from explicit previous/current interpolation,
- one resolved logical projection shared by culling, legacy Sprite, production Sprite, and particles,
- generic presentation raster viewport applied once per render pass without overriding the narrower SR6 pixel-perfect contract,
- Agent inspection for selected camera identity and canonical camera state.

Optional #88 features deliberately not pulled into C0:

- camera rotation,
- follow/smoothing/bounds,
- shake.

They remain future needs-driven work because implementing them now would widen legacy Sprite/particle GPU ABI/state without a demonstrated requirement.

Final #188 validation was green on exact head `cf77e62fb9c68cc66556c7ac66fd4709a542b197`:

- Windows MSVC Main CI configure/build/full CTest,
- SPERF evidence and B0 qualification,
- clean-clone README configure/build/test,
- Windows real-GPU Sprite/particle smoke + conformance evidence,
- External Consumer SDK configure/build/install/package/doctor/external-consumer validation,
- Sprite S0/SA0 contract gates,
- Content Evidence,
- B0 Codex Wrapper,
- B0 Godot Agent Oracle.

## #72 entry contract

#72 is now unblocked by #88. The next implementation must consume C0 instead of inventing another camera/screen-coordinate authority.

At minimum, #72 should preserve these boundaries from the committed lane and its issue acceptance criteria:

- host/device input is normalized outside authoritative gameplay state,
- gameplay consumes stable action/axis semantics rather than raw backend key/scancode identity,
- input consumed by deterministic simulation is delivered on fixed-step boundaries,
- pointer/screen input uses the committed C0 presentation → viewport → world conversion and respects fit letterbox/pillarbox exclusion,
- headless/replay tests can inject the same semantic input without SDL/window/GPU dependence,
- normal fixed-step input reads must avoid per-frame parsing, filesystem work, semantic map reconstruction, or avoidable allocation,
- Agent/game inspection must observe the same canonical input/action state used by simulation.

Resolve the live #72 issue before implementation and treat that issue as the detailed acceptance authority.

## Frozen Sprite contract continuity

The following production contracts remain frozen and trusted:

- #144 / PR #145 — SA0 deterministic Sprite animation timing/frame/event contract,
- #142 / PR #143 — SR8 renderer conformance presentation-GPU authority,
- #152 / PR #153 — SA4 deterministic animation conformance/workload evidence,
- #172 / PR #173 — SPERF production Sprite performance evidence.

Do not weaken these contracts while implementing later foundation work.

## Benchmark B1 — immutable baseline

Exact scored head: `6d6904e99ad7060341861cb3823e04591a579bf7`.

Owner scored workflow run: `31763107941`.

Scored artifact:

- id `9206626314`,
- `benchmark-b1-scored-6d6904e99ad7060341861cb3823e04591a579bf7`,
- SHA-256 `74ab53220927f557621c96ee7b8df7395010e60c191d3959705ab7ba09f8d4d6`.

Frozen aggregate result:

- Godot Generic: 5/9 successful (55.6%),
- Trace2D Agent: 3/9 successful (33.3%),
- selected Godot Agent: 0/9 successful,
- Trace2D by task: Sprite 1/3, animation 2/3, particle 0/3,
- Godot Generic by task: Sprite 2/3, animation 0/3, particle 3/3.

These are cohort observations, not universal product claims. Do not rerun, replace, repair, or retrospectively alter scored B1 slots.

The merged #175 postmortem freezes the important distinction:

- Trace2D scored success: **3/9**,
- final independent verifier acceptance: **9/9**,
- unsuccessful scored slots: **6/6 final verifier pass**,
- all six scored failures were `budget_exceeded` above the frozen 100k input-token ceiling,
- deterministic verifier rejection: 0,
- human intervention: 0,
- engine-native authoring operations observed in the nine Trace2D traces: 0.

B1 therefore exposed Agent-facing authoring-surface efficiency/discoverability defects for those cases rather than deterministic correctness defects in the six final resources.

## Agent Complexity Budget

Future authoring contracts record at least:

- input/output tokens,
- tool calls,
- distinct exposed concepts/resources,
- revisions,
- visual-feedback calls,
- human interventions.

For the demonstrated single-resource deterministic repair class, the product-surface target remains:

- one discoverable public `trace2d` authoring root,
- raw text editing not required,
- Git metadata not required,
- at most one primary semantic mutation,
- at most one deterministic validation call after mutation,
- expected authoring revision count one,
- zero visual-feedback calls required for deterministic acceptance,
- compact structured output rather than full-resource echo by default.

Do not solve B1 by simply enlarging prompts/budgets or proliferating benchmark-specific MCP tools.

## Fixed continuation lane

The committed `after_core` order remains:

```text
#69 -> #70 -> #71 -> #86 -> #87 -> #88
 -> #72 -> #73 -> #74 -> #75
 -> #178 Sprite Agent authoring
 -> #179 Particle Agent authoring
 -> #104 B2
 -> #89 material/shader
 -> #90 tween
 -> #76 physics
 -> #77 audio
 -> #91 profiler
 -> #78 Linux toolchain
 -> #92 tiered GPU QA
 -> #79 save/migration
 -> #12 flagship game
 -> #60 Mesh2D
 -> #177 Asset Intelligence / Asset IR
 -> #176 native deterministic skeleton
 -> #61 Spine license gate
```

#178/#179 remain registered before #104 and must not be pulled forward unless the repository owner explicitly changes the fixed lane.

## B2 entry gate

B2 remains blocked until:

1. #175 postmortem is merged,
2. #178 is implemented and independently tested on non-B1 fixtures,
3. #179 is implemented and independently tested on non-B1 fixtures,
4. B1 tasks/verifiers/results/artifact identity remain unchanged,
5. B2 uses new held-out tasks or variants frozen before scoring,
6. budgets, verifier authorities, retry/exclusion rules, and baseline identities are preregistered again.

B2 tests generalization after architectural improvement; it is not a rerun-until-win exercise.

## Long-range roadmap additions

### #177 — Source-neutral Asset Intelligence Pipeline and Asset IR

Generated, imported, and hand-authored images enter one source-neutral boundary:

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

Normal animation evaluation uses pre-resolved stable indices and reusable contiguous buffers with no ordinary per-frame allocation, filesystem/JSON work, string lookup, or hierarchy discovery.

### #61 — Spine compatibility stays optional

Native Skeleton is core capability. Spine remains an explicit license-gated optional compatibility/import/export/runtime adapter and must not define Trace2D's native semantics.

## Continuation rule

The next `@GitHub Trace2D 다음 진행해줘` must resolve live state first.

- If a #72 implementation PR is open, continue/fix #72 only until its exact-head required gates are green; do not start #73.
- If #72 is open with no implementation PR, #72 is the exact next implementation item.
- Only after #72 merges green and closes does the core lane advance to #73.
- If live GitHub state conflicts with this Markdown handoff, live issue/PR/CI state plus `config/trace2d.core-lane.json` wins.
