# Trace2D Project Status

Last repository-state update: **2026-08-12**

This file is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/CI/merge state, explicit owner-approved contracts, and exact active issue acceptance outrank stale prose.

## Current state

Trace2D is an **AI-first / AI-operated C++20 2D engine** with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Completed AI-operated foundation:

- #97 machine-readable intent / Definition of Done — PR #115,
- #98 unified verification / diagnosis / repair / WorkResult — PR #116,
- #99 Workspace / human feedback loop — PR #117,
- #102 Benchmark B0 — PR #118 / squash `13a28d7baf8bd72d9f3233a57b2a048450825bee`.

Completed Sprite foundation/renderer stages:

- #119 / S0 — PR #120,
- #121 / S1 — PR #122,
- #123 / SR0 — PR #124,
- #125 / SR1 — PR #126,
- #127 / SR2 — PR #128,
- #130 / SR3 — PR #131,
- #132 / SR4 — PR #133,
- #134 / SR5 — PR #135,
- #136 / SR6 — PR #137,
- #138 / SR7 — PR #139,
- #142 / SR8 — PR #143 / squash `2108122dad5ac2dcbb964f7ada0e80f7afa21003`.

Completed Sprite animation stages:

- #144 / SA0 deterministic timing/frame/event contract — PR #145 / squash `d9955d4c987a627f0009a018b9b5293c6f3d8e73`,
- #146 / SA1 `SpriteAnimator2D` authoritative state — PR #147 / squash `dc8909b24dc5f67e8bec2506263d7b433c6fb2f4`,
- #148 / SA2 deterministic playback/events/loops/transitions — PR #149 / squash `7f530dc8001a49aeddc7b0d98aa9dbeb781b7c66`,
- #150 / SA3 Agent/MCP exact verification — PR #151 / squash `d7a509ca03f851436d495183503f798c8afb8c2a`,
- #152 / SA4 animation conformance/deterministic workloads — PR #153 / squash `c5952c0e905c46816b0a182b7d91143bf54b188b`.

Trusted owner real-GPU automation from #140/#141 remains the presentation-GPU gate. SR8 final real-GPU evidence remains accepted. SA0-SA4 and the active SPP0 stage add no presentation/GPU behavior and therefore do not create a new real-GPU acceptance gate.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #154 / draft PR #155 — SPP0 deterministic offline processing / QA report.**  
**Active branch: `agent/sprite-spp0-processing-qa`.**  
**Exact next child after SPP0 merges green: SPP1 — deterministic alpha/background/frame extraction.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SPP0 contract: [`docs/SPRITE_PROCESSING_QA_SPP0.md`](docs/SPRITE_PROCESSING_QA_SPP0.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [complete] -> SA2 [complete] -> SA3 [complete]
 -> SA4 [complete]
 -> SPP0 [active #154/#155]
 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #154 / SPP0 — deterministic offline processing / QA report — active via draft PR #155

SPP0 establishes the deterministic evidence surface used by later Sprite extraction, repair, import and generation stages.

Authority:

```text
decoded RGBA8 pixels + explicit frame/page metadata
 -> deterministic raw measurements
 -> typed rule-based findings
 -> later SPP1/SPP2 processing decisions
 -> canonical SpriteAsset only after explicit validation
```

The report is derived evidence. It does not mutate source pixels, infer missing frames, auto-repair content or become runtime/render/animation truth.

### Implemented scope in draft PR #155

The protocol-independent in-memory analyzer measures:

- dimensions and exact pixel counts,
- transparent/partial/opaque alpha counts,
- visible-alpha bounds using `alpha > 0`,
- empty frames and visible edge contact,
- transparent pixels carrying non-zero RGB residue,
- unique RGBA and visible-RGB color counts,
- explicit pivot and dimension histograms,
- exact byte-identical frame groups,
- adjacent equal-dimension changed-pixel counts,
- visible-bounds origin displacement,
- explicit grid evidence without segmentation inference,
- atlas page area, packed-area utilization as exact integer numerator/denominator, out-of-bounds rectangles and overlap pairs.

Findings are typed and kept separate from raw facts. Bounds-displacement and low-utilization findings require explicit thresholds rather than hidden defaults.

`trace2d_sprite_process` is an explicit offline CLI adapter over the existing `TextureAssetCache`; the core analyzer itself requires no filesystem, renderer or GPU.

### Determinism / performance boundary

- frame measurement scans each frame in `O(pixel_count)`,
- exact duplicate identity confirms full dimensions + RGBA8 equality; no probabilistic hash is authoritative,
- adjacent diff is `O(pixel_count)` per adjacent comparable pair,
- atlas overlap is deliberately explicit offline `O(rect_count^2)`,
- JSON is schema-versioned explicit tool output,
- identical input/options must emit field/byte-identical JSON,
- source pixel buffers are viewed rather than copied merely for analysis,
- no SPP0 work enters `SpriteAnimator2D::Advance`, render extraction, normal frame submission or gameplay fixed-step execution.

### External-reference decisions — 2026-08-12

- W3C PNG Specification Third Edition — **ADOPT/ADAPT** alpha-zero / positive-alpha semantics over decoded RGBA8,
- Godot `Image` current stable docs — **ADAPT** non-zero-alpha used-rectangle precedent; **REJECT** Godot runtime/resource architecture,
- Aseprite official file format/docs — **ADOPT/ADAPT** explicit dimensions/frame/palette/grid metadata; importer remains deferred to SPP3,
- Aseprite sprite-sheet docs — **ADAPT** explicit frame size/offset/padding/order precedent; **REJECT** silent SPP0 segmentation inference.

No new runtime dependency is introduced.

### Current validation state

The implementation container cannot resolve `github.com`, so a full local checkout/integration build is unavailable. Draft PR #155 is published and GitHub Actions on the exact branch head are the integration compile/test authority.

Required before readiness:

1. focused `SpriteProcessingTests` compile and pass,
2. `trace2d_sprite_process --help` CTest passes,
3. normal Windows MSVC configure/build/full CTest passes,
4. clean-clone README configure/build/full CTest passes,
5. repository/content/release/benchmark/contract workflows remain green,
6. `docs/SPRITE_PROCESSING_QA_SPP0.md`, `docs/SPRITES.md`, this file, #154 and implementation agree,
7. no Sprite runtime/animation/render hot-path processing/report work is introduced.

No new local real-GPU gate is required because SPP0 changes no presentation/GPU path.

### SPP0 completion gate

PR #155 must remain draft/unmerged until one final exact head satisfies all acceptance items above. After all gates pass, record exact-head validation evidence, mark PR #155 ready, merge it, confirm #154 closes, and stop. **Do not create or implement SPP1 in that same completion continuation.**

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below.

```text
AI-operated foundation
 -> #97 WorkSpec                                            [complete]
 -> #98 WorkResult verify/diagnose/repair                   [complete]
 -> #99 Workspace/review loop                               [complete]
 -> #102 Benchmark B0                                       [complete]

Content production
 -> #59 complete Sprite program                             [active]
      -> S0/S1/SR0..SR8                                    [complete]
      -> SA0..SA4                                           [complete]
      -> #154 SPP0                                          [active via draft #155]
      -> SPP1..SPP5 offline processing/generation
      -> SE2E -> SPERF
 -> #103 Benchmark B1 Sprite/animation/particle matched tasks

External game-production foundation
 -> #69 -> #70 -> #71 -> #86 -> #87 -> #88 -> #72 -> #73 -> #74 -> #75
 -> #104 -> #89 -> #90 -> #76 -> #77 -> #91 -> #78 -> #92 -> #79

Proof / later geometry and compatibility
 -> #12 flagship external game
 -> #60 generic Mesh2D foundation
 -> #61 Spine SP0 human license gate
```

Umbrellas/registers #13/#96/#100/#67/#85/#93/#101/#106 do not authorize bypassing this fixed order.

## Durable authority boundaries

WorkSpec/WorkResult/Workspace continue to enforce deterministic verification before perceptual review. Agent self-report is never independent truth.

Sprite authority remains:

```text
external/generation input
 -> deterministic offline processing/import evidence
 -> canonical authored Sprite metadata
 -> authoritative typed runtime/animation state
 -> explicit Agent inspection / deterministic workloads
 -> resolved presentation
 -> backend renderer resources
```

SPP0 reports, Agent/MCP snapshots, workload hashes, timing samples, GPU resources, pixels and review artifacts never become canonical Sprite/gameplay truth.

The accepted B0 cohort/raw evidence remains under `benchmarks/b0/`; B0 proves the matched methodology/evidence loop, not broad engine superiority.

## Continuation rule

SPP0 / #154 / draft PR #155 is the only active Sprite child. The current continuation must:

1. keep PR #155 scoped to deterministic offline Sprite measurement/reporting and its CLI/test/docs surface,
2. keep raw measurements separate from rule findings and later perceptual judgment,
3. repair only SPP0 implementation/test/documentation issues exposed by review or CI,
4. require normal hosted CI/audits on the final PR head,
5. keep #155 draft/unmerged until those gates are green,
6. require no new real-GPU gate because SPP0 introduces no presentation-GPU behavior,
7. after all gates pass, record exact validation evidence, mark ready/merge #155, confirm #154 closes, and stop,
8. not create or implement SPP1 in that completion continuation.
