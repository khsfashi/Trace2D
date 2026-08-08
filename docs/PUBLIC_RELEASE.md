# Public Release Plan

## Target

The first public milestone is:

```text
v0.1.0-alpha.1 — Public Alpha
```

The repository should remain private until every mandatory repository-quality gate is satisfied.

Public Alpha is a **proof of architecture and workflow**, not a claim that Trace2D is a feature-complete engine.

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

That technical loop is implemented. The remaining Public Alpha work is repository/release quality.

## Required release gates

### Gate A — Reproducible project foundation

- [ ] clean Windows checkout Quick Start job passes on the release-quality candidate
- [x] Debug build succeeds in hosted CI
- [x] automated tests pass in hosted CI
- [ ] latest release-candidate `main` CI is green
- [x] documented build and CI use the same root CMake project and presets
- [x] vcpkg baseline/dependency resolution is pinned by repository state
- [x] generated build artifacts are excluded from Git policy

The README Quick Start is now executable CI policy: `clean-clone-quick-start` uses `windows-2022`, installs the exact repository-pinned vcpkg baseline, then runs `windows-msvc`, `windows-debug`, and `ctest --preset windows-debug` from a clean checkout.

### Gate B — Deterministic runtime

- [x] headless runtime path exists
- [x] windowed and headless modes share authoritative simulation code
- [x] fixed simulation timestep is explicit
- [x] tests/agents can advance exactly N frames without sleeping
- [x] simulation frame number is observable
- [x] deterministic seed/state reset is available
- [x] repeated deterministic tests reproduce expected state

### Gate C — Text-authored scene and stable identity

- [x] minimal scene is authored as readable text
- [x] authored entities have stable semantic identity
- [x] transform/name/tags are available
- [x] invalid scene data reports actionable errors
- [x] serialization/output order is deterministic for useful Git diffs
- [x] stale runtime handles cannot silently access reused entities

### Gate D — Structured observability

- [x] active scene/runtime frame can be inspected
- [x] entity state can be inspected without pixel parsing
- [x] machine-facing output is structured
- [x] semantic selectors support authored ID, names/tags, and component queries
- [x] query result ordering is deterministic where order is observable
- [x] ambiguous/invalid queries fail explicitly

Implemented CLI surfaces include:

```text
trace2d inspect ... --json
trace2d query ... --selector "#player" --json
```

### Gate E — Virtual input and gameplay QA

- [x] physical and virtual input feed the same engine-level input state
- [x] tests can schedule press/release against simulation frames
- [x] gameplay scenario can load/reset deterministically
- [x] assertion API can read semantic entity/component state
- [x] failed assertion records expected/observed values
- [x] failed assertion records frame, deterministic seed, input, runtime, and entity context
- [x] gameplay QA runs in CI without human interaction

### Gate F — Minimal 2D rendering and visual capture

- [x] SDL3 GPU renderer can display a small sprite scene
- [x] orthographic 2D camera exists
- [x] measured contiguous same-texture batching baseline is implemented
- [x] simulation remains authoritative without rendering
- [x] an image can be captured at an explicitly requested simulation frame
- [x] capture failure is machine-detectable

No advanced lighting, PBR, scene editor, or renderer framework is required.

### Gate G — End-to-end sample

The committed Public Alpha sample contains:

- [x] `#player` semantic controlled entity
- [x] additional semantic/visible entities
- [x] movement driven by engine input
- [x] observable state changed by gameplay
- [x] deterministic automated gameplay test
- [x] rendered exact-frame capture workflow
- [x] complete edit -> build -> inspect -> query -> input -> assert -> capture documentation

Default sample contract:

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

### Gate H — Public repository quality

Before changing visibility to Public:

- [ ] choose and add a repository license
- [x] review and document third-party source dependency licenses (`THIRD_PARTY.md`)
- [ ] release audit passes over current tree and fetched Git history with no high-confidence secret/private-path findings
- [ ] README clean-checkout Quick Start job passes
- [x] README clearly labels implemented versus planned capabilities
- [x] architecture overview is understandable without chat context
- [x] contributor/agent workflow is documented
- [x] Public Alpha limitations are explicit (`PUBLIC_ALPHA_LIMITATIONS.md`)
- [ ] repository-relative Markdown link audit passes
- [ ] latest release-candidate `main` CI is green

The repeatable audit command is:

```powershell
./scripts/release_audit.ps1
```

After the project license is selected, the release-candidate form is:

```powershell
./scripts/release_audit.ps1 -RequireLicense
```

The project-license decision is intentionally owner-controlled. `LICENSE_DECISION.md` documents the MIT vs Apache-2.0 tradeoff without silently selecting one.

## Third-party license policy

Direct manifest dependencies are reviewed in `THIRD_PARTY.md`.

Public Alpha is primarily a source/repository release. If compiled binaries are attached later, the binary release must separately review the exact resolved vcpkg runtime graph and carry the required notices from the installed ports. Source-release review is not treated as a blanket binary-distribution approval.

## Public Alpha limitations

`PUBLIC_ALPHA_LIMITATIONS.md` is part of the release surface. It explicitly records platform, API, scene, renderer, capture, subsystem, performance, agent-integration, distribution, and security-boundary limitations.

Do not remove limitations merely to make the alpha appear broader than it is.

## What is intentionally not required

These are post-alpha features, not release blockers:

- MCP integration
- JSON-RPC specifically
- graphical scene editor
- scripting language
- full ECS
- custom memory allocator framework
- job/work-stealing system
- advanced animation graph
- advanced UI toolkit
- networking
- audio engine
- PBR or dynamic lighting
- navigation/pathfinding
- broad asset import pipeline
- Linux/macOS support
- mobile support

The release must resist scope growth. A small, complete automation loop is more valuable than a large half-finished engine.

## Release-quality performance evidence

Trace2D is performance-conscious, but Public Alpha does not publish arbitrary FPS or cross-engine marketing claims.

- [x] representative renderer workload is committed and reproducible
- [x] before/after structural draw counts are stored: 7 visible sprite draws -> 2 ordered instanced draws
- [x] no synthetic percentage improvement is claimed
- [x] hot-path design documents state allocation/resource-lifetime constraints
- [x] known performance limitations are explicit

Any future wall-clock/FPS benchmark must additionally record the exact machine, GPU, driver, build configuration, workload, and capture method.

## Release sequence

When all mandatory gates are satisfied:

1. merge repository-quality changes to `main`,
2. choose/add the project license and enable `-RequireLicense` in the release audit,
3. run clean release-candidate CI on `main`,
4. mark `PROJECT_STATUS.md` and Issue #14 release-ready,
5. create `v0.1.0-alpha.1`,
6. change repository visibility to Public,
7. verify README, release, links, and license as an unauthenticated viewer,
8. create post-alpha issues from known limitations instead of hiding them.

## After Public Alpha

After the first public release, priorities shift toward proving that the architecture scales beyond the minimal demo:

- richer practical 2D engine features
- asset caching/import workflow
- physics and semantic UI integration
- broader performance benchmarks
- protocol transport and MCP adapter
- additional agent clients
- polished portfolio demonstration

Those features should extend the already-proven automation contract rather than replace it.
