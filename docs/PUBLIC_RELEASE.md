# Public Release Plan

## Target

The first public milestone is:

```text
v0.1.0-alpha.1 — Public Alpha
```

The repository should remain private until this release gate is satisfied.

Public Alpha is a **proof of architecture and workflow**, not a claim that Trace2D is a feature-complete engine.

## What the release must prove

A coding agent should be able to use normal source-control and command-line tooling to perform this complete loop without relying on a graphical editor:

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

If this loop does not work end-to-end, the repository is not ready for Public Alpha even if individual subsystems look impressive.

## Required release gates

### Gate A — Reproducible project foundation

- [ ] clean Windows checkout configures successfully
- [ ] Debug build succeeds
- [ ] automated tests pass
- [ ] CI is green on `main`
- [ ] documented build uses the same source-of-truth CMake project as CI
- [ ] vcpkg baseline/dependency resolution is reproducible
- [ ] generated build artifacts are excluded from Git

### Gate B — Deterministic runtime

- [ ] headless runtime path exists
- [ ] windowed and headless modes share authoritative simulation code
- [ ] fixed simulation timestep is explicit
- [ ] tests/agents can advance exactly N frames without sleeping
- [ ] simulation frame number is observable
- [ ] deterministic seed/state reset is available
- [ ] repeated deterministic tests reproduce expected state

### Gate C — Text-authored scene and stable identity

- [ ] minimal scene is authored as readable text
- [ ] authored entities have stable semantic identity
- [ ] transform/name/tags are available
- [ ] invalid scene data reports actionable errors
- [ ] serialization/output order is stable enough for useful Git diffs
- [ ] stale runtime handles cannot silently access reused entities

### Gate D — Structured observability

- [ ] active scene/runtime frame can be inspected
- [ ] entity state can be inspected without pixel parsing
- [ ] machine-facing output is structured
- [ ] semantic selectors support at least authored ID and tags
- [ ] query result ordering is deterministic where order is observable
- [ ] ambiguous/invalid queries fail explicitly

Minimum examples should be conceptually equivalent to:

```text
trace2d inspect ... --json
trace2d query '#player' --json
trace2d query 'tag:enemy' --json
```

Exact CLI syntax may evolve before release.

### Gate E — Virtual input and gameplay QA

- [ ] physical and virtual input feed the same engine-level input state
- [ ] tests can schedule press/release against simulation frames
- [ ] gameplay scenario can load/reset deterministically
- [ ] assertion API can read semantic entity/component state
- [ ] failed assertion records expected/observed values
- [ ] failed assertion records frame and deterministic seed/reproduction context
- [ ] gameplay QA runs in CI without human interaction

### Gate F — Minimal 2D rendering and visual capture

- [ ] SDL3 GPU renderer can display a small sprite scene
- [ ] orthographic 2D camera exists
- [ ] multiple sprites use a reasonable batching baseline
- [ ] simulation remains authoritative without rendering
- [ ] an image can be captured at an explicitly requested simulation frame
- [ ] capture failure is machine-detectable

No advanced lighting, PBR, scene editor, or renderer framework is required.

### Gate G — End-to-end sample

The repository must include one deliberately tiny sample designed to prove automation rather than content-production scale.

The sample should contain at minimum:

- [ ] player-like controlled entity
- [ ] at least one other semantic entity
- [ ] movement driven by engine input
- [ ] observable state changed by gameplay
- [ ] at least one deterministic automated gameplay test
- [ ] at least one rendered screenshot/capture workflow

A good release demo should be able to show something equivalent to:

```text
load sample
query #player
press Right
step 120
release Right
assert #player.position.x > initial_x
capture frame_120.png
```

The exact game genre is intentionally open until the sample implementation phase.

### Gate H — Public repository quality

Before changing visibility to Public:

- [ ] choose and add a repository license
- [ ] review third-party dependency licenses
- [ ] verify no credentials, private endpoints, personal paths, or secrets are present in Git history
- [ ] README has a working quick start
- [ ] README clearly labels implemented versus planned capabilities
- [ ] architecture overview is understandable without chat context
- [ ] contributor/agent workflow is documented
- [ ] Public Alpha limitations are explicit
- [ ] all links in top-level docs resolve
- [ ] latest `main` CI is green

## What is intentionally not required

These features are valuable later but are not Public Alpha blockers:

- MCP integration
- JSON-RPC specifically (another clean transport may be selected later)
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

Trace2D is performance-conscious, but Public Alpha does not need arbitrary FPS marketing numbers.

Before release:

- [ ] record the test machine/environment for any published benchmark
- [ ] keep at least one reproducible baseline measurement for a meaningful runtime or rendering path
- [ ] do not publish synthetic improvement percentages without stored before/after data
- [ ] inspect hot-path allocations before claiming allocation-free behavior
- [ ] document known performance limitations honestly

Performance work should demonstrate engineering discipline rather than chase premature complexity.

## Release sequence

When all mandatory gates are satisfied:

1. finish the Public Alpha tracking issue
2. ensure all release changes are merged to `main`
3. run clean CI on `main`
4. perform a local clean-clone quick-start verification
5. add/finalize license and third-party notices
6. update `PROJECT_STATUS.md` to Public Alpha ready
7. create the `v0.1.0-alpha.1` tag/release
8. change repository visibility to Public
9. verify README/release assets as an unauthenticated viewer
10. create post-alpha issues from known limitations instead of hiding them

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
