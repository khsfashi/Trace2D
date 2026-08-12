# Trace2D Project Status

Last explanatory handoff update: **2026-08-12**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state through the repository-state tooling. Do not guess a merge or active child from this Markdown file when those sources disagree.

## Current program

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Active core program: **#59 Complete Sprite**.

Completed Sprite chain:

```text
S0 -> S1
 -> SR0..SR8
 -> SA0..SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3
```

Frozen milestone references retained for contract continuity:

- #144 / SA0 — deterministic Sprite animation timing/frame/event contract, frozen and complete.
- #164 / PR #165 / `926993ace6d020e00e3d4565d0ffacff866ee252` — SPP3 external sheet/import conversion, frozen and complete.

Current child: **#166 / SPP4 — deterministic sprite-gen / PerfectPixel manifest interoperability**.  
Current draft PR: **#167**.  
Current branch: `agent/sprite-spp4-generator-manifests`.  
Exact next child after SPP4 merges green: **SPP5 — provider-neutral generation orchestration**.

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SPP4 contract: [`docs/SPRITE_GENERATOR_INTEROP_SPP4.md`](docs/SPRITE_GENERATOR_INTEROP_SPP4.md).  
SPP3 frozen contract: [`docs/SPRITE_IMPORT_SPP3.md`](docs/SPRITE_IMPORT_SPP3.md).  
Repository-state authority/rationale: [`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md).

## SPP4 authority

```text
explicit provider manifest + decoded RGBA8 atlas
 -> explicit finite SPP4 adapter
 -> ordered generic SPP3 regions + animation evidence
 -> ImportGenericSpriteSheet
 -> existing S1 canonical validation
 -> deterministic SPP4 structural evidence
```

SPP4 remains offline/setup work. It does not replace canonical S1 assets, SA0-SA4 animation state, runtime Sprite components, renderer state, provider orchestration or GPU evidence.

Baseline scope in PR #167:

- explicit `SpriteGenComponentRow` and `PerfectPixelV2` adapter kinds; no weak-field auto-detection,
- sprite-gen runtime `game_input`, `degraded_static_fallback=false`, absolute `frame_layout`, explicit animation rows/FPS/loop/per-frame millisecond durations,
- repeated sprite-gen atlas rectangles preserved as distinct playback slots,
- mandatory explicit rational sprite-gen pivot rather than alpha/pixel inference,
- PerfectPixel `perfectpixel.sprite/2` sheet identity, full-cell rects, local trims, integer pivot and duration/FPS/loop metadata,
- exact PerfectPixel trim lowering into S1 packed-content/source/trim geometry,
- explicit row ordering independent of JSON map order,
- checked integer millisecond-to-nanosecond duration conversion,
- both formats lowered through existing SPP3 `ImportGenericSpriteSheet` and S1 canonicalization rather than a second SpriteAsset builder,
- deterministic schema-versioned structural evidence,
- transactional failure with no partial authoritative output,
- focused backend-independent tests.

Provider execution, curation/editor state, raw generation state, background workers, runtime source-format dispatch and automatic semantic/pivot inference are outside SPP4. Live provider-neutral generation belongs to SPP5.

## Performance boundary

- manifest parsing/planning: `O(manifest bytes + frame count)`,
- animation row ordering: `O(animation count log animation count)`,
- canonical lowering: existing SPP3 `O(frame count)` plus S1 metadata validation,
- decoded atlas bytes are viewed for exact byte/dimension validation rather than recopied merely for metadata adaptation,
- no SPP4 work enters fixed-step animation, normal render submission, GPU presentation or provider runtime paths,
- existing `nlohmann-json` is reused; no new package is introduced.

## Current validation gate

Required on the final exact PR #167 head:

- Project State Contract,
- Sprite S0 Contract,
- Sprite SA0 Contract,
- repository/release/benchmark/content/Godot qualification jobs,
- Windows MSVC configure/build/full CTest,
- clean-clone README configure/build/full CTest.

No local full-build claim is made for this execution because its local checkout path cannot resolve `github.com`. Hosted exact-head CI is the integration authority for this continuation.

No new real-GPU gate is required because SPP4 changes no presentation behavior.

## Continuation rule

Keep #167 draft until the final exact head has green hosted integration/audit evidence and stage documentation agrees with #166/implementation. Repair only SPP4 issues exposed by code review or CI. After #167 is ready and merged and #166 closes, stop. Do **not** start SPP5 in the same completion continuation.
