# Trace2D Project Status

Last repository-state update: **2026-08-10**

This is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/merge/CI state, and explicit owner decisions win over stale prose.

## Current state

**Public Alpha `v0.1.0-alpha.1` is released. Particle children #47-#51 are complete. PR #84 merged #51 CPU cost analysis/compiler. Owner-requested architecture/governance PR #94 is currently active and freezes the missing production-engine integration contracts before later Sprite/game-production implementation. Do not start #52 while PR #94 is open. After #94 merges green, #52 is the exact next core implementation task.**

PR #94 changes contracts/roadmap only; it does not implement a later engine feature or reorder the active particle pair #52 -> #53.

Primary architecture-freeze contract:

- [`docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`](docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md)
- tracking issue #85
- concrete future implementation issues #86-#93

## Completed foundation and particle predecessors

1. **#40** deterministic texture asset cache/import — complete via PR #45
2. **#42** text/basic UI — complete via PR #55
3. **#43** semantic UI tree/Agent interaction — complete via PR #56
4. **#39** MCP transport over Agent/Testing — complete via PR #58
5. **#41** reproducible renderer workloads — complete via PR #63
6. **#47** particle deterministic frame/keyed-random contracts — complete via PR #64
7. **#48** rich deterministic CPU particle reference — complete via PR #65
8. **#49** text-authored particle effects + `ParticleEmitter2D` — complete via PR #66
9. **#50** complete Agent verification over CPU particle reference state — complete via PR #83
10. **#51** CPU particle cost analysis + explicit human backend choice + deterministic particle compiler — complete via PR #84

## Current owner-fixed core execution order

After PR #94 merges, routine `Trace2D next/continue` follows exactly:

```text
#52 explicit GPU particle runtime
 -> #53 CPU/GPU conformance, workloads, safe budgets and guidance
 -> #59 complete Sprite program
 -> #69 Game/Application boundary
 -> #70 Project manifest + external consumer build/install/package
 -> #71 Scene hierarchy + engine/game typed component composition
 -> #86 unified typed resource lifecycle
 -> #87 scene templates + deterministic instancing/world lifecycle
 -> #88 Camera2D + Viewport2D
 -> #72 Input Actions + gamepad/mouse/text/IME
 -> #73 TileSet/TileMap
 -> #74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI hierarchy/layout/widgets
 -> #89 Material2D + Shader2D
 -> #90 deterministic resolved-property tween animation
 -> #76 Physics2D
 -> #77 Audio
 -> #91 unified Agent-readable profiler/diagnostics
 -> #78 Linux/compiler/toolchain hardening
 -> #92 tiered real-GPU conformance/release validation
 -> #79 save/persistence + authored schema migration
 -> #12 flagship external Trace2D game proof
 -> #60 generic Mesh2D foundation
 -> #61 Spine compatibility SP0 human license gate
```

Umbrellas/contracts:

- core roadmap: **#13**
- particle program: **#46**
- Sprite program: **#59**
- game-production foundation: **#67** and children/extensions above
- architecture freeze: **#85** / `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`
- later non-core production breadth gates: **#93**

Issue #93 is deliberately **not** part of routine `next/continue`: it records lighting/shadows, navigation/pathfinding, broader platforms, networking and safe hot reload until an explicit owner decision promotes one area.

## Continuation rule

For `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, `Trace2D continue`, or equivalent routine continuation:

1. read `AGENTS.md` and this file,
2. inspect live open PRs/CI and recent merges,
3. reconcile stale status if live state advanced,
4. if an owner-requested roadmap/governance PR such as #94 is active, finish that contract work first,
5. otherwise finish the active core PR if one exists,
6. if none exists, select the first incomplete/unblocked item from the fixed sequence above,
7. implement exactly one issue/child vertical slice,
8. test/validate/document it,
9. publish/update one scoped PR,
10. do not begin the next core child until the current one merges green.

The owner does **not** need to re-select Sprite vs Physics, whether user gameplay components exist, whether resources need a common lifecycle, whether Camera2D/Material2D/Tween/profiling/GPU conformance belong before the external-game proof, or whether Mesh2D/Spine precede those foundations. Those decisions are now fixed by #13/#85 and the contract documents.

## #51 completion contract

Issue #51 established the explicit analysis/compiler gate between CPU particle semantics and GPU runtime.

Implemented surface:

- deterministic `ParticleProgram` compiled from the validated canonical `ParticleEffectAsset`,
- semantic program fingerprint independent of runtime seed/emitter identity and human backend selection,
- static feature and spawn/update/render attribute analysis,
- stable required keyed-random-channel analysis,
- exact CPU-reference semantic operation counts,
- direct reuse of the existing 92-byte reference SoA memory accounting,
- explicit workload accumulation that survives lifecycle loop resets,
- minimized planned GPU runtime fields/stride/buffer layout,
- deterministic GPU artifact/layout fingerprint for explicitly GPU-selected effects,
- `trace2d_particle_analyze` machine-readable structural analysis,
- optional environment-labelled local timing,
- explicit `backend = "cpu" | "gpu"` ownership with no analyzer mutation or silent fallback.

Primary #51 contract: [`docs/PARTICLE_ANALYSIS.md`](docs/PARTICLE_ANALYSIS.md).

Performance/ownership rules remain:

- `ParticleReferenceEmitter::Step()` and `ParticleEmitter2D::Step()` do not compile, hash, stringify, time, touch filesystems or build reports,
- CPU analysis reuses the semantic reference backend,
- deterministic structural metrics and wall-clock timing remain separate,
- hosted CI does not treat arbitrary machine timing as portable truth,
- GPU-selected effects did not silently fall back before #52.

## Immediate #52 entry gate

After PR #94 merges green, #52 owns the actual GPU execution backend for effects explicitly authored with `backend = "gpu"`.

#52 must consume the deterministic program/layout contract from #51 and preserve:

- CPU reference remains the semantic oracle,
- normal GPU mode does not run a duplicate full CPU simulation,
- unsupported GPU features fail explicitly,
- persistent/capacity-reused GPU resources,
- no normal-frame readback/fence wait for inspection,
- no shader compilation/filesystem/report work in normal particle stepping,
- no per-particle draw-call path.

#53 then closes CPU/GPU conformance, representative workloads, safe budgets/build flow and human/LLM guidance before Sprite begins.

## Production architecture freeze (#85 / PR #94)

PR #94 freezes contracts now but deliberately leaves implementation to the correct later stage.

The mandatory seams are:

1. external user/game-defined typed components participate in the same Scene model as engine components without requiring generic reflection/ECS,
2. fixed-step authoritative transforms/state are separate from interactive interpolated presentation,
3. canonical typed resources use project-relative identity and generation-safe resolved handles with explicit lifetime/memory evidence,
4. reusable authored scene templates instantiate/despawn/load/unload deterministically,
5. Camera2D/Viewport2D provide backend-independent coordinate/presentation semantics,
6. Sprite batching/rendering reserves a material-ready resolved pipeline identity for later Material2D/Shader2D,
7. deterministic tween property targets are resolved during setup rather than string-looked-up each frame,
8. unified profiling separates structural metrics from CPU/GPU machine timing,
9. real-GPU conformance is tiered and does not claim universal bit-identical output,
10. lighting/navigation/platform expansion/networking/hot reload are recorded but remain explicit later gates.

The detailed authority, lifecycle, performance and QA rules live in `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` and take precedence over older prose that treats these areas as unspecified.

## Sprite phase #59

After #53, follow `docs/SPRITES.md` plus `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`.

Existing fixed Sprite program remains:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Additional frozen requirements from #85:

- `SpriteRenderer2D` / `SpriteAnimator2D` semantics must be attachable as finite typed #71 components rather than owning a parallel entity model,
- moving-sprite presentation must implement fixed-step previous/current interpolation while exact-frame capture renders authoritative current state unless an explicit sub-frame alpha is requested,
- renderer/batch contracts must reserve a resolved built-in/custom material identity so #89 does not replace the architecture,
- view input must be compatible with future #88 Camera2D/Viewport2D rather than permanently assuming one renderer-owned camera,
- Sprite authored identity remains CPU/project-relative and separate from GPU handles for #86 compatibility.

## Expanded game-production sequence

The original #69-#79 foundation remains valid but is now interleaved with concrete missing production children.

### #69 — Game/Application boundary

External game C++ lives outside `engine/`; headless/windowed compose the same game logic; SDL/MCP/backend types do not leak into gameplay APIs.

### #70 — Project/build/package

Versioned project identity, external CMake consumer flow, build/run/test/package contracts and distribution-facing shader/package policy.

### #71 — Scene hierarchy + engine/game components

In addition to hierarchy/local-world transforms and engine components, #71 must prove an **external user-defined authored gameplay component** with stable component type ID/schema version, explicit parse/validate/serialize/inspect adapter, strongly typed game access and Agent assertion. Runtime-only game components are allowed. Generic reflection/property bags remain unnecessary.

### #86 — Unified resource lifecycle

Typed project-relative identities, generation-safe runtime handles, CPU/GPU ownership separation, dependency/unload rules, cache reuse and memory accounting.

### #87 — Scene templates/world lifecycle

Reusable text-authored hierarchies, stable template-local/instance identity, deterministic instantiate/despawn safe points, explicit world/scene load/unload and no mandatory hidden pooling.

### #88 — Camera2D/Viewport2D

World component camera state, logical viewport/presentation mapping, deterministic active-camera rules, backend-independent world/screen conversion and integration with fixed-step presentation interpolation.

### #72-#75 — Input/Tile/Text/UI

Existing contracts remain, now built on the resolved world/resource/camera foundations.

### #89 — Material2D/Shader2D

Small programmable 2D fragment-shader/material surface with setup-time parameter binding, cached pipelines/resources and painter-order-preserving batch compatibility. No material graph/render graph.

### #90 — Deterministic tween/property animation

Fixed-step timeline and explicit resolved property bindings; no per-frame property strings/generic reflection.

### #76-#77 — Physics2D/Audio

Existing semantic/headless contracts remain.

### #91 — Unified profiler/diagnostics

One bounded machine-readable profile surface covering deterministic structural metrics, CPU timing, optional GPU timing and known resource memory evidence.

### #78 — Linux/compiler hardening

Windows/MSVC remains supported; add a continuously validated non-MSVC path and targeted sanitizers/static analysis where reliable.

### #92 — Real-GPU conformance

Tier A hosted math/shader validation, Tier B maintained real-GPU baseline before stable production-oriented claims, and Tier C release/vendor matrix only for claims actually covered.

### #79 — Persistence/migration

Versioned save state and authored schema migration close the external-user lifecycle before the flagship game proof.

## Flagship proof #12

The external sample game must be an ordinary external Trace2D consumer and should now prove a coherent subset of:

- external game/application module,
- at least one user-defined typed gameplay component,
- project/build/package flow,
- hierarchy and reusable template instancing,
- shared resource lifecycle/memory inspection,
- Camera2D/Viewport2D,
- Input Actions,
- TileMap,
- production UTF-8 text/UI,
- at least one Material2D effect,
- deterministic tween,
- Physics2D,
- semantic audio,
- profiler output,
- Windows plus the non-MSVC platform/toolchain,
- applicable GPU conformance evidence,
- persistence/migration,
- headless semantic QA and exact-frame capture.

No sample-only hidden shortcut may substitute for public/external contracts.

## Later fixed gates

After the flagship proof, #60 adds generic Mesh2D (`M0 -> M1`). #61 then stops at SP0 unless explicit owner approval records the then-current Spine licensing/integration/distribution model.

Issue #93 records later genuine gaps but is not routine core work until explicitly promoted.

## Open-source contribution lanes

The fixed sequence governs core continuation. Independent community contributions may still be reviewed when narrow fixes/tests/docs/portability improvements do not overlap active implementation, preempt future architecture, add unresolved dependency/license obligations or violate hard determinism/performance/ownership contracts.
