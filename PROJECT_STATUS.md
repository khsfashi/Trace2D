# Trace2D Project Status

Last repository-state update: **2026-08-10**

This is the operational handoff for the next contributor or coding agent. Live compiling code/tests, current PR/merge/CI state, and explicit owner decisions win over stale prose.

## Current state

**Public Alpha `v0.1.0-alpha.1` is released. Particle children #47, #48, and #49 are complete. PR #66 merged #49 text-authored particle effects + `ParticleEmitter2D`. The exact next core implementation task is #50: complete Agent verification over the CPU particle reference state.**

The current roadmap/governance synchronization is tracked by **#68**. It inserts the open-source game-production program #67 before Mesh2D/Spine and makes the future order executable through `@GitHub Trace2D 다음 진행해줘`.

Do not start #51 or any later feature while #50 is incomplete. If a roadmap synchronization PR for #68 is still open, finish/merge that governance work first, then begin #50.

## Exact owner-fixed core execution order

Completed predecessors:

1. **#40** deterministic texture asset cache/import — complete via PR #45
2. **#42** text rendering/basic UI — complete via PR #55
3. **#43** semantic UI tree/Agent interaction — complete via PR #56
4. **#39** MCP transport over Agent/Testing — complete via PR #58
5. **#41** reproducible renderer workloads — complete via PR #63
6. **#47** particle deterministic frame/keyed-random contracts — complete via PR #64
7. **#48** rich deterministic CPU particle reference — complete via PR #65
8. **#49** text-authored particle effects + `ParticleEmitter2D` — complete via PR #66

Current and future core sequence:

9. **#50 — complete Agent verification over CPU particle reference state — EXACT NEXT CORE TASK**
10. **#51 — CPU particle cost analysis + explicit human backend choice + deterministic particle compiler**
11. **#52 — GPU runtime backend for explicitly GPU-selected particle effects**
12. **#53 — CPU/GPU conformance, workloads, safe budgets, build flow and human/LLM guidance**
13. **#59 — complete Sprite program: canonical assets, production renderer, deterministic animation, processing/generation QA, end-to-end motion QA and performance**
14. **#67 — open-source game-production foundation**, fixed children:
    - **#69 E0** Game/Application module boundary
    - **#70 E1** project manifest + external consumer build/install/package
    - **#71 E2** scene hierarchy + typed authored component composition
    - **#72 E3** Input Actions + gamepad/mouse/text/IME
    - **#73 E4** TileSet/TileMap
    - **#74 E5** production UTF-8 font/text/localization foundation
    - **#75 E6** practical deterministic UI hierarchy/layout/widgets
    - **#76 E7** Physics2D
    - **#77 E8** Audio
    - **#78 E9** Linux/compiler/toolchain hardening
    - **#79 E10** save/persistence + authored schema migration
15. **#12 — flagship external Trace2D sample game proof**
16. **#60 — generic Mesh2D foundation, M0 then M1**
17. **#61 — Spine compatibility, stop first at SP0 human license gate**

Core roadmap umbrella: **#13**.
Particle umbrella: **#46**.
Sprite umbrella: **#59**.
Open-source game-production umbrella: **#67**.
Community-lane policy tracker: **#80**.

## Continuation rule

For `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, `Trace2D continue`, or an equivalent routine continuation request:

1. read `AGENTS.md` and this file,
2. inspect live open PRs/CI and recent merges,
3. reconcile stale status if live state advanced,
4. finish the active core PR if one exists,
5. otherwise select the first incomplete/unblocked item above,
6. implement exactly one issue/child vertical slice,
7. test/validate/document it,
8. publish/update one scoped PR,
9. do not begin the next core child until the current one merges green.

The owner does **not** need to re-select Physics vs Sprite vs hot reload, or decide whether game/project/tile/text/audio foundations should precede Mesh2D/Spine. Those choices are now fixed by #13/#67.

## Immediate #50 handoff

Issue #50 is particle child 4 of 7.

It must make the already-complete CPU reference/runtime state fully usable by coding agents before any backend compiler/GPU work begins.

Required direction from #50:

- aggregate emitter/effect state without default full-particle dumps,
- explicit bounded particle detail by stable spawn ordinal and/or offset+limit,
- every supported authoritative V1 particle field inspectable,
- typed assertions with exact expected/observed/frame/seed/emitter/effect context,
- deterministic state fingerprint over canonical CPU reference state,
- stable ordering/error behavior,
- no renderer/GPU requirement,
- no JSON/string/snapshot/fingerprint work in ordinary particle stepping,
- aggregate scalar counters remain cheap,
- bounded detail allocation scales with requested detail rather than total alive count.

#50 must preserve #47-#49 semantics rather than reinterpret them.

Primary particle contracts:

- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PARTICLE_DETERMINISM.md`](docs/PARTICLE_DETERMINISM.md)
- [`docs/PARTICLE_REFERENCE.md`](docs/PARTICLE_REFERENCE.md)
- [`docs/PARTICLE_EFFECTS.md`](docs/PARTICLE_EFFECTS.md)
- Issue #50 acceptance criteria

## Particle phase invariant through #53

The intended flow is:

```text
text-authored effect
 -> deterministic CPU reference simulation
 -> complete Agent inspection/assertion (#50)
 -> deterministic structural CPU cost + optional local timing (#51)
 -> HUMAN backend decision cpu|gpu (#51)
 -> deterministic minimized GPU program for gpu-selected effects (#51/#52)
 -> GPU execution without simultaneous full CPU reference in normal mode (#52)
 -> CPU/GPU conformance + workloads + safe guidance (#53)
```

Hard rules:

- CPU remains the semantic reference/oracle.
- Backend choice is explicit and human-controlled; an LLM may recommend but never silently rewrite it.
- GPU-selected unsupported effects fail clearly rather than silently falling back.
- Normal GPU mode does not also run the full CPU reference simulation.
- Structural deterministic metrics and machine-dependent timing stay separate.
- No universal cross-vendor bit-identical GPU floating-point claim without proof.

## Sprite phase #59

After #53, do **not** re-open the old physics-vs-animation-vs-hot-reload fork. #59 is already selected.

Follow `docs/SPRITES.md` exactly:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

The Sprite target is intentionally broader than a minimal quad renderer. It includes canonical assets, production traditional SpriteRenderer semantics, deterministic animation/Agent QA, import/generation/repair interoperability, end-to-end visual/motion QA, and reproducible performance evidence.

## Open-source game-production phase #67

After #59, the exact next program is **#67**, not Mesh2D or Spine.

Detailed contract: [`docs/GAME_PRODUCTION.md`](docs/GAME_PRODUCTION.md).

Fixed child sequence:

```text
#69 Game/Application boundary
 -> #70 project manifest + external build/install/package
 -> #71 scene hierarchy + typed components
 -> #72 Input Actions + device/text input
 -> #73 TileSet/TileMap
 -> #74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI layout/widgets
 -> #76 Physics2D
 -> #77 Audio
 -> #78 Linux/compiler/toolchain hardening
 -> #79 persistence + authored schema migration
```

Why this precedes Mesh2D/Spine:

Trace2D must first answer the user-facing engine questions that are more fundamental to third-party adoption: where game code lives, how a project is consumed, how world/components are authored, how input/tile/text/UI/physics/audio work, how the engine is validated beyond one compiler, and how saves/formats survive version changes.

### Existing implementation hardening assigned to #67

- Texture handles currently leave tombstones and are not recycled. Add generation-safe reuse only if realistic resource churn demonstrates a real issue; do not rewrite speculatively.
- Renderer shadercross compilation happens during setup, not `RenderFrame`. #70 owns distributable offline/reproducible shader packaging if required.
- Current 5x7 ASCII-oriented text remains a deterministic fixture; #74 supplies production UTF-8/font/CJK capability.
- Alpha authored formats have no migration tooling; #79 closes that external-user lifecycle gap.

## Flagship proof #12

After #67, build one deliberately small real game as an **ordinary external Trace2D consumer**, not an engine test fixture or sample-only code path.

It should exercise a coherent subset of the public game/project/scene/input/tile/text/UI/physics/audio/persistence contracts, headless semantic QA, exact-frame capture, the documented consumer build, and the additional platform/toolchain from #78.

Only after #12 merges green does the core sequence advance to #60.

## Mesh2D #60

#60 is now blocked by #59, #67 and #12.

Fixed internal order:

```text
M0 TexturedMesh2D contract/render path
 -> M1 persistent/capacity-reused dynamic geometry + conformance/workloads
```

Mesh2D remains presentation state and must reuse the project/resource/distribution contracts already established by #67.

## Spine #61

#61 is blocked by #59, #67, #12 and #60.

At SP0, stop unless explicit owner approval records the then-current allowed public MIT core / optional integration / source / binary / CI / notice / downstream-user licensing model.

Before SP0 approval:

- no `spine-cpp` vendoring/copy,
- no package/submodule/download dependency,
- no Spine-containing binary,
- no Spine-derived implementation code,
- no claim that Trace2D ships Spine support.

If approved, continue SP1 -> SP2 -> SP3 -> SP4 as defined by #61 and `docs/SPINE.md`.

## Open-source contribution lanes

The owner-fixed order above governs **core continuation**.

Independent community contributions may still be reviewed when they are narrow fixes/tests/docs/portability/improvements that do not overlap the active core implementation, preempt a future subsystem architecture, add unresolved dependencies/licenses, or violate hard contracts.

See `AGENTS.md` and #80.

## Current implemented foundation snapshot

Already present and preserved by future work:

- C++20 / CMake / pinned vcpkg / Windows MSVC CI,
- deterministic fixed-step runtime and reset/seed ownership,
- text-authored TOML scene baseline with stable semantic entity identity,
- protocol-independent Agent inspect/query facade,
- deterministic physical/virtual input convergence,
- exact-frame gameplay scenario/assertions,
- SDL3 GPU renderer with order-preserving contiguous batching and persistent resources,
- explicit exact-frame capture,
- deterministic texture asset cache/import,
- engine-owned semantic UI + headless actions/assertions,
- MCP stdio adapter over Agent/Testing,
- reproducible renderer workloads,
- particle deterministic semantics, rich CPU reference, authored effect cache, and `ParticleEmitter2D`.

Future breadth must preserve the same explicit ownership, headless semantic verification, bounded resource use, and measurement-driven optimization principles.
