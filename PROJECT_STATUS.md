# Trace2D Project Status

Last explanatory handoff update: **2026-08-13**

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
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
```

Frozen milestone references retained for contract continuity:

- #144 / SA0 — deterministic Sprite animation timing/frame/event contract, frozen and complete.
- #142 / SR8 — production Sprite renderer conformance and trusted presentation-GPU evidence, frozen and complete.
- #152 / SA4 — deterministic Sprite animation conformance, frozen and complete.
- #168 / PR #169 / `c3bcac89ca8c7ca21a9130b1b16cf7ece9e31c1a` — SPP5 provider-neutral generation orchestration, frozen and complete.

Current child: **#170 / SE2E — end-to-end generated/imported Sprite proof**.  
Current branch: `agent/sprite-se2e-end-to-end-proof`.  
Exact next child after SE2E merges green: **SPERF — final reproducible Sprite performance evidence**.

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SE2E contract: [`docs/SPRITE_END_TO_END_SE2E.md`](docs/SPRITE_END_TO_END_SE2E.md).  
SPP5 frozen contract: [`docs/SPRITE_GENERATION_SPP5.md`](docs/SPRITE_GENERATION_SPP5.md).  
Repository-state authority/rationale: [`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md).

## SE2E authority

SE2E composes existing authorities rather than introducing another production layer:

```text
recorded request/provider response
 -> SPP deterministic cleanup / QA / canonical import
 -> canonical SpriteAsset
 -> SA deterministic animation
 -> SA3 headless exact-frame Agent inspection/assertion
 -> SR0 renderer-facing canonical region selection
 -> explicit capture artifact handoff
 -> #98 WorkResult deterministic evidence + review_needed presentation evidence
```

Deterministic verification remains separate from perceptual review. A test-authored capture fixture proves exact-frame artifact packaging only; it is not GPU presentation truth. SR8 remains the authority for real-GPU rendering/capture behavior, while multimodal or human review owns visual/motion judgment.

Baseline scope in #170:

- backend-independent `tests/e2e` integration only; no new production SE2E module,
- recorded/fake SPP5 provider data; no live/paid provider dependency,
- exact canonical asset pointer and region identity preserved into animation and render-facing extraction,
- authored frame-boundary advance observed through the existing SA3 Agent surface,
- existing SR0 O(1) index resolver/extraction used after animation selection,
- explicit capture artifact bound to the same logical simulation-frame id,
- #98 `WorkResult` remains the machine-readable result/review boundary,
- perceptual visual/motion quality remains `review_needed`,
- invalid generation plans expose no canonical/downstream authority.

## Performance boundary

SE2E changes test/tooling code only. It adds no normal-frame production cost: no provider calls, verification polling, filesystem work, capture/readback, reporting allocation, parsing, serialization, or new caches in gameplay/render loops. Existing SPP, SA, SR, and capture complexity/ownership remain authoritative.

## Current validation gate

Required on the final exact SE2E PR head:

- focused `trace2d_sprite_e2e_tests`,
- Project State Contract,
- Sprite S0 Contract,
- Sprite SA0 Contract,
- repository/release/benchmark/content/Godot qualification jobs,
- Windows MSVC configure/build/full CTest,
- clean-clone README configure/build/full CTest.

No new real-GPU gate is introduced by SE2E. SR8 already owns trusted presentation-GPU conformance; SE2E must not create a competing GPU truth model.

## Continuation rule

Keep the SE2E PR draft until its exact head has green hosted integration/audit evidence and stage documentation agrees with #170. Repair only SE2E issues exposed by review or CI. After the SE2E PR is ready, merged, and #170 closes, stop. The following `@GitHub Trace2D 다음 진행해줘` continuation creates exactly one SPERF child.
