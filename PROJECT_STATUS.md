# Trace2D Project Status

Last repository-state update: **2026-08-08**

This document is the operational handoff for the next contributor or coding agent. Live repository state wins over stale prose.

## Current phase

**Public Alpha released — post-alpha development may begin.**

`v0.1.0-alpha.1` was published on 2026-08-08 and the repository is Public under the MIT License. The first release proves one complete minimal agent-first 2D development loop:

```text
text-authored scene
  -> build
  -> deterministic headless run
  -> explicit frame step
  -> semantic state query
  -> virtual input
  -> gameplay assertion
  -> ordered 2D render
  -> frame-specific visual capture
```

The technical loop, first evidence-driven renderer optimization, repository-quality preparation, license-required release audit, clean-clone Quick Start, and Public Alpha publication are complete.

## Public Alpha release record

- [x] P0-P5 technical milestones complete
- [x] Public Alpha vertical sample — PR #32
- [x] measured contiguous same-texture GPU instancing — PR #34
- [x] repository quality gates — PR #35 / CI #100
- [x] MIT license-required release gate — PR #36 / CI #103
- [x] release-ready documentation — PR #37 / CI #105
- [x] repository visibility changed to Public
- [x] tag `v0.1.0-alpha.1` exists and exposes the release source tree
- [x] root MIT `LICENSE` exists on the release tag
- [x] Public Alpha release notes and limitations are committed

GitHub Issue #14 is the canonical completion record for the first Public Alpha.

## Public Alpha sample contract

Committed sample:

```text
samples/public_alpha/public_alpha.trace2d.toml
```

Default deterministic contract:

```text
frames:                     8
seed:                       42
KeyD press frame:           2
KeyD release frame:         6
#player.position.x:          4.0
visible sprites:             7
culled sprites:              0
contiguous texture runs:     2
unbatched baseline draws:    7
ordered instanced draws:     2
measured draw-call saving:   5
```

The renderer keeps `submittedSprites == 7` while `drawCalls == 2` after successful windowed submission because only adjacent visible same-texture sprites share a draw.

## Completed technical milestones

- P0 project/build foundation — PR #1
- P1 SDL3 platform boundary — Issue #2 / PR #16
- P1 deterministic fixed-step runtime — Issue #3 / PR #17
- P2 stable entity identity / scene registry — Issue #4 / PR #18
- P2 text-first deterministic scene format — Issue #5 / PR #19
- P3 protocol-independent runtime inspection — Issue #6 / PR #20
- P3 semantic selectors / runtime queries — Issue #7 / PR #21
- P4 deterministic virtual input — Issue #8 / PR #22
- P4 gameplay scenario runner / assertions — Issue #9 / PR #23
- P5 SDL3 GPU renderer foundation — PR #24
- orthographic camera / sprite render-data contract — PR #25
- textured sprite submission — PR #26
- ordered multi-sprite baseline — PR #27
- submission culling + metrics — PR #28
- contiguous-texture batching measurement — PR #29
- persistent offscreen presentation target — PR #30
- explicit-frame GPU readback + deterministic BMP — PR #31
- Public Alpha end-to-end vertical sample — PR #32
- Public Alpha handoff / batching evidence — PR #33
- contiguous same-texture GPU instancing — PR #34
- Public Alpha repository quality gates — PR #35
- MIT/license-required release gate — PR #36
- release-ready publication handoff — PR #37

## Validation policy

Release-facing CI remains the baseline for changes that can affect the supported developer path:

- `release-audit` using `./scripts/release_audit.ps1 -RequireLicense`
- `windows-msvc` configure/build/full CTest
- `clean-clone-quick-start` using the README-pinned vcpkg baseline

GPU presentation itself is not a hosted-runner requirement. Backend-independent camera, instance-transform, visibility, batching measurement, capture-layout, artifact, headless runtime, and gameplay contracts remain CI-testable without an interactive GPU/window.

## Architecture invariants

- `engine/core` has no SDL dependency.
- SDL-specific ownership/types stay behind platform/render boundaries.
- `engine/platform` owns SDL initialization/window lifetime; renderer receives a Trace2D-owned numeric window ID.
- `engine/render` may depend on platform/SDL3, but runtime/scene/input/agent/testing do not depend on renderer presentation state.
- renderer GPU state is presentation state and never authoritative simulation state.
- CPU camera/sprite/instance render data is Trace2D-owned presentation input.
- persistent renderer resources are setup or capacity/size-dependent state; steady-state frames do not recreate them.
- normal non-capture frames perform no capture download, fence wait, mapping, normalization, or file I/O.
- multi-sprite submission consumes non-owning caller storage and does not copy/grow a renderer frame list.
- renderer submission preserves caller-provided painter order.
- texture identity never participates in global draw-order sorting.
- culling uses the shared inclusive AABB rule.
- batching may only combine sprites already contiguous in the visible painter sequence unless equivalence is proven.
- `submittedSprites` and `drawCalls` are independent metrics after instancing.
- renderer metrics are committed from actual successful GPU submission, not speculative work before failure.
- texture validation semantics do not depend on camera visibility.
- swapchain is presentation-only; capture/readback sources the renderer-owned offscreen target.
- capture frame selection uses simulation frame identity, never wall-clock timing.
- runtime has no SDL, renderer, CLI, JSON, or MCP dependency.
- agent/testing layers compose lower-level systems without reversing dependency direction.
- authored scene/project state is text-first and deterministic.
- structured state beats pixel inference for gameplay QA.
- semantic selectors beat coordinate targeting where identity exists.
- optimization complexity follows measurement.

## Post-alpha priorities

Post-alpha work should extend the proven automation contract rather than replace it. Concrete follow-up issues should be narrow and justified by either a known Public Alpha limitation or measured need.

Recommended order:

1. add a protocol transport/MCP adapter over the existing protocol-independent agent facade without introducing runtime dependency inversion,
2. add a practical asset caching/import slice that remains text-first and deterministic,
3. establish broader reproducible performance workloads before adding more renderer complexity,
4. add physics and semantic UI only through deterministic/queryable contracts,
5. broaden platform support after the Windows developer path remains stable.

Do not add broad editor, render-graph, allocator, job-system, or engine-framework abstractions merely because Public Alpha is complete.

## Handoff rule

Every PR that materially changes project phase, release state, architecture invariants, or the active post-alpha priority must keep this file aligned with live repository state. A future conversation should be able to continue from the repository without relying on previous chat context.
