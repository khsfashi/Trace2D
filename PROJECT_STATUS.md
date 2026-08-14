# Trace2D Project Status

Last explanatory handoff update: **2026-08-14**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state through the repository-state tooling. Do not guess a merge or active child from this Markdown file when those sources disagree.

## Current program

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Complete Sprite #59 and Benchmark B1 #103 are merged and closed. PR #174 merged B1 to `main` as `1ba74562000425cde2a5eab342edf8f0315a6092`.

#175 B1 postmortem and PR #181's Agent-complexity product rule are merged. PR #182 merged #69 E0 as `20cd8282c1232234ea2bf117287ef167ee14e521`, establishing the external Game/Application boundary. PR #183 merged #70 E1 as `5c4f426542fe4e43e98ef107db88a640f16aeadf`, establishing the installable external project/SDK/package boundary.

**#71 E2 deterministic scene hierarchy and typed authored component composition is the current core stage while PR #184 is open.** Continue/fix #71 only until its exact-head CI and extracted-SDK external consumer gates are green. Once PR #184 merges and #71 closes, the exact next core-lane item is **#86 — R0 unified typed resource lifecycle, dependency, unloading, and memory accounting**.

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

Key constraints retained from E0:

- game code cannot step Runtime or advance Input frames,
- host input enters only through bounded `ApplyInput` / `ScheduleInput`,
- headless and windowed hosts use the same external Game class,
- SDL/render/MCP/backend types stay out of GameContext,
- existing WorkSpec/WorkResult/VerificationRecord and Agent inspection are reused over canonical application-owned state,
- no binary plugin ABI, generic service locator, reflection/ECS/event framework, or Agent-only game database,
- no required per-fixed-step heap allocation/filesystem/parsing/report/GPU-readback work is added by Application orchestration.

`docs/GAME_APPLICATION_E0.md` remains the lifecycle/ownership contract.

## #70 E1 external project / SDK package

PR #183 completed E1 and promoted the same E0 game into a first-class project and install/package consumer rather than creating another gameplay authority.

The committed E1 contract is `docs/EXTERNAL_PROJECT_E1.md`. Its core shape is:

```text
trace2d.project.json
 + project CMakePresets.json / pinned vcpkg.json
 + external Game source/content
        ↓
Trace2D doctor/preflight
        ↓
installed or extracted Trace2D SDK
        ↓
find_package(Trace2D CONFIG REQUIRED)
        ↓
Trace2D::Application / Agent / Platform / Render
        ↓
build + headless test + optional presentation host
```

E1 constraints:

- project ID/startup/content/build/package policy is versioned text and path-independent,
- project metadata does not duplicate #97 WorkSpec intent/Definition-of-Done state,
- game code does not modify `engine/` or link internal source targets,
- only the game-facing public `Trace2D::...` target graph is exported; MCP/testing/repository tools are not declared stable consumer API,
- installed target include paths do not capture absolute repository checkout paths,
- the E1 doctor is explicit setup tooling and keeps `available`, `eligible`, `tested`, and `supported` distinct,
- missing/mismatched vcpkg is a stable setup diagnostic rather than an engine-code failure,
- current package texture policy explicitly records the uncompressed RGBA8/mip limitation and does not pretend it is the permanent production strategy,
- install/package artifacts carry Trace2D license/notices plus SDK identity, and CI records source/toolchain/dependency/package SHA-256 provenance,
- package/install hashing and project discovery do no normal-frame work.

The E1 clean-checkout gate uses the extracted CPack SDK to configure/build the external game and run its headless test. The windowed host must compile, but #70 does not create a new real-GPU evidence authority; renderer/GPU evidence remains with its existing gates.

Current reproducibility scope is explicit: repeat installation from one compiled build must produce the same canonical file-tree digest; repeated ZIP hashes are recorded; independent compiler/linker bit reproducibility, signing, SBOM publication and release attestations are not claimed until Trace2D publishes a user-facing binary/SDK artifact.

## #71 E2 deterministic Scene composition

PR #184 is the E2 implementation candidate. It extends the same application-owned `Scene` rather than creating a second game/Agent state authority.

The E2 contract is frozen in `docs/SCENE_COMPOSITION_E2.md` and `docs/SCENE_FORMAT.md`:

```text
versioned authored scene
 -> create stable entity identities
 -> construct registered typed authored components
 -> typed parse + validation
 -> resolve semantic parent references
 -> deterministic local/world hierarchy
 -> publish one canonical Scene
 -> external Game typed access + Agent inspect/query
```

E2 constraints:

- `Scene` owns entity identity, parent/child hierarchy, local transforms and component instances,
- authored component identity is an explicit stable type ID + schema version; C++ RTTI names, pointer addresses and registry order are not semantic identity,
- a setup-time `ComponentRegistry` is frozen before authored component loading,
- game code uses resolved `ComponentTypeHandle<T>` / generation-safe `ComponentHandle<T>` instead of per-frame string/property dispatch,
- version-2 TOML adds hierarchy and typed authored components while version 1 remains readable and upgrades canonically on save,
- built-in `trace2d.visibility2d` and external `game.health` prove engine/game composition without moving game code into `engine/`,
- `Transform()` remains local authoritative state; world transforms are allocation-free O(hierarchy depth) and no hidden dirty cache is introduced before workload evidence,
- subtree destruction invalidates descendant generations before slot reuse can alias stale handles,
- existing Agent `Query` / `QueryOne` and component snapshot surface are reused; `Hierarchy2D` exposes serializer-safe parent/children/world fields without adding an E2-only tool,
- semantic `#id` Agent queries resolve through the same `Scene::FindBySemanticId` authority used by game code,
- TOML parsing/serialization, semantic string resolution, sorting and Agent snapshot copies are explicit setup/tooling operations, not mandatory fixed-step work,
- no generic ECS/property bag, tracing GC, mandatory shared-ownership atomics, scripting VM or service locator is introduced.

The #71 extracted-SDK gate must compile the external consumer against the packaged public SDK and run its authored scene round-trip + fixed-step + Agent verification. PR #184 does not merge until its exact-head repository CI and external consumer SDK gates are green.

After #71 merges green and closes, **#86 becomes the exact core-lane handoff**. #86 must build the unified typed resource identity/lifetime/dependency/unload/memory contract on top of E2 rather than introducing another handle/resource authority.

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

- If PR #184 for #71 is open, continue #71 only: inspect exact-head CI/review state and fix E2 if needed. Do **not** start #86.
- If PR #184 is merged and #71 is closed, the exact next core implementation item is **#86 — R0 unified typed resource lifecycle, dependency, unloading, and memory accounting**.
- After #86 merges green, continue to #87 as frozen by #86.
- #178/#179 remain registered before #104 and must not be pulled forward unless the repository owner explicitly changes the fixed lane.
