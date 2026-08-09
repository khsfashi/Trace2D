# Trace2D Project Status

Last repository-state update: **2026-08-09**

This document is the operational handoff for the next contributor or coding agent. Live repository code, active PR state, and CI results win over stale prose.

## Current phase

**Public Alpha released. #39 MCP transport is complete via merged PR #58. Owner-directed roadmap/agent-protocol PR #62 is currently open and must be finished/merged before starting #41. After PR #62 merges, #41 reproducible renderer performance workloads is the active next implementation task.**

`v0.1.0-alpha.1` was published on 2026-08-08. The repository is Public under the MIT License. Post-alpha work extends the proven agent-first loop rather than replacing it.

## Next execution order — owner-fixed

The repository owner originally fixed the P6 sequence on **2026-08-08** and explicitly extended the post-particle direction on **2026-08-09**. Future coding agents must follow this order unless the repository owner explicitly changes it.

1. **#40 — deterministic texture asset cache/import slice** — complete via PR #45
2. **#42 — text rendering and basic UI primitives** — complete via PR #55
3. **#43 — semantic UI tree and agent interaction** — complete via PR #56
4. **#39 — MCP transport over the completed protocol-independent agent/UI facade** — complete via PR #58
5. **#41 — reproducible renderer performance workloads** — **active next implementation after PR #62 merges**
6. **#47 — particle deterministic frame/keyed-random contracts**
7. **#48 — rich deterministic CPU particle reference simulation**
8. **#49 — text-authored particle effect assets + `ParticleEmitter2D`**
9. **#50 — complete Agent verification over CPU particle reference state**
10. **#51 — CPU particle cost analysis + explicit human backend choice + deterministic particle compiler**
11. **#52 — GPU runtime backend for explicitly GPU-selected effects**
12. **#53 — CPU/GPU conformance, workloads, safe budgets, build flow, and human/LLM guidance**
13. **#59 — Sprite pipeline: production rendering, deterministic animation, generation/import, processing QA, end-to-end motion QA and performance**
14. **#60 — Mesh2D foundation: reusable textured indexed geometry and measured dynamic submission path**
15. **#61 — Spine compatibility: SP0 explicit human license gate, then optional integration only if approved**

Particle umbrella: **#46**. Detailed particle contract: [`docs/PARTICLES.md`](docs/PARTICLES.md).

Sprite umbrella: **#59**. Detailed owner-approved contract: [`docs/SPRITES.md`](docs/SPRITES.md).

Mesh2D umbrella: **#60**. Its fixed M0/M1 boundary is recorded in #60 and the Sprite handoff contract.

Spine compatibility: **#61**. License/integration gate: [`docs/SPINE.md`](docs/SPINE.md).

### Owner decision replacing the old post-#53 choice

Older Issue #13 / roadmap prose said that after #53 the owner would choose one of physics/Box2D, sprite animation, or safe hot reload. That choice has now been made and must not be re-opened by a future coding agent:

```text
#53
 -> #59 complete end-to-end Sprite program
 -> #60 generic Mesh2D foundation
 -> #61 Spine SP0 human license gate
```

Physics/Box2D and safe hot reload remain valid future breadth candidates, but they are **not** the owner-fixed next task after #53 and must not displace #59/#60/#61 without a later explicit owner change.

## Continuation execution rule

The detailed short-command algorithm lives in `AGENTS.md`. Operationally:

- Work only on the first incomplete and unblocked item in the owner-fixed order.
- If that work has an active PR, finish/repair/validate that PR before starting anything later.
- **PR #62 is the active owner-directed roadmap/governance PR now. Do not start #41 until #62 is merged.**
- After #62 merges, #41 is the first incomplete implementation task.
- Merge only with green CI/repository gates; if merge is an owner action, report that single action instead of jumping ahead.
- Do **not** start #47 particles before #41 renderer workloads are complete.
- Within particles, complete exactly one of #47 -> #48 -> #49 -> #50 -> #51 -> #52 -> #53 at a time.
- Within #59, follow the exact fixed stage order in `docs/SPRITES.md`, creating/implementing one child issue/PR at a time.
- After #59, complete #60 M0 then M1 one child/PR at a time.
- When #61 is reached, **stop at SP0 if no explicit owner license approval is recorded**. Do not vendor/add/fetch/distribute Spine Runtime code while the gate is blocked.
- MCP is transport, not the engine API.
- Structured semantic state beats pixel inference for gameplay/UI/particle/sprite animation assertions.
- Visual capture remains first-class QA evidence when pixels genuinely matter.

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

## Spine boundary

Spine is a planned compatibility backend/integration, not Trace2D's native animation architecture.

Before #61 SP0 owner approval:

- no `spine-cpp` vendoring/copy,
- no package/submodule/download dependency,
- no prebuilt Spine-containing binary,
- no Spine-derived implementation code,
- no claim that Trace2D ships Spine support.

If SP0 is approved after license confirmation, the fixed conceptual order becomes SP1 optional official runtime adapter -> SP2 Mesh2D render integration -> SP3 semantic animation state -> SP4 Agent/MCP QA/conformance/workloads. See `docs/SPINE.md`.

## Completed #40 texture asset slice

The P6 asset foundation provides:

- deterministic project-relative texture identity with separator normalization,
- rejection of absolute paths and `..` traversal,
- PNG/JPEG/BMP/TGA decode to immutable RGBA8 CPU assets,
- successful-import caching with explicit invalidation/clear,
- stable structured diagnostics and cache metrics,
- no per-frame filesystem discovery/decoding,
- SDL-free `engine/assets` with no renderer/GPU ownership.

See [`docs/ASSETS.md`](docs/ASSETS.md).

## Completed #42 text/basic UI slice

The first UI slice established:

- SDL-free `engine/ui`,
- strict versioned `*.trace2d.toml` UI authoring,
- stable element IDs and deterministic authored order,
- `panel`, `label`, `button`, and `text_input`,
- deterministic integer pixel bounds,
- engine-owned focus and button activation state,
- deterministic dependency-free 5x7 ASCII-oriented text,
- caller-owned/reused RGBA8 CPU raster storage,
- headless and windowed preview paths over the same `UiDocument`,
- no renderer-owned authoritative UI state.

## Completed #43 semantic UI automation — PR #56

PR #56 established semantic ID/role/name state, deterministic UI ordering, protocol-independent Agent inspection/query/focus/activation/text/assertion, and a headless semantic UI -> authoritative scene-state verification path.

Performance rules remain: ordinary UI raster/simulation does not build JSON/MCP snapshots, query/action results allocate only on explicit requests, and no speculative browser/DOM/coordinate-primary framework was introduced.

See [`docs/UI.md`](docs/UI.md).

## Completed #39 MCP transport — PR #58

PR #58 added a deliberately thin modern MCP `2026-07-28` stdio adapter over existing Agent/Testing semantics.

Boundary:

```text
Runtime / Scene / Input / UI
          |
          v
      AgentFacade
          |
          v
  GameplayScenario
          |
          v
      engine/mcp
          |
          v
   trace2d_mcp stdio host
```

MCP owns protocol parsing/serialization only. JSON/MCP types do not enter core/runtime/scene/input/UI/agent/testing public contracts. Protocol tests cover semantic scene/UI/input/step/assert flows headlessly.

See [`docs/MCP.md`](docs/MCP.md).

## Why the current order

#41 follows MCP to establish reproducible renderer workloads before another render-heavy subsystem. Particles then prove the CPU-reference -> structured QA -> measured cost -> explicit human backend choice -> GPU compiler/runtime/conformance workflow.

The owner then deliberately prioritizes Sprite because Trace2D's portfolio/product goal is stronger than ordinary animation playback: an agent should eventually be able to take source/generated pixels through deterministic import/repair/QA, run deterministic animation, inspect/assert it headlessly, render it with production sprite semantics, and complete motion/visual/performance QA.

Mesh2D follows Sprite so arbitrary textured indexed geometry does not bloat the traditional SpriteRenderer. Spine follows Mesh2D only as a separately licensed optional compatibility integration and stops first at SP0.

## Particle target after #41

Particle implementation is governed by #46 and `docs/PARTICLES.md`.

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

Hard rules remain:

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

New machine-facing capabilities require deterministic automated tests when practical. GPU presentation itself is not a hosted-runner requirement; backend-independent semantic/math/import/ordering/QA logic must remain headless-CI testable where practical.

Wall-clock timing is environment-dependent evidence and must not become a deterministic correctness threshold.

## Architecture invariants

- `engine/core` has no SDL dependency.
- SDL-specific ownership/types stay behind platform/render boundaries.
- `engine/assets` and `engine/ui` are SDL-free.
- renderer GPU state is presentation state, never authoritative gameplay/UI/animation state.
- runtime/scene/input/UI/agent/testing semantic state does not depend on MCP transport.
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