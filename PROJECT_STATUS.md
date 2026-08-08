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

The technical loop and the first evidence-driven renderer optimization are complete. Do **not** start P6 engine breadth before the Public Alpha repository/release gates are complete.

## Current phase

**Public Alpha repository-quality / release preparation — Issue #14**

P0-P5 are complete. The Public Alpha vertical sample is complete through PR #32, and PR #34 implements the measured contiguous same-texture instancing slice.

The active repository-quality candidate adds:

- repeatable current-tree and Git-history release auditing,
- tracked generated/build artifact checks,
- repository-relative Markdown link checks,
- a dedicated Windows Server 2022 clean-checkout README Quick Start CI job,
- third-party source dependency/license review,
- explicit Public Alpha limitations,
- an owner-controlled MIT vs Apache-2.0 license decision document,
- refreshed README and canonical release gates.

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

## Repository-quality work

### Implemented in the active candidate

- [x] third-party source dependency/license review documented in `docs/THIRD_PARTY.md`
- [x] implemented-vs-planned README wording refreshed
- [x] explicit Public Alpha limitations documented in `docs/PUBLIC_ALPHA_LIMITATIONS.md`
- [x] MIT vs Apache-2.0 project-license decision prepared in `docs/LICENSE_DECISION.md`
- [x] repeatable release audit added at `scripts/release_audit.ps1`
- [x] audit checks tracked generated/build artifacts
- [x] audit checks high-confidence secret/private-path patterns in current tree
- [x] audit checks fetched Git patch history
- [x] audit checks repository-relative Markdown links
- [x] README Quick Start pins the same vcpkg baseline as CI
- [x] CI candidate includes a `windows-2022` clean-checkout Quick Start job

### Still requires validation or owner action

- [ ] project license explicitly selected and root `LICENSE` added
- [ ] release audit passes on the repository-quality PR
- [ ] clean-checkout Quick Start job passes on the repository-quality PR
- [ ] ordinary Windows/MSVC configure/build/full CTest passes on the repository-quality PR
- [ ] repository-quality changes merged to `main`
- [ ] release audit switched to require `LICENSE`
- [ ] final release-candidate `main` CI green
- [ ] `PROJECT_STATUS.md` and Issue #14 marked release-ready
- [ ] `v0.1.0-alpha.1` created
- [ ] repository visibility changed to Public
- [ ] README/release/license verified from unauthenticated public view

## License decision

Do not guess the project license.

The repository was intentionally created with no license. The two prepared candidates are:

- **MIT** — simplest/familiar permissive license with minimal administrative text,
- **Apache-2.0** — permissive license with an explicit patent grant/termination framework.

The direct Public Alpha dependency set reviewed in `docs/THIRD_PARTY.md` is permissive and does not force one candidate over the other.

After owner selection, add the canonical root `LICENSE`, update README licensing text, and run:

```powershell
./scripts/release_audit.ps1 -RequireLicense
```

## Third-party distribution decision

The first Public Alpha is primarily a source/repository release.

Direct manifest dependencies:

- SDL3 — zlib
- SDL3_shadercross — zlib
- toml++ — MIT
- GoogleTest — BSD-3-Clause, tests/development

Compiled binary release artifacts are **not** automatically cleared by this source review. Any future binary attachment must review the exact resolved vcpkg runtime graph and bundle required port notices.

## Validation status

Validation uses clean GitHub-hosted Windows runners with the repository-pinned vcpkg baseline and MSVC warnings-as-errors configuration.

Recent validated technical milestones:

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

The repository-quality candidate intentionally adds two release-facing validations beyond the prior CI surface: the repository/history audit and a Windows Server 2022 clean-checkout job that executes the README developer presets.

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

### Repository/release quality — in progress

- [ ] root project license
- [x] third-party source license review prepared
- [ ] automated secret/private-path/history audit green
- [ ] clean-checkout README Quick Start green
- [x] implemented/planned wording clear
- [x] explicit Public Alpha limitations
- [ ] Markdown link audit green
- [ ] green release-candidate `main` CI
- [ ] tag/release/public visibility/public-view verification

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

1. validate and merge the repository-quality candidate,
2. obtain the owner's explicit **MIT** or **Apache-2.0** selection,
3. add the root license and enable license-required release auditing,
4. run final `main` CI,
5. mark Issue #14 release-ready,
6. create `v0.1.0-alpha.1`, make the repository Public, verify the unauthenticated view, and open only concrete post-alpha follow-up issues.

## Handoff rule

Every PR that materially advances Public Alpha must keep this file aligned with live repository state. A future conversation should be able to continue from the repository without relying on previous chat context.
