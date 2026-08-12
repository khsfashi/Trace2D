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
 -> SPP0 -> SPP1 -> SPP2
```

Current child: **#164 / SPP3 — deterministic Aseprite-sheet, generic sheet and loose-frame import conversion**.  
Current draft PR: **#165**.  
Current branch: `agent/sprite-spp3-importers`.  
Exact next child after SPP3 merges green: **SPP4 — sprite-gen / PerfectPixel-style manifest interoperability**.

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SPP3 contract: [`docs/SPRITE_IMPORT_SPP3.md`](docs/SPRITE_IMPORT_SPP3.md).  
Repository-state authority/rationale: [`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md).

## SPP3 authority

```text
external exported manifest + decoded RGBA8
 -> explicit deterministic SPP3 conversion
 -> existing S1 canonical SpriteAsset validation
 -> ordered import evidence
 -> later save/runtime preparation
```

SPP3 remains offline/setup work. It does not replace canonical S1 assets, SA0-SA4 animation state, runtime Sprite components, renderer state or GPU evidence.

Baseline scope in PR #165:

- Aseprite exported sprite-sheet JSON `array|hash` import,
- exact page/source/trim geometry,
- checked integer millisecond-to-nanosecond frame durations,
- ordered frame-tag authoring evidence,
- explicit `rotated=true` policy rather than direction inference,
- generic explicit-region and explicit-grid import,
- loose-frame import without forced repacking,
- existing S1 serializer/parser reused as the final canonical validation authority,
- deterministic schema-versioned structural JSON,
- transactional failure with no partial canonical output,
- focused backend-independent tests.

Native `.ase/.aseprite` binary compositing, hidden generic layout inference, atlas packing, background workers and runtime source-tool dispatch are outside SPP3.

## Performance boundary

- Aseprite import: `O(manifest bytes + frame count + tag count)`,
- generic/loose import: `O(frame count)`,
- decoded source pixel buffers are viewed for exact byte/dimension validation,
- no importer work enters fixed-step animation, normal render submission or GPU presentation,
- existing `nlohmann-json` is reused; no new package is introduced.

## Current validation gate

Initial PR head: `391aac108e27e44549b7c8c1e7496e4a925847a8`.

At publication of that head:

- Project State Contract: green,
- Sprite S0 Contract: green,
- Sprite SA0 Contract: green,
- repository/release/benchmark/content/Godot qualification jobs observed so far: green,
- Windows MSVC configure/build/full CTest: still required on the final exact head,
- clean-clone README configure/build/full CTest: still required on the final exact head.

No local full-build claim is made for this execution because its local checkout path could not resolve `github.com`. Hosted exact-head CI is the integration authority for this continuation.

No new real-GPU gate is required because SPP3 changes no presentation behavior.

## Continuation rule

Keep #165 draft until the final exact head has green hosted integration/audit evidence and stage documentation agrees with #164/implementation. Repair only SPP3 issues exposed by code review or CI. After #165 is ready and merged and #164 closes, stop. Do **not** start SPP4 in the same completion continuation.
