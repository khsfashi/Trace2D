# Trace2D Project Status

Last repository-state update: **2026-08-08**

This document is the operational handoff for the next contributor or coding agent. Live repository state wins over stale prose.

## Current mission

Ship **v0.1.0-alpha.1 Public Alpha** with one complete minimal agent-first 2D development loop:

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

The technical loop, first evidence-driven renderer optimization, repository-quality preparation, MIT licensing, and final release-candidate validation are complete. Do **not** start P6 engine breadth until the Public Alpha publication sequence finishes.

## Current phase

**Public Alpha release-ready / publication — Issue #14**

P0-P5 are complete. The Public Alpha vertical sample is complete through PR #32, PR #34 implements the measured contiguous same-texture instancing slice, PR #35 added executable repository-quality gates, and PR #36 finalized the MIT license-required release gate.

The repository owner selected the **MIT License** on 2026-08-08. The canonical root `LICENSE` exists and CI requires it through `./scripts/release_audit.ps1 -RequireLicense`.

PR #36 CI #103 passed all release-facing jobs, PR #36 was squash-merged to `main` as `82fed78`, and the repository owner confirmed the resulting `main` CI is green on 2026-08-08.

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

## Public Alpha sample contract

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

## Repository / release quality

Completed:

- [x] third-party source dependency/license review documented in `docs/THIRD_PARTY.md`
- [x] implemented-vs-planned README wording clear
- [x] explicit Public Alpha limitations documented in `docs/PUBLIC_ALPHA_LIMITATIONS.md`
- [x] repeatable release audit at `scripts/release_audit.ps1`
- [x] audit checks tracked generated/build artifacts
- [x] audit checks high-confidence secret/private-path patterns in current tree
- [x] audit checks fetched Git patch history
- [x] audit checks repository-relative Markdown links
- [x] README Quick Start pins the same vcpkg baseline as CI
- [x] `windows-2022` clean-checkout Quick Start CI job
- [x] PR #35 / CI #100: `release-audit`, `windows-msvc`, and `clean-clone-quick-start` all green
- [x] project license explicitly selected: **MIT**
- [x] canonical root `LICENSE` added
- [x] license decision recorded in `docs/LICENSE_DECISION.md`
- [x] release audit requires `LICENSE`
- [x] PR #36 / CI #103 green across all release-facing jobs
- [x] PR #36 merged to `main`
- [x] final release-candidate `main` CI confirmed green
- [x] release state marked ready in `PROJECT_STATUS.md` and Issue #14

Still required for publication:

- [ ] create `v0.1.0-alpha.1` GitHub Release/tag
- [ ] change repository visibility to Public
- [ ] verify README/release/license from an unauthenticated public view
- [ ] open only concrete post-alpha follow-up issues from known limitations

## License decision

Trace2D uses the **MIT License**.

The repository owner explicitly chose MIT for the initial Public Alpha because it provides a familiar permissive license with minimal administrative overhead for a small source-first engine/portfolio project.

The direct Public Alpha dependency set remains independently licensed:

- SDL3 — zlib
- SDL3_shadercross — zlib
- toml++ — MIT
- GoogleTest — BSD-3-Clause, tests/development

The project-level MIT license does not replace third-party license obligations. Future compiled binary attachments must still review the exact resolved vcpkg runtime graph and bundle required notices.

Strict release audit:

```powershell
./scripts/release_audit.ps1 -RequireLicense
```

## Validation status

Validation uses clean GitHub-hosted Windows runners with the repository-pinned vcpkg baseline and MSVC warnings-as-errors configuration.

Recent validated milestones:

- PR #24 — CI #64 green
- PR #25 — CI #68 green
- PR #26 — CI #71 green
- PR #27 — CI #74 green
- PR #28 — CI #78 green
- PR #29 — CI #81 green
- PR #30 — CI #84 green
- PR #31 — CI #90 green
- PR #32 — CI #93 green
- PR #34 implementation head — CI #97 green
- PR #35 repository-quality candidate — CI #100 green across all three release-facing jobs
- PR #36 MIT/license-required release gate — CI #103 green across all three release-facing jobs
- final `main` release candidate after PR #36 — green, owner-confirmed 2026-08-08

GPU presentation itself remains outside hosted-runner requirements. Backend-independent camera, instance-transform, visibility, batching measurement, capture-layout, artifact, headless runtime, and gameplay tests remain CI-testable without an interactive GPU/window.

## Public Alpha exit criteria

### Technical loop — complete

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

### Repository/release quality — complete

- [x] root MIT project license
- [x] third-party source license review
- [x] automated secret/private-path/history audit green
- [x] clean-checkout README Quick Start green
- [x] implemented/planned wording clear
- [x] explicit Public Alpha limitations
- [x] Markdown link audit green
- [x] license-required release-gate CI green
- [x] green release-candidate `main` CI

### Publication — pending

- [ ] `v0.1.0-alpha.1` GitHub Release/tag
- [ ] Public repository visibility
- [ ] unauthenticated public-view verification
- [ ] concrete post-alpha follow-up issues

See `docs/PUBLIC_RELEASE.md` for the canonical gate definitions.

## Explicit non-blockers

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

## Immediate next task

1. create GitHub Release/tag `v0.1.0-alpha.1` targeting the release-ready `main`,
2. change repository visibility to Public,
3. verify README, release, links, and MIT license from an unauthenticated public view,
4. close Issue #14 after publication verification,
5. open only concrete post-alpha issues from known limitations,
6. then begin P6 / post-alpha work.

## Handoff rule

Every PR that materially advances Public Alpha must keep this file aligned with live repository state. A future conversation should be able to continue from the repository without relying on previous chat context.
