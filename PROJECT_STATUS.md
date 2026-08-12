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

Completed Sprite offline-processing predecessor:

- #154 / SPP0 deterministic offline processing / QA report — PR #155 / squash `54d13db3c0547311afdbab25854212edc8226116`.

Trusted owner real-GPU automation from #140/#141 remains the presentation-GPU gate. SR8 final real-GPU evidence remains accepted. SA0-SA4, SPP0 and active SPP1 add no presentation/GPU behavior and therefore do not create a new real-GPU acceptance gate.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #156 / draft PR #157 — SPP1 deterministic alpha/background/frame extraction.**  
**Active branch: `agent/sprite-spp1-extraction`.**  
**Exact next child after SPP1 merges green: SPP2 — pixel-grid/palette/pivot/identity/motion QA and repair.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SPP0 contract: [`docs/SPRITE_PROCESSING_QA_SPP0.md`](docs/SPRITE_PROCESSING_QA_SPP0.md).  
Active SPP1 contract: [`docs/SPRITE_EXTRACTION_SPP1.md`](docs/SPRITE_EXTRACTION_SPP1.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [complete] -> SA2 [complete] -> SA3 [complete]
 -> SA4 [complete]
 -> SPP0 [complete]
 -> SPP1 [active #156/#157]
 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #156 / SPP1 — deterministic alpha/background/frame extraction — active via draft PR #157

SPP1 converts decoded RGBA8 source sheets into owned derived frame pixels only through explicit deterministic rules, then passes those outputs through the existing SPP0 analyzer.

Authority:

```text
decoded RGBA8 sheet + explicit extraction spec
 -> exact deterministic cleanup
 -> explicit rectangles / explicit uniform grid / alpha components
 -> exact expected-frame-count gate
 -> owned extracted RGBA8 frames + exact source rectangles
 -> existing SPP0 deterministic QA report
 -> later SPP2 repair/perceptual decisions
 -> canonical SpriteAsset only after later validation/import
```

The extraction result is offline derived content/evidence. It does not become runtime/render/animation truth.

### Implemented scope in draft PR #157

The protocol-independent in-memory extraction API adds:

- optional exact RGB background key -> transparent black,
- optional explicit alpha cutoff -> transparent black,
- optional zeroing of RGB for already-transparent pixels,
- ordered explicit rectangle extraction with stable unique IDs,
- explicit grid extraction with origin/cell/rows/columns/spacing and row-major/column-major order,
- deterministic 4-connected post-cleanup alpha-component extraction with row-major component seeding,
- mandatory `expectedFrameCount > 0` for every mode,
- hard expected-count mismatch failure with no partial authoritative output,
- optional non-zero-alpha trim preserving the final absolute source rectangle,
- structured trim-to-empty failure,
- owned output RGBA8 buffers plus schema-versioned deterministic structural JSON,
- direct reuse of `AnalyzeSpriteProcessing` so SPP0 remains the QA vocabulary.

SPP1 deliberately excludes fuzzy RGB matching, learned/VLM background removal, flood-fill tolerance and perceptual segmentation. Alpha components are geometry evidence only, not a semantic claim that every connected region is an authored frame.

### Determinism / performance boundary

- explicit/grid planning is `O(frame_count)`,
- alpha-component discovery is `O(source_pixel_count)`,
- trim scans only the planned source rectangle,
- output copy cost is proportional to extracted pixels,
- visitation storage is bounded to the source operation,
- extracted output capacity is reserved from the expected frame count where practical,
- no SPP1 work enters `SpriteAnimator2D::Advance`, render extraction, normal frame submission or gameplay fixed-step execution,
- no renderer/GPU readback, background worker framework, global extraction cache or new external dependency is introduced.

### External-reference decisions — 2026-08-12

- W3C PNG Specification Third Edition — **ADOPT/ADAPT** decoded alpha and exact transparent-color semantics; deliberately removed pixels normalize to transparent black,
- SDL3 `SDL_SetSurfaceColorKey` — **ADOPT/ADAPT** exact color-key transparency; **REJECT** SDL surface state as Trace2D processing authority,
- Aseprite CLI / sprite-sheet export — **ADAPT** explicit trim/crop/rows/columns/order/padding controls; **REJECT** hidden layout inference,
- Godot `Image` stable API — **ADAPT** explicit region copying and non-zero-alpha used-rectangle semantics; **REJECT** Godot resource/runtime ownership.

No new runtime dependency is introduced.

### Current validation state

Draft PR #157 was published from branch `agent/sprite-spp1-extraction`.

Before publication, the new extraction source was standalone C++20 syntax-compiled with `-Wall -Wextra -Werror` against the existing public SPP0 API shape. Focused GTest coverage is committed for exact cleanup, explicit rectangles, grid order, 4-connected component behavior, expected-count failure, trim behavior, SPP0 QA reuse and deterministic serialization.

Full repository integration remains owned by GitHub Actions on the final exact PR head. Required before readiness:

1. focused `SpriteExtractionTests` compile and pass,
2. normal Windows MSVC configure/build/full CTest passes,
3. clean-clone README configure/build/full CTest passes,
4. repository/content/release/benchmark/contract workflows remain green,
5. `docs/SPRITE_EXTRACTION_SPP1.md`, `docs/SPRITES.md`, this file, #156 and implementation agree,
6. no Sprite runtime/animation/render hot-path extraction/report work is introduced.

No new local real-GPU gate is required because SPP1 changes no presentation/GPU path.

### SPP1 completion gate

PR #157 must remain draft/unmerged until one final exact head satisfies all acceptance items above. After all gates pass, record exact-head validation evidence, mark PR #157 ready, merge it, confirm #156 closes, and stop. **Do not create or implement SPP2 in that same completion continuation.**

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
      -> #154 SPP0                                          [complete]
      -> #156 SPP1                                          [active via draft #157]
      -> SPP2..SPP5 offline processing/generation
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
 -> deterministic offline processing/extraction/import evidence
 -> canonical authored Sprite metadata
 -> authoritative typed runtime/animation state
 -> explicit Agent inspection / deterministic workloads
 -> resolved presentation
 -> backend renderer resources
```

SPP0 reports, SPP1 extraction outputs/manifests, Agent/MCP snapshots, workload hashes, timing samples, GPU resources, pixels and review artifacts never become canonical Sprite/gameplay truth by themselves.

The accepted B0 cohort/raw evidence remains under `benchmarks/b0/`; B0 proves the matched methodology/evidence loop, not broad engine superiority.

## Continuation rule

SPP1 / #156 / draft PR #157 is the only active Sprite child. The current continuation must:

1. keep PR #157 scoped to deterministic offline Sprite cleanup/extraction, expected-count gates, exact source geometry, SPP0 QA reuse and its tests/docs surface,
2. keep deterministic cleanup/extraction facts separate from later heuristic/perceptual repair judgment,
3. repair only SPP1 implementation/test/documentation issues exposed by review or CI,
4. require normal hosted CI/audits on the final PR head,
5. keep #157 draft/unmerged until those gates are green,
6. require no new real-GPU gate because SPP1 introduces no presentation-GPU behavior,
7. after all gates pass, record exact validation evidence, mark ready/merge #157, confirm #156 closes, and stop,
8. not create or implement SPP2 in that completion continuation.
