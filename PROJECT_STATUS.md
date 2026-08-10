# Trace2D Project Status

Last repository-state update: **2026-08-10**

This is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/merge/CI state, and explicit owner decisions win over stale prose.

## Current state

**Public Alpha `v0.1.0-alpha.1` is released. Particle children #47-#50 are complete. PR #83 merged #50 complete Agent verification. Particle child #51 is implemented by draft PR #84 and is the active core work item while that PR remains open. After #84 merges green, #51 is complete and #52 becomes the exact next core implementation task.**

Do not start #52 or any later core feature while PR #84 remains open. Live PR/CI state wins over this handoff text.

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
9. **#50** complete Agent verification over CPU particle reference state — complete via PR #83
10. **#51** CPU particle cost analysis + explicit human backend choice + deterministic particle compiler — implemented by PR #84; complete after that PR merges green

Current and future core sequence after #84 merges:

11. **#52 — GPU runtime backend for explicitly GPU-selected particle effects — EXACT NEXT CORE TASK AFTER #84**
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

For `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, `Trace2D continue`, or equivalent routine continuation:

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

## #51 completion contract / PR #84

Issue #51 is particle child 5 of 7. PR #84 introduces the explicit analysis/compiler gate after the complete CPU semantic/Agent oracle and before the GPU runtime.

Implemented surface:

- one deterministic `ParticleProgram` compiled from the already-validated canonical `ParticleEffectAsset`,
- deterministic semantic program fingerprint independent of runtime seed/emitter identity and human backend selection,
- static feature and spawn/update/render attribute analysis,
- stable required keyed-random-channel analysis,
- exact CPU-reference semantic operation counts,
- direct reuse of the existing 92-byte reference SoA memory accounting,
- explicit workload accumulation that survives lifecycle loop resets,
- minimized planned GPU runtime fields/stride/buffer layout,
- deterministic GPU artifact/layout fingerprint for explicitly GPU-selected effects,
- `trace2d_particle_analyze` machine-readable structural analysis,
- optional environment-labelled local timing path,
- explicit `backend = "cpu" | "gpu"` ownership with no analyzer mutation or silent fallback.

Primary #51 contract:

- [`docs/PARTICLE_ANALYSIS.md`](docs/PARTICLE_ANALYSIS.md)

Preserved particle contracts:

- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PARTICLE_DETERMINISM.md`](docs/PARTICLE_DETERMINISM.md)
- [`docs/PARTICLE_REFERENCE.md`](docs/PARTICLE_REFERENCE.md)
- [`docs/PARTICLE_EFFECTS.md`](docs/PARTICLE_EFFECTS.md)
- [`docs/PARTICLE_AGENT_VERIFICATION.md`](docs/PARTICLE_AGENT_VERIFICATION.md)

### Performance/ownership rules

- `ParticleReferenceEmitter::Step()` and `ParticleEmitter2D::Step()` are unchanged by #51.
- No compiler, hash, JSON, timing, filesystem, or report work runs in ordinary particle stepping.
- The CPU analyzer executes the existing reference backend rather than duplicating simulation semantics.
- Structural metrics and wall-clock timing remain separate.
- Timing is local evidence and is never a hosted-CI performance threshold.
- No automatic CPU/GPU score or recommendation threshold is introduced before #53 establishes representative measured budgets.
- A GPU-selected effect can compile a deterministic layout artifact, but normal runtime preparation still reports `BackendUnavailable` until #52. There is no CPU fallback.

While PR #84 is open, its CI/review state is authoritative and #51 remains the active core item. After it merges green, this section is the final #51 handoff and #52 is next.

## Particle phase invariant through #53

The intended flow remains:

```text
text-authored effect
 -> deterministic CPU reference simulation
 -> complete Agent inspection/assertion (#50)
 -> deterministic structural CPU cost + optional local timing (#51)
 -> HUMAN backend decision cpu|gpu (#51)
 -> deterministic minimized GPU program/artifact (#51)
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

## Immediate #52 entry gate after PR #84

#52 may begin only after PR #84 is merged green.

It owns the actual GPU execution backend for effects whose authored `backend` is explicitly `gpu`, consuming the deterministic `ParticleProgram`/GPU layout contract from #51.

#52 must preserve the CPU reference as the semantic oracle without running a simultaneous full CPU simulation in normal GPU mode. Unsupported features must remain explicit errors; resource allocation should be persistent/capacity-reused; no normal-frame readback, fence wait for inspection, shader compilation, filesystem work, or per-particle draw-call path may be introduced.

## Sprite phase #59

After #53, follow `docs/SPRITES.md` exactly:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

The Sprite target includes canonical assets, production traditional SpriteRenderer semantics, deterministic animation/Agent QA, import/generation/repair interoperability, end-to-end visual/motion QA, and reproducible performance evidence.

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

## Later fixed gates

After #67, complete #12 as an ordinary external Trace2D consumer proof before #60 Mesh2D. After #60 M0 -> M1, #61 Spine stops at SP0 unless explicit owner approval records the then-current licensing/integration/distribution model.

Before Spine SP0 approval there is no Spine runtime dependency, vendoring, package/submodule, binary, derived implementation code, or claim that Trace2D ships Spine support.

## Open-source contribution lanes

The owner-fixed order above governs **core continuation**. Independent community contributions may still be reviewed when they are narrow fixes/tests/docs/portability improvements that do not overlap the active core implementation, preempt future subsystem architecture, add unresolved dependencies/licenses, or violate hard contracts.

See `AGENTS.md`.
