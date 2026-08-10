# Trace2D Project Status

Last repository-state update: **2026-08-10**

This is the operational handoff for the next contributor or coding agent. Live compiling code/tests, current PR/merge/CI state, and explicit owner decisions win over stale prose.

## Current state

**Public Alpha `v0.1.0-alpha.1` is released. Particle children #47, #48, and #49 are merged. PR #82 merged the open-source game-production roadmap/governance synchronization. Particle child #50 is implemented by PR #83 and is the active core work item while that PR remains open. After #83 merges green, #50 is complete and #51 becomes the exact next core implementation task.**

Do not start #51 or any later core feature while PR #83 remains open. Live PR/CI state wins over this post-merge handoff text.

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
9. **#50** complete Agent verification over CPU particle reference state — implemented by PR #83; considered complete after that PR merges green

Current and future core sequence after #83 merges:

10. **#51 — CPU particle cost analysis + explicit human backend choice + deterministic particle compiler — EXACT NEXT CORE TASK AFTER #83**
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

The owner does **not** need to re-select Physics vs Sprite vs hot reload, or decide whether game/project/tile/text/audio foundations should precede Mesh2D/Spine. Those choices are fixed by #13/#67.

## #50 completion contract / PR #83

Issue #50 is particle child 4 of 7. PR #83 makes the already-complete CPU reference/runtime state fully usable by coding agents before any backend compiler/GPU work begins.

The completed surface is:

- aggregate emitter/effect observation without a default particle array,
- explicit bounded particle detail with `offset + limit`, capped at 1024 entries per request,
- stable single-particle lookup by spawn ordinal,
- every authoritative V1 CPU-reference particle field exposed,
- composable typed aggregate/per-particle assertions,
- exact expected/observed values and bounded frame/seed/entity/emitter/effect failure context,
- explicit deterministic CPU-reference state fingerprint,
- stable structured error vocabulary,
- headless operation with no renderer/GPU requirement.

Performance and ownership rules remain unchanged:

- `ParticleReferenceEmitter::Step()` and `ParticleEmitter2D::Step()` do not hash, stringify, build snapshots, or allocate Agent detail,
- aggregate counters remain cheap scalar state,
- aggregate fingerprint work is explicit O(alive) QA work and does not materialize a particle vector,
- bounded detail allocation scales with the returned bounded page rather than total alive count,
- stable ordinal lookup keeps the existing direct O(N) scan instead of adding an unmeasured index/cache,
- no reflection/property-bag framework was introduced.

Primary #50 contract:

- [`docs/PARTICLE_AGENT_VERIFICATION.md`](docs/PARTICLE_AGENT_VERIFICATION.md)

Preserved particle contracts:

- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PARTICLE_DETERMINISM.md`](docs/PARTICLE_DETERMINISM.md)
- [`docs/PARTICLE_REFERENCE.md`](docs/PARTICLE_REFERENCE.md)
- [`docs/PARTICLE_EFFECTS.md`](docs/PARTICLE_EFFECTS.md)

While PR #83 is open, its CI is authoritative and #50 remains the active core item. After it merges green, this section is the final #50 handoff and #51 is next.

## Particle phase invariant through #53

The intended flow remains:

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

## Immediate #51 entry gate after PR #83

#51 may begin only after PR #83 is merged green.

It owns:

- deterministic structural CPU particle cost analysis,
- transparent reference-state memory/operation evidence,
- optional local Release timing kept separate from portable structural metrics,
- explicit human-controlled `backend = "cpu" | "gpu"` decision flow,
- deterministic particle-program/compiler analysis needed by the later GPU runtime,
- clear unsupported-GPU diagnostics rather than silent fallback.

#51 must consume the same authored semantics and CPU oracle established by #47-#50. It must not move analysis, hashing, compilation, filesystem work, allocation, or backend choice into ordinary particle stepping.

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

## Flagship proof #12

After #67, build one deliberately small real game as an **ordinary external Trace2D consumer**, not an engine test fixture or sample-only code path.

Only after #12 merges green does the core sequence advance to #60.

## Mesh2D #60

#60 is blocked by #59, #67 and #12.

Fixed internal order:

```text
M0 TexturedMesh2D contract/render path
 -> M1 persistent/capacity-reused dynamic geometry + conformance/workloads
```

## Spine #61

#61 is blocked by #59, #67, #12 and #60.

At SP0, stop unless explicit owner approval records the then-current allowed public MIT core / optional integration / source / binary / CI / notice / downstream-user licensing model.

Before SP0 approval:

- no `spine-cpp` vendoring/copy,
- no package/submodule/download dependency,
- no Spine-containing binary,
- no Spine-derived implementation code,
- no claim that Trace2D ships Spine support.

## Open-source contribution lanes

The owner-fixed order above governs **core continuation**. Independent community contributions may still be reviewed when they are narrow fixes/tests/docs/portability improvements that do not overlap the active core implementation, preempt future subsystem architecture, add unresolved dependencies/licenses, or violate hard contracts.

See `AGENTS.md`.

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
- particle deterministic semantics, rich CPU reference, authored effect cache/`ParticleEmitter2D`, and complete structured Agent verification via #50/PR #83.

Future breadth must preserve the same explicit ownership, headless semantic verification, bounded resource use, and measurement-driven optimization principles.
