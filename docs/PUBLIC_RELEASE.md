# Public Release Record

## v0.1.0-alpha.1 — Public Alpha

**Released 2026-08-08.**

Trace2D's first public milestone proves one complete agent-first 2D development loop. It is a proof of architecture and workflow, not a claim that Trace2D is a feature-complete general-purpose engine.

## What the release proves

A coding agent can use normal source-control and command-line tooling to perform this loop without relying on a graphical editor:

```text
1. edit text-authored project/scene data or C++ code
2. build the project
3. launch the runtime headlessly
4. control simulation advancement explicitly
5. inspect authoritative structured game state
6. locate entities through semantic selectors
7. inject virtual input
8. assert gameplay behavior
9. render the same project in windowed/offscreen mode
10. capture a visual artifact at a known simulation frame
11. receive actionable structured diagnostics when a step fails
```

## Completed release gates

### Reproducible project foundation

- [x] clean Windows checkout Quick Start validated in hosted CI
- [x] Debug build and automated tests pass in hosted CI
- [x] README Quick Start and CI use the same CMake presets and pinned vcpkg baseline
- [x] generated build artifacts excluded from repository policy

### Deterministic runtime

- [x] shared authoritative simulation between headless and windowed modes
- [x] explicit fixed timestep and exact-N-frame advancement
- [x] observable frame/seed/runtime state
- [x] deterministic reset and repeated scenario behavior

### Text-authored scene and stable identity

- [x] readable TOML scene format
- [x] stable semantic entity identity, names/tags, and `Transform2D`
- [x] strict actionable schema errors
- [x] deterministic serialization and observable ordering
- [x] generation-safe runtime entity handles

### Structured observability

- [x] protocol-independent runtime inspection facade
- [x] structured runtime/scene/entity/component snapshots
- [x] semantic selectors by authored ID, name, tag, and component
- [x] deterministic query ordering and explicit ambiguity/no-match failures

### Virtual input and gameplay QA

- [x] physical and virtual input converge on engine-owned input state
- [x] frame-indexed virtual press/release scheduling
- [x] deterministic gameplay scenarios and exact-frame assertions
- [x] reproducible failure context with expected/observed values, frame, seed, input, runtime, and entity state

### Minimal 2D rendering and visual capture

- [x] SDL3 GPU orthographic sprite renderer
- [x] caller/painter-order-preserving multi-sprite submission
- [x] inclusive AABB visibility rule
- [x] measured contiguous same-texture GPU instancing
- [x] renderer-owned offscreen target and explicit simulation-frame capture
- [x] deterministic dependency-free 32-bit BMP artifact

### End-to-end sample

The committed Public Alpha sample is:

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
contiguous texture runs:     2
unbatched baseline draws:    7
ordered instanced draws:     2
```

### Public repository quality

- [x] MIT project license selected and root `LICENSE` added
- [x] third-party source dependency licenses reviewed in `THIRD_PARTY.md`
- [x] repository/history audit checks tracked artifacts, relative links, and high-confidence secret/private-path patterns
- [x] clean-clone README Quick Start passes
- [x] implemented versus planned capabilities are explicit
- [x] architecture and agent workflow are documented without requiring chat context
- [x] Public Alpha limitations are explicit
- [x] license-required release audit is enforced in CI
- [x] repository visibility changed to Public
- [x] tag `v0.1.0-alpha.1` exists
- [x] release tag contains the MIT `LICENSE`

## Validation evidence

Key release-facing checkpoints:

- PR #34 implementation head — CI #97 green
- PR #35 repository-quality candidate — CI #100 green
- PR #36 MIT/license-required release gate — CI #103 green
- PR #37 release-ready documentation — CI #105 green

The strict repository audit remains:

```powershell
./scripts/release_audit.ps1 -RequireLicense
```

## Third-party license policy

Public Alpha is primarily a source/repository release. Compiled binary attachments require a separate review of the exact resolved runtime dependency graph and required notices. Project-level MIT licensing does not replace third-party license obligations.

## Public Alpha limitations

See `PUBLIC_ALPHA_LIMITATIONS.md` for the platform, API, scene, renderer, capture, subsystem, performance, agent-integration, distribution, and security boundaries of this release.

Do not remove limitations merely to make the alpha appear broader than it is.

## Post-alpha direction

The first release is complete. Post-alpha work should extend the proven automation contract rather than replace it.

Priority areas include:

- protocol transport / MCP adapter over the existing agent facade
- practical deterministic asset caching/import workflow
- broader reproducible performance workloads before additional renderer complexity
- physics and semantic UI integration through structured deterministic contracts
- additional platform support after the Windows path remains stable
- portfolio/demo polish that makes the automation loop immediately visible to new visitors

Broad editor, render-graph, allocator, job-system, or engine-framework abstractions remain unjustified until a concrete measured need requires them.
