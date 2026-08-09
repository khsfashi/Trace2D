# Trace2D Project Status

Last repository-state update: **2026-08-08**

This document is the operational handoff for the next contributor or coding agent. Live repository state wins over stale prose.

## Current phase

**Public Alpha released — #40 deterministic texture assets and #42 text/basic UI are complete; #43 semantic UI is next. The owner-fixed post-#41 particle pipeline is planned in #46 / #47-#53.**

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

Public Alpha completion is recorded in Issue #14. Post-alpha work must extend this proven contract rather than replace it.

## Next execution order — owner-fixed

The repository owner fixed this implementation order on **2026-08-08**.

**Future coding agents must follow this sequence unless the repository owner explicitly changes it. Do not reorder, skip, parallelize, or substitute later roadmap work because another task appears more attractive.**

1. **#40 — deterministic texture asset cache/import slice** — complete via PR #45
2. **#42 — text rendering and basic UI primitives** — complete via PR #55
3. **#43 — semantic UI tree and agent interaction**
4. **#39 — MCP transport over the completed protocol-independent agent/UI facade**
5. **#41 — reproducible renderer performance workloads**
6. **#47 — particle deterministic frame/keyed-random contracts**
7. **#48 — rich deterministic CPU particle reference simulation**
8. **#49 — text-authored particle effect assets + `ParticleEmitter2D`**
9. **#50 — complete Agent verification over CPU particle reference state**
10. **#51 — CPU particle cost analysis + explicit human backend choice + deterministic particle compiler**
11. **#52 — GPU runtime backend for explicitly GPU-selected effects**
12. **#53 — CPU/GPU conformance, workloads, safe budgets, build flow, and human/LLM guidance**
13. after #53, return to umbrella **#13** and split exactly one next breadth item: physics/Box2D, sprite animation, or safe hot reload

Particle umbrella: **#46**. Detailed design contract: [`docs/PARTICLES.md`](docs/PARTICLES.md).

### Execution rule

- Work only on the first incomplete and unblocked item above.
- If that item has an active PR, finish/repair that PR before starting anything else.
- Merge with green CI, update this file, then advance exactly one step.
- If an earlier task reveals a genuine prerequisite, implement only the smallest prerequisite necessary and keep the owner-fixed sequence intact.
- Do **not** start #39 MCP before #43 semantic UI is complete.
- Do **not** start #47 particles before #41 renderer workloads are complete.
- Within particles, do not start the next numbered child until the previous child is merged with green CI and this handoff advances.
- MCP is transport, not the engine API. It exposes an already-complete protocol-independent agent/UI vocabulary rather than driving engine architecture.
- UI automation is semantic-first: stable identity/role/name plus structured state/actions. Coordinates may be observable bounds, but must not be the primary automation identity when semantic identity exists.
- Structured state beats pixel inference for gameplay, UI, and CPU-reference particle assertions.

**Active next implementation task after PR #55 merges: #43.**

## Completed #40 texture asset slice

The first P6 asset slice establishes a narrow CPU-side texture import/cache boundary:

- text-authored texture identity is the canonical project-relative path, never a machine-local absolute path,
- `/` and `\` spellings normalize to one `/`-separated cache ID,
- absolute paths and `..` traversal are rejected,
- PNG/JPEG/BMP/TGA sources decode to immutable RGBA8 CPU data,
- successful imports are cached and repeated references return the same decoded asset object,
- invalidation is explicit through `Invalidate` / `Clear`; there is no frame-loop polling or watcher,
- failures are not cached, so a corrected source can recover on the next explicit load,
- structured diagnostic codes cover invalid reference, unsupported format, missing/read/decode failure, and size overflow,
- cache metrics expose requests/hits/misses/imports/failures/current entry count,
- a deterministic seven-sprite-style test proves one player plus six marker references resolve to two imports and five cache hits,
- `engine/assets` is SDL-free and owns no renderer/GPU handles.

See `docs/ASSETS.md` for the contract and scope boundaries.

## Completed #42 text/basic UI slice

The first UI slice establishes the engine-owned state and rendering input that #43 semantic automation will extend:

- `engine/ui` is SDL-free and does not depend on the renderer,
- authored `*.trace2d.toml` UI uses a strict versioned schema with stable non-empty element IDs,
- V1 supports exactly `panel`, `label`, `button`, and `text_input`,
- bounds are deterministic unsigned integer canvas pixels and observable order is authored order,
- focus is supported for buttons/text inputs and button activation counts are inspectable headlessly,
- focus/activation perform no heap allocation and element lookup remains a simple deterministic O(N) scan until workloads justify indexing,
- a fixed dependency-free 5x7 ASCII-oriented font provides deterministic label rendering without machine font discovery,
- `RasterizeUi` produces deterministic RGBA8 pixels from the same `UiDocument` used by headless state tests,
- caller-owned raster storage is reused when dimensions do not change and the rasterizer creates no temporary element/glyph collections,
- `trace2d_ui_preview --headless` validates authored UI without a renderer,
- `trace2d_ui_preview --windowed` uploads that same CPU raster through `Renderer::CreateTextureRgba8` and presents it as one sprite,
- the renderer owns only presentation resources; UI identity/state/bounds/text never become GPU-authoritative,
- committed tests cover authored-order/bounds determinism, structured invalid-field/duplicate/out-of-bounds diagnostics, focus/activation behavior, deterministic repeated raster bytes, and steady-size output-buffer reuse,
- `samples/ui/basic_ui.trace2d.toml` is the first diffable authored UI sample.

The built-in font is deliberately not a complete typography system: Unicode shaping, CJK, font fallback, kerning, authored fonts, rich text, and localization layout remain out of scope until real content requires them.

See `docs/UI.md` for the format, performance contract, preview commands, and deliberate non-goals.

## Why this order

The asset slice comes first because practical authored UI/text needs deterministic project-relative resource identity and reuse. Text rendering/basic UI then establishes the smallest engine-owned UI state and layout foundation. Semantic UI automation now builds directly on that completed foundation before MCP so the transport layer exposes a stable final vocabulary instead of forcing a second redesign. Renderer workloads then establish reproducible measurement rules before another render-heavy feature arrives.

Particles follow #41 because their design intentionally makes performance decisions from evidence. Trace2D first builds a rich, deterministic, fully observable CPU reference effect, measures its structural and local CPU cost, and only then allows a human to choose whether the effect remains CPU or is explicitly compiled to a minimized GPU backend.

## P6 umbrella

Issue #13 is the parent roadmap for the practical authored-game slice. The fixed child sequence is:

```text
#40 assets
  -> #42 text/basic UI
  -> #43 semantic UI automation
  -> #39 MCP transport
  -> #41 renderer workloads
  -> #47 particle semantics/random
  -> #48 rich CPU reference
  -> #49 authored effects/emitter
  -> #50 Agent particle verification
  -> #51 CPU cost + human backend decision + compiler
  -> #52 explicit GPU backend
  -> #53 conformance/workloads/guidance
  -> split one next breadth item
```

Do not treat #13 as permission to start Box2D, animation, hot reload, editor work, or other breadth before the fixed sequence completes.

## Semantic UI target

Issue #43 must extend `engine/ui` rather than replace it. The semantic UI milestone is not complete until supported controls expose structured state such as:

- stable semantic identity
- role
- name
- bounds
- visible
- enabled
- focused
- text/value where applicable

The protocol-independent Agent facade must support semantic operations conceptually equivalent to:

```text
query role=button name="Start Game"
inspect enabled/visible/bounds
activate semantic control
query role=textbox name="Player Name"
focus control
input text
assert resulting UI/game state
```

Headless semantic UI tests must operate on the same `UiDocument` state used for presentation and must not require renderer initialization. MCP support comes only afterward in #39.

## Particle target after #41

Particle implementation is governed by #46 and `docs/PARTICLES.md`.

The defining workflow is:

```text
rich text effect
  -> deterministic CPU reference simulation
  -> full structured Agent verification
  -> deterministic structural CPU cost report
  -> optional machine-specific Release timing
  -> human chooses backend=cpu or backend=gpu
  -> GPU-selected effects compile to minimized GPU state
  -> CPU/GPU conformance + visual QA
```

Hard rules:

- the CPU backend is the exact semantic reference and may keep rich supported particle state,
- CPU capacity remains bounded and per-particle object/allocation/string/map/callback state is not allowed in the steady update path,
- ordinary CPU stepping performs no JSON/snapshot/fingerprint work unless explicitly requested,
- cost reports expose raw memory/operation/particle metrics; machine-dependent timing is labeled separately,
- an LLM may recommend `keep_cpu` or `consider_gpu` only from documented evidence,
- backend selection is explicit reviewable text controlled by a human,
- no tool silently converts CPU -> GPU and no unsupported GPU effect silently falls back to CPU,
- GPU-selected effects do not also pay full CPU reference simulation in normal runtime mode,
- GPU runtime layout is minimized from compiler/static analysis instead of copying the complete CPU reference state,
- CPU remains the exact deterministic oracle; V1 does not claim universal cross-vendor bit-identical GPU floating point,
- gameplay authority never depends on visual particle state.

## Public Alpha release record

- [x] P0-P5 technical milestones complete
- [x] Public Alpha vertical sample — PR #32
- [x] measured contiguous same-texture GPU instancing — PR #34
- [x] repository quality gates — PR #35 / CI #100
- [x] MIT license-required release gate — PR #36 / CI #103
- [x] release-ready documentation — PR #37 / CI #105
- [x] post-public documentation cleanup — PR #38 / CI #107
- [x] repository visibility Public
- [x] release/tag `v0.1.0-alpha.1`
- [x] root MIT `LICENSE`
- [x] Issue #14 closed completed

## Validation policy

Release-facing CI remains the baseline for changes that can affect the supported developer path:

- `release-audit` using `./scripts/release_audit.ps1 -RequireLicense`
- `windows-msvc` configure/build/full CTest
- `clean-clone-quick-start` using the README-pinned vcpkg baseline

New machine-facing capabilities require deterministic automated tests when practical. A semantic UI, MCP, or CPU-reference particle feature without headless coverage is incomplete unless its PR documents a concrete reason.

GPU presentation itself is not a hosted-runner requirement. Backend-independent UI state/layout/rasterization, query, input, assertion, camera, visibility, batching measurement, capture-layout, artifact contracts, asset behavior, particle CPU reference semantics, particle compiler/static analysis, and structural cost reports should remain CI-testable without an interactive GPU/window.

Wall-clock CPU/GPU timing is environment-dependent evidence. Hosted CI must not use unstable microsecond thresholds as deterministic correctness gates.

## Architecture invariants

- `engine/core` has no SDL dependency.
- SDL-specific ownership/types stay behind platform/render boundaries.
- `engine/assets` is SDL-free; decoded CPU assets are independent of renderer-owned GPU resources.
- asset references are deterministic project-relative text identities; no per-frame discovery/decoding is permitted.
- `engine/ui` is SDL-free; UI identity/state/bounds/text and CPU raster output are independent of renderer-owned GPU resources.
- renderer GPU state is presentation state and never authoritative gameplay/UI state.
- runtime/scene/input/agent/testing and engine-owned UI/particle semantic state do not depend on MCP transport.
- MCP/JSON-RPC/CLI are adapters over protocol-independent engine/agent contracts.
- persistent renderer resources are setup or capacity/size-dependent state; steady-state frames do not recreate them.
- normal non-capture frames perform no capture download, fence wait, mapping, normalization, or file I/O.
- renderer submission preserves caller-provided painter order.
- texture identity never participates in global draw-order sorting.
- culling uses shared documented visibility semantics and never changes authoritative simulation state.
- batching may only combine sprites/particles already compatible and contiguous in the visible painter sequence unless equivalence is proven.
- capture frame selection uses simulation frame identity, never wall-clock timing.
- authored project/scene/UI/particle state is text-first and deterministic.
- structured state beats pixel inference.
- semantic selectors beat coordinate targeting where identity exists.
- particle CPU reference state is the semantic oracle; GPU is an explicitly selected compiled runtime backend.
- particle backend selection is never changed silently by analysis or runtime fallback.
- optimization complexity follows measurement.

## Handoff rule

Every PR that completes or materially changes an item in the owner-fixed execution order must update this file in the same PR.

A fresh coding agent following `AGENTS.md` should be able to read this file, select the first incomplete/unblocked item, open the relevant issue/design document, and continue without previous chat history.
