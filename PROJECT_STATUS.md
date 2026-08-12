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
 -> SE2E
```

Frozen milestone references retained for contract continuity:

- #144 / PR #145 — SA0 deterministic Sprite animation timing/frame/event contract, frozen and complete.
- #142 / PR #143 / `2108122dad5ac2dcbb964f7ada0e80f7afa21003` — SR8 production Sprite renderer conformance, reproducible structural workloads and trusted presentation-GPU evidence.
- #152 / PR #153 / `c5952c0e905c46816b0a182b7d91143bf54b188b` — SA4 deterministic animation conformance and workload runner.
- #168 / PR #169 / `c3bcac89ca8c7ca21a9130b1b16cf7ece9e31c1a` — SPP5 provider-neutral generation orchestration.
- #170 / PR #171 / `41536c6045b8c89831a2026f090b5be7889599f3` — SE2E generated/imported Sprite proof, merged and complete.

Current child: **#172 / SPERF — final reproducible Sprite performance evidence and guidance**.  
Current branch: `agent/sprite-sperf-final-performance-evidence`.  
Exact next core item after SPERF merges green and #59 closes: **#103 Benchmark B1**.

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SPERF contract: [`docs/SPRITE_PERFORMANCE_SPERF.md`](docs/SPRITE_PERFORMANCE_SPERF.md).  
Machine-readable SPERF contract: [`docs/contracts/sprite-performance-sperf.json`](docs/contracts/sprite-performance-sperf.json).  
Repository-state authority/rationale: [`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md).

## SPERF authority

SPERF composes existing production evidence instead of adding another runtime subsystem:

```text
SR8 production Sprite structural metrics
 + SR7 existing upload/capacity counters
 + SA4 deterministic animation workloads
 + S1/SPP memory/complexity boundaries
 -> explicit deterministic evidence collection
 -> commit-bound trace2d.sprite-performance-final-gate.v1 manifest
 -> optional machine-labelled local timing kept outside portable correctness
```

Key frozen interpretation:

- SR8 remains real-GPU presentation authority; SPERF introduces no new GPU truth model.
- The frozen SR8 workload is 1,024 submitted / 768 visible / 256 culled / 960 visible quads / 7 contiguous compatibility runs.
- The current built-in Sprite vertex payload is 6 vertices per quad and 48 bytes per vertex, so the frozen workload represents exactly 276,480 Trace2D-owned uploaded vertex bytes.
- `spritePresentationUploadedVertexBytes` and `spriteVertexCapacityBytes` are different metrics: per-frame payload versus retained reusable high-water capacity.
- Neither engine-owned value is total graphics-driver allocation or residency.
- SA4 structural replay/digests remain deterministic evidence; optional Release timing remains environment-labelled evidence only.
- Decoded RGBA8 page payload may be calculated exactly as `width * height * 4` only for that explicit representation. #70/#86 own future package/resource representation and lifetime.
- SPP0-SPP5 work stays offline and outside ordinary animation/render frame budgets.

## Performance boundary

SPERF changes documentation/tooling only. It must add zero normal-frame production cost: no per-frame allocation, JSON/string formatting, filesystem access, wall-clock reads, hashing, capture/readback, provider work, Agent/MCP reporting, or duplicate global caches.

Generic runtime profiling remains future #91 rather than being pre-implemented inside Sprite.

## Current validation gate

Required on the final exact SPERF PR head:

- `trace2d.sprite-performance.v1` contract parses and agrees with stage documentation,
- `scripts/sprite_performance_final_gate.ps1` reproduces the SR8 structural marker,
- all three supplementary renderer workload structures match their committed values,
- all three SA4 animation workload structural runs return successful deterministic replay,
- Project State Contract,
- Sprite S0 Contract,
- Sprite SA0 Contract,
- repository/release/benchmark/content/Godot qualification jobs,
- Windows MSVC configure/build/full CTest,
- clean-clone README configure/build/full CTest.

No new real-GPU acceptance gate is introduced because SPERF changes no renderer behavior and SR8 already owns trusted presentation-GPU evidence. Optional timing is local Release evidence and is never a shared-CI threshold.

## Continuation rule

Keep the SPERF PR scoped to #172 and repair only evidence/contract/tooling issues exposed by review or CI. After the exact SPERF head is green, merge it, close #172 and close #59. Stop. The following `@GitHub Trace2D 다음 진행해줘` continuation begins **#103 Benchmark B1**.
