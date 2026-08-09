# Trace2D Project Status

Last repository-state update: **2026-08-09**

This document is the operational handoff for the next contributor or coding agent. Live repository code, active PR state, and CI results win over stale prose.

## Current phase

**Public Alpha is released. Issue #41 reproducible renderer performance workloads is complete via merged PR #63. Issue #47 particle deterministic frame/keyed-random contracts is now the active implementation through draft PR #64. Finish/repair/validate #64 before starting #48. After #64 merges green, #48 rich deterministic CPU particle reference simulation is the exact next task.**

`v0.1.0-alpha.1` was published on 2026-08-08. The repository is Public under the MIT License. Post-alpha work extends the proven agent-first loop rather than replacing it.

## Active PR

- **PR #64 — Lock particle frame and keyed-random semantics**
- branch: `agent/particle-determinism`
- issue: **#47 / particle child 1 of 7**
- scope: SDL-free frame ordering/lifetime primitives, stable emitter/random-key inputs, explicit random-channel IDs, exact keyed integer mixing, exact CPU unit/range mapping, and pure regression tests
- no particle storage, authored effect format, emitter component, renderer, compiler, or GPU backend is introduced in this slice
- do not begin #48 while #64 remains open
- if CI/review finds a problem, repair #64 in scope and rerun the repository gates

## Next execution order — owner-fixed

The repository owner fixed the P6 sequence on **2026-08-08** and explicitly extended the post-particle direction on **2026-08-09**. Future coding agents must follow this order unless the repository owner explicitly changes it.

1. **#40 — deterministic texture asset cache/import slice** — complete via PR #45
2. **#42 — text rendering and basic UI primitives** — complete via PR #55
3. **#43 — semantic UI tree and agent interaction** — complete via PR #56
4. **#39 — MCP transport over the completed protocol-independent agent/UI facade** — complete via PR #58
5. **#41 — reproducible renderer performance workloads** — complete via PR #63
6. **#47 — particle deterministic frame/keyed-random contracts** — **active via PR #64**
7. **#48 — rich deterministic CPU particle reference simulation** — exact next after #47 merges green
8. **#49 — text-authored particle effect assets + `ParticleEmitter2D`**
9. **#50 — complete Agent verification over CPU particle reference state**
10. **#51 — CPU particle cost analysis + explicit human backend choice + deterministic particle compiler**
11. **#52 — GPU runtime backend for explicitly GPU-selected effects**
12. **#53 — CPU/GPU conformance, workloads, safe budgets, build flow, and human/LLM guidance**
13. **#59 — Sprite pipeline: production rendering, deterministic animation, generation/import, processing QA, end-to-end motion QA and performance**
14. **#60 — Mesh2D foundation: reusable textured indexed geometry and measured dynamic submission path**
15. **#61 — Spine compatibility: SP0 explicit human license gate, then optional integration only if approved**

Particle umbrella: **#46**. Broad particle contract: [`docs/PARTICLES.md`](docs/PARTICLES.md). Exact #47 semantic contract: [`docs/PARTICLE_DETERMINISM.md`](docs/PARTICLE_DETERMINISM.md).

Sprite umbrella: **#59**. Detailed owner-approved contract: [`docs/SPRITES.md`](docs/SPRITES.md).

Mesh2D umbrella: **#60**. Its fixed M0/M1 boundary is recorded in #60 and the Sprite handoff contract.

Spine compatibility: **#61**. License/integration gate: [`docs/SPINE.md`](docs/SPINE.md).

## Continuation execution rule

The detailed short-command algorithm lives in `AGENTS.md`. Operationally:

- Work only on the first incomplete and unblocked item in the owner-fixed order.
- If that work has an active PR, finish/repair/validate that PR before starting anything later.
- While PR #64 is open, it is the active work item.
- Merge only with green CI/repository gates; if merge becomes a genuine human-only action, report that one action instead of jumping ahead.
- After #64 merges, start #48 directly. Do **not** skip ahead to authored effects, Agent integration, GPU work, Sprite, Mesh2D, or Spine.
- Within particles, complete exactly one of #47 -> #48 -> #49 -> #50 -> #51 -> #52 -> #53 at a time.
- Within #59, follow the exact fixed stage order in `docs/SPRITES.md`, creating/implementing one child issue/PR at a time.
- After #59, complete #60 M0 then M1 one child/PR at a time.
- When #61 is reached, **stop at SP0 if no explicit owner license approval is recorded**. Do not vendor/add/fetch/distribute Spine Runtime code while the gate is blocked.
- MCP is transport, not the engine API.
- Structured semantic state beats pixel inference for gameplay/UI/particle/sprite animation assertions.
- Visual capture remains first-class QA evidence when pixels genuinely matter.

## Active #47 particle determinism contract

PR #64 establishes the semantic source that #48-#53 must preserve.

### Exact frame order

```text
frame N
  -> ApplyCommands
  -> UpdateExisting
  -> ExpireExisting
  -> Emit
  -> Observe
  -> ExtractBackend
```

Consequences:

- only particles existing before frame N update during N,
- particles emitted in frame N are observable at `ageFrames = 0` and do not update immediately,
- after an existing particle updates, reaching `ageFrames == lifetimeFrames` expires it before observation,
- backend extraction happens after authoritative CPU observation state is established.

### Stable spawn ordinal

`ParticleSpawnOrdinal` is a per-emitter 64-bit **spawn-attempt ordinal**. Later emitters must consume an ordinal for every deterministic spawn attempt, including attempts dropped because capacity is full. This prevents capacity pressure from shifting the keyed random values of later scheduled attempts.

### Keyed randomness

Random values are pure functions of:

```text
(globalSeed, emitterStableId, spawnOrdinal, randomChannel)
```

Hard rules:

- no mutable per-emitter PRNG stream,
- no allocation/string/hash/distribution lookup in random helpers,
- emitter identity is stable numeric state, never pointer/allocation/vector/unordered-iteration identity,
- random-channel IDs are explicit stable integers and existing IDs are never renumbered merely because new properties are inserted,
- the exact 64-bit mixing algorithm and domain constants are documented and tested,
- CPU `[0,1)` uses the top 24 random bits multiplied by exactly `2^-24`,
- range mapping uses the documented CPU expression and fixed bit-vector test,
- same key => exact same integer/CPU float bits on the supported deterministic toolchain,
- querying another ordinal/emitter/channel has no side effect on existing values.

Committed reference vector:

```text
globalSeed      = 0x0123456789ABCDEF
emitterStableId = 0xFEDCBA9876543210
spawnOrdinal    = 42
channel         = SpawnPositionX

bits            = 0xE2B5E492311156F8
u32             = 0xE2B5E492
unit-float bits = 0x3F62B5E4
```

See [`docs/PARTICLE_DETERMINISM.md`](docs/PARTICLE_DETERMINISM.md) for the exact integer algorithm, channel table, lifetime examples, and #48 handoff constraints.

## Completed #41 renderer workload foundation — PR #63

Committed deterministic workloads:

| workload | authored | visible | culled | contiguous visible texture runs |
| --- | ---: | ---: | ---: | ---: |
| `dense_single_texture` | 400 | 400 | 0 | 1 |
| `alternating_two_textures` | 400 | 400 | 0 | 400 |
| `interleaved_culling` | 600 | 400 | 200 | 1 |

#41 established:

- machine-readable headless deterministic workload structure,
- actual successful `Renderer::Metrics()` deltas for local windowed runs,
- optional CPU wall-clock `RenderFrame` submission timing with machine/GPU/driver/OS/compiler/build/backend metadata,
- no benchmark-only allocation/JSON/clock work inside `Renderer::RenderFrame`,
- no global texture sorting or culling-semantics change,
- no hosted-CI wall-clock threshold,
- an evidence requirement for future renderer optimization claims.

See [`docs/RENDERER_WORKLOADS.md`](docs/RENDERER_WORKLOADS.md).

## Fixed internal order for #59 Sprite

When #59 becomes active, the order is:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Meaning:

- **S0-S1:** architecture + canonical SpriteAsset model,
- **SR0-SR8:** production-complete traditional Sprite Renderer, not a minimal quad renderer,
- **SA0-SA4:** deterministic `SpriteAnimator2D`, events, Agent/MCP exact-frame QA and workloads,
- **SPP0-SPP5:** deterministic processing/QA, repair, Aseprite/generic and sprite-gen/PerfectPixel-style interoperability, provider-neutral generation orchestration,
- **SE2E:** generation/import -> deterministic cleanup/QA -> canonical asset -> animation -> headless assertions -> render/capture -> motion/visual QA proof,
- **SPERF:** final reproducible CPU/upload/draw/memory/atlas/animation workload guidance.

The complete criteria are in `docs/SPRITES.md`; future agents must not reduce SR0-SR8 to a minimal implementation merely to complete the umbrella quickly.

## Owner decision replacing the old post-#53 choice

Older roadmap prose said that after #53 the owner would choose one of physics/Box2D, sprite animation, or safe hot reload. That choice has already been made and must not be re-opened by a future coding agent:

```text
#53
 -> #59 complete end-to-end Sprite program
 -> #60 generic Mesh2D foundation
 -> #61 Spine SP0 human license gate
```

Physics/Box2D and safe hot reload remain valid future breadth candidates, but they are **not** the owner-fixed next tasks after #53.

## Spine boundary

Spine is a planned compatibility backend/integration, not Trace2D's native animation architecture.

Before #61 SP0 owner approval:

- no `spine-cpp` vendoring/copy,
- no package/submodule/download dependency,
- no prebuilt Spine-containing binary,
- no Spine-derived implementation code,
- no claim that Trace2D ships Spine support.

`docs/SPINE.md` records the official licensing snapshot reviewed on 2026-08-09 and requires then-current official terms to be re-checked at SP0.

If SP0 is approved after license confirmation, the fixed conceptual order is SP1 optional official runtime adapter -> SP2 Mesh2D render integration -> SP3 semantic animation state -> SP4 Agent/MCP QA/conformance/workloads.

## Completed post-alpha foundations

### #40 texture assets — PR #45

- deterministic project-relative texture identity,
- rejection of absolute paths and `..` traversal,
- PNG/JPEG/BMP/TGA decode to immutable RGBA8 CPU assets,
- successful-import caching with explicit invalidation/clear,
- stable diagnostics/cache metrics,
- no per-frame filesystem discovery or decode,
- SDL-free `engine/assets` with no renderer/GPU ownership.

See [`docs/ASSETS.md`](docs/ASSETS.md).

### #42 text/basic UI — PR #55

- SDL-free `engine/ui`,
- strict versioned `*.trace2d.toml` UI authoring,
- stable IDs and deterministic authored order,
- panel/label/button/text-input primitives,
- deterministic integer pixel bounds,
- engine-owned focus/activation state,
- dependency-free 5x7 ASCII-oriented text,
- caller-owned/reused RGBA8 CPU raster storage,
- headless and windowed preview paths over the same document.

See [`docs/UI.md`](docs/UI.md).

### #43 semantic UI automation — PR #56

- stable semantic ID/role/name/visibility,
- protocol-independent Agent UI inspection/query/focus/activation/text/assertion,
- deterministic authored-order results,
- semantic UI -> authoritative scene-state verification without coordinate targeting,
- no DOM/browser abstraction or renderer-owned UI truth.

### #39 MCP transport — PR #58

- modern MCP `2026-07-28` stdio transport,
- thin adapter over existing Agent/Testing contracts,
- deterministic runtime/scene/UI/input/step/assert protocol tests,
- no JSON/MCP types in lower engine public contracts,
- no renderer requirement for headless protocol tests.

See [`docs/MCP.md`](docs/MCP.md).

## Particle target

Particle implementation is governed by #46, `docs/PARTICLES.md`, and the active child contract.

The defining workflow remains:

```text
rich text effect
  -> deterministic CPU reference simulation
  -> full structured Agent verification
  -> deterministic structural CPU cost report
  -> optional machine-specific Release timing
  -> human chooses backend=cpu or backend=gpu
  -> GPU-selected effects compile to minimized GPU state
  -> CPU/GPU conformance + visual QA
```

Hard rules:

- CPU is the exact semantic reference,
- capacity is explicit and bounded,
- ordinary stepping creates no JSON/snapshot/fingerprint work unless requested,
- structural cost metrics and machine-specific timing are separated,
- an LLM may recommend a backend but never changes it automatically,
- backend choice is explicit reviewable human-controlled text,
- unsupported GPU effects fail clearly rather than silently falling back to CPU,
- normal GPU mode does not also run the full CPU reference simulation,
- GPU runtime state is minimized from compiler/static analysis,
- V1 does not claim universal cross-vendor bit-identical GPU floating point,
- visual particles never become gameplay authority.

## Public Alpha release record

- [x] P0-P5 technical milestones complete
- [x] Public Alpha vertical sample — PR #32
- [x] measured contiguous same-texture GPU instancing — PR #34
- [x] repository quality gates — PR #35
- [x] MIT license-required release gate — PR #36
- [x] release-ready documentation — PR #37
- [x] post-public documentation cleanup — PR #38
- [x] repository visibility Public
- [x] release/tag `v0.1.0-alpha.1`
- [x] root MIT `LICENSE`
- [x] Issue #14 closed completed

## Validation policy

Release-facing CI remains the baseline:

- `release-audit` using `./scripts/release_audit.ps1 -RequireLicense`,
- `windows-msvc` configure/build/full CTest,
- `clean-clone-quick-start` using the README-pinned vcpkg baseline.

New machine-facing capabilities require deterministic automated tests when practical. GPU presentation is not a hosted-runner requirement; backend-independent semantic/math/import/ordering/QA logic must remain headless-CI testable where practical.

Wall-clock timing is environment-dependent evidence and must not become a deterministic correctness threshold.

## Architecture invariants

- `engine/core` has no SDL dependency.
- SDL-specific ownership/types stay behind platform/render boundaries.
- `engine/assets`, `engine/ui`, and `engine/particles` deterministic/CPU semantics are SDL-free.
- renderer GPU state is presentation state, never authoritative gameplay/UI/animation/particle state.
- runtime/scene/input/UI/particles/agent/testing semantic state does not depend on MCP transport.
- MCP/JSON-RPC/CLI are adapters over protocol-independent engine/agent contracts.
- authored project/scene/UI/particle/sprite metadata is text-first and deterministic where practical.
- semantic selectors beat coordinate targeting where identity exists.
- structured state beats pixel inference.
- persistent renderer resources are reused rather than recreated in steady state.
- capture frame selection uses simulation-frame identity, not wall-clock timing.
- texture/material identity never participates in a global reorder that violates painter order.
- particle CPU state is the semantic oracle; GPU is an explicitly selected compiled backend.
- generated Sprite pixels are not canonical until explicit import/validation.
- arbitrary textured geometry belongs to Mesh2D rather than forcing SpriteRenderer into a generic renderer.
- Spine remains outside the MIT-native core unless/until #61 SP0 explicitly approves the integration model.
- optimization complexity follows measurement.

## Handoff rule

Every PR that completes or materially changes an item in the owner-fixed execution order must update this file in the same PR.

A fresh coding agent following `AGENTS.md` should be able to read this file, inspect live PR/CI state, identify the exact first incomplete task or human gate, and continue without previous chat history.
