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
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4
```

Frozen milestone references retained for contract continuity:

- #144 / SA0 — deterministic Sprite animation timing/frame/event contract, frozen and complete.
- #164 / PR #165 / `926993ace6d020e00e3d4565d0ffacff866ee252` — SPP3 external sheet/import conversion, frozen and complete.
- #166 / PR #167 / `e195afb2a9dc7c80f49d71abff32c920e3e850c4` — SPP4 generator-manifest interoperability, frozen and complete.

Current child: **#168 / SPP5 — provider-neutral generation orchestration with deterministic post-generation validation**.  
Current draft PR: **#169**.  
Current branch: `agent/sprite-spp5-generation-orchestration`.  
Exact next child after SPP5 merges green: **SE2E — end-to-end generated/imported Sprite proof**.

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SPP5 contract: [`docs/SPRITE_GENERATION_SPP5.md`](docs/SPRITE_GENERATION_SPP5.md).  
SPP4 frozen contract: [`docs/SPRITE_GENERATOR_INTEROP_SPP4.md`](docs/SPRITE_GENERATOR_INTEROP_SPP4.md).  
Repository-state authority/rationale: [`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md).

## SPP5 authority

```text
provider-neutral request + deterministic post-process plan
 -> replaceable external SpriteGenerationProvider
 -> nondeterministic owned candidate response
 -> deterministic SPP5 candidate validation
 -> loose frames: SPP2 -> SPP3 -> S1
 -> manifest atlas: SPP4 -> SPP3 -> S1
 -> canonical SpriteAsset only after every required gate passes
```

Generation/provider output is external nondeterministic input. SPP5 does not make provider/model state, request retries, network state or generated pixels into engine runtime truth. Determinism begins from one concrete recorded response and explicit post-process plan.

Baseline scope in PR #169:

- protocol/network-independent `SpriteGenerationProvider` with one explicit generation call,
- preflight validation before provider execution so invalid plans cause zero provider calls,
- provider-neutral loose-frame output where caller-owned targets define canonical page/region/texture identity and optional exact pivot,
- exact positive-dimension RGBA8 byte validation and expected-frame gates,
- existing SPP2 quality/repair followed by SPP3 loose-frame import and S1 validation,
- explicit SPP4 generator-manifest atlas path without format auto-detection,
- provider failure/exception/request-identity/candidate-kind rejection,
- no canonical output exposure after failed post-generation validation,
- deterministic schema-versioned structural JSON for the same recorded response,
- fake/recorded providers only in CI; no live/paid generation service is required,
- focused backend-independent tests.

Provider SDKs, HTTP/auth/secrets, retries/backoff, background workers, prompt routing/optimization, runtime generation, learned/VLM quality judgment as deterministic truth and automatic semantic/pivot inference are outside SPP5.

## Performance boundary

- request/plan/envelope validation: `O(frame target count)`,
- loose candidate shape checks: `O(frame count)`,
- loose pixel processing reuses existing SPP2 cost and SPP3 `O(frame count)` import,
- manifest processing reuses SPP4 `O(manifest bytes + frame count)` plus SPP3/S1 validation,
- generated buffers are owned by the provider response and viewed by downstream APIs where possible,
- repaired RGBA8 copies occur only for explicit SPP2 repair,
- no SPP5 work enters fixed-step animation, normal rendering or GPU presentation,
- no new package is introduced.

## Current validation gate

Required on the final exact PR #169 head:

- Project State Contract,
- Sprite S0 Contract,
- Sprite SA0 Contract,
- repository/release/benchmark/content/Godot qualification jobs,
- Windows MSVC configure/build/full CTest,
- clean-clone README configure/build/full CTest.

No local full-build claim is made for this execution because the implementation container cannot resolve `github.com`. Hosted exact-head CI is the integration authority for this continuation.

No new real-GPU gate is required because SPP5 changes no presentation behavior.

## Continuation rule

Keep #169 draft until the final exact head has green hosted integration/audit evidence and stage documentation agrees with #168/implementation. Repair only SPP5 issues exposed by code review or CI. After #169 is ready and merged and #168 closes, stop. Do **not** start SE2E in the same completion continuation.
