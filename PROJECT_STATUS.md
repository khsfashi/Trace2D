# Trace2D Project Status

Last repository-state update: **2026-08-08**

This document is the operational handoff for the next contributor or coding agent. Live repository state wins over stale prose.

## Current mission

Reach **v0.1.0-alpha.1 Public Alpha** with one complete minimal agent-first 2D development loop:

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

The complete loop and the first evidence-driven renderer optimization are implemented. Remaining work is repository/release quality, not broader engine feature development.

## Current phase

**Public Alpha repository-quality / release preparation — Issue #14**

P0-P5 are complete. The Public Alpha vertical sample is complete through PR #32. PR #34 implements the measured contiguous same-texture instancing candidate identified by that sample.

Recent milestones:

- P5 SDL3 GPU renderer foundation — PR #24
- orthographic camera / sprite render-data contract — PR #25
- textured sprite submission — PR #26
- ordered multi-sprite baseline — PR #27
- actual submission culling + metrics — PR #28
- contiguous-texture batching measurement — PR #29
- persistent offscreen presentation target — PR #30
- explicit-frame GPU readback + deterministic BMP — PR #31
- Public Alpha end-to-end vertical sample — PR #32
- Public Alpha handoff refresh / batching evidence — PR #33
- contiguous same-texture GPU instancing — PR #34

## Public Alpha vertical sample

Committed sample:

```text
samples/public_alpha/public_alpha.trace2d.toml
```

Documented workflow:

```text
docs/PUBLIC_ALPHA_SAMPLE.md
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

## Completed measured batching decision

PR #32 changed the representative workload from one visible sprite to seven visible sprites in two contiguous texture runs. The five-draw candidate reduction justified the narrow mechanism already documented in `docs/BATCHING.md`.

PR #34 implements it with these invariants:

- caller-provided visible painter sequence is preserved,
- no texture sorting,
- culled sprites emit no instance/draw and do not split a visible same-texture run,
- full supplied-span texture validation semantics remain visibility-independent,
- no renderer-owned per-frame visible-sprite list,
- persistent GPU instance and upload transfer buffers,
- geometric capacity growth only when retained visible capacity is insufficient,
- steady-capacity frames do not recreate application-level instance buffers,
- SDL GPU cycling is used for transfer/destination reuse across in-flight work,
- `submittedSprites` counts encoded visible instances independently from `drawCalls`,
- headless/runtime/scene/input/agent/testing layers remain renderer-independent.

The Public Alpha sample's before/after draw contract is therefore:

```text
before: 7 visible sprites -> 7 sprite draws
PR #34: 7 visible sprites -> 2 contiguous instanced draws
```

Hosted CI validates Windows/MSVC compilation and backend-independent tests; the documented windowed Public Alpha command is the explicit real-GPU presentation/metrics smoke surface.

## Immediate next task — repository-quality gates

Do **not** start P6 engine breadth yet.

Continue Issue #14 in this order:

1. choose/add the project license,
2. review and document third-party license obligations,
3. inspect repository content/history for secrets and private machine paths,
4. verify README quick start from a clean clone,
5. verify implemented-vs-planned wording is accurate,
6. document explicit Public Alpha limitations,
7. run release-candidate `main` CI and mark this file release-ready,
8. create `v0.1.0-alpha.1`,
9. change repository visibility to Public,
10. verify README/release from the public view and open follow-up issues for known limitations.

If license choice requires owner preference, do not guess; prepare the comparison/impact and resolve it before visibility changes.

## Current validation status

Validation uses clean GitHub-hosted Windows runners with the repository-pinned vcpkg baseline and MSVC warnings-as-errors configuration.

Recent validated milestones include:

- PR #24 — CI #64 green
- PR #25 — CI #68 green
- PR #26 — CI #71 green
- PR #27 — CI #74 green
- PR #28 — CI #78 green
- PR #29 — CI #81 green
- PR #30 — CI #84 green
- PR #31 — CI #90 green
- PR #32 — CI #93 green
- PR #34 implementation head — CI #97 green: Configure, Build, and full CTest passed

GPU presentation itself is intentionally not a hosted-runner requirement. Backend-independent camera, instance-transform, visibility, batching measurement, capture-layout, artifact, headless runtime, and gameplay tests remain CI-testable without an interactive GPU/window.

## Phase exit criteria

### P0-P5 — complete

- [x] project/build foundation
- [x] SDL3 platform boundary
- [x] deterministic fixed-step runtime
- [x] stable entity identity / deterministic scene registry
- [x] text-first deterministic scene format
- [x] structured inspection / semantic queries
- [x] deterministic virtual input
- [x] deterministic gameplay scenario runner / assertions
- [x] minimal SDL3 GPU 2D renderer
- [x] ordered multi-sprite submission
- [x] culling integration + metrics
- [x] batching opportunity measurement
- [x] persistent offscreen presentation/capture source
- [x] explicit simulation-frame capture artifact

### Public Alpha — in progress

- [x] tiny end-to-end sample proving the full automation loop
- [x] documented edit -> build -> run -> inspect -> input -> assert -> capture workflow
- [x] measured contiguous same-texture instancing implementation
- [ ] repository license selected and added
- [ ] third-party license review completed
- [ ] repository/history secret and private-path review completed
- [ ] README quick start verified from a clean clone
- [ ] implemented/planned features clearly distinguished
- [ ] Public Alpha limitations documented
- [ ] release-candidate `main` CI green
- [ ] create `v0.1.0-alpha.1`
- [ ] change repository visibility to Public
- [ ] verify README/release from public view

## Public Alpha blockers

Completed technical blockers:

- [x] deterministic headless execution
- [x] explicit frame stepping
- [x] stable text-authored scene/entity identity
- [x] structured runtime inspection
- [x] semantic selectors
- [x] virtual input
- [x] gameplay assertions
- [x] minimal textured sprite renderer
- [x] ordered multi-sprite submission
- [x] culling baseline
- [x] measured contiguous same-texture instancing
- [x] offscreen render target suitable for visual readback
- [x] deterministic capture at a known simulation frame
- [x] one tiny end-to-end sample proving the workflow

Remaining release blockers:

- [ ] clean Windows build/test documentation verified from a clean clone
- [ ] green release-candidate `main` CI
- [ ] repository license and third-party license review
- [ ] secret/private-path/history review
- [ ] explicit Public Alpha limitations
- [ ] documentation remains accurate from public view

See `docs/PUBLIC_RELEASE.md` for exact release gates.

## Explicit non-blockers for first public release

Do not delay Public Alpha for:

- MCP adapter
- full editor
- Box2D feature completeness
- semantic UI tree completeness
- networking/audio
- job system
- custom allocator framework
- advanced renderer/lighting
- render graph
- bindless/GPU-driven rendering
- Linux/macOS/mobile support

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

## Known decisions still open

Resolve only when their release/implementation phase arrives:

- project license before repository visibility changes to Public,
- exact third-party notice/documentation shape required by the selected project license and dependencies,
- whether construction-time shadercross should later be replaced by offline precompiled shader artifacts, only if measured startup/distribution/CI cost justifies it,
- exact protocol/transport before a future MCP adapter.

The first batching mechanism is no longer an open decision: contiguous same-texture instancing is implemented and global texture sorting remains disallowed.

## Handoff rule

Every PR that materially advances the Public Alpha release must keep this file aligned with live repository state. At minimum keep these sections true:

- Current phase
- Immediate next task
- Current validation status
- Phase exit criteria
- Public Alpha blockers
- Architecture invariants
- Known decisions still open

A future conversation should be able to continue from this repository without relying on previous chat context.
