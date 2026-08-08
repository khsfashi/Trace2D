# Public Alpha Limitations

`v0.1.0-alpha.1` is a proof that Trace2D's agent-first development loop works end to end. It is not a production-ready general-purpose game engine.

The following limitations are intentional and must remain visible in the first public release.

## Platform and toolchain

- Windows x64 is the only supported Public Alpha development/runtime target.
- The documented developer quick start targets Visual Studio 2022 with the Desktop development with C++ workload.
- Linux, macOS, mobile, consoles, and Web are not supported release targets yet.
- Hosted CI validates build/test contracts; interactive GPU presentation is not treated as a hosted-runner requirement.

## API and compatibility

- The C++ API, scene schema, CLI surface, and file formats are alpha contracts and may change incompatibly before a stable release.
- There is no binary/plugin ABI compatibility promise.
- There is no migration tooling for future scene-schema changes yet.

## Scene and gameplay scope

- The text-authored scene format is deliberately small and currently centers on stable semantic identity, names/tags, and `Transform2D` state required by the automation loop.
- There is no graphical scene editor, prefab workflow, full ECS, scripting language, animation graph, production asset database, or broad import pipeline.
- The Public Alpha sample is a deterministic validation workload, not a representative game-content production sample.

## Rendering

- Rendering is a minimal SDL3 GPU 2D path, not a general material/render-graph architecture.
- Sprite order remains caller/painter authoritative.
- Batching combines only contiguous visible sprites that already share a texture; Trace2D intentionally does not globally sort by texture.
- There is no texture-atlas system, bindless/GPU-driven renderer, advanced lighting, PBR, post-processing stack, or order-independent transparency solution.
- Culling uses the current inclusive 2D AABB rule; there is no spatial acceleration structure yet.
- Shader translation currently uses SDL3_shadercross at runtime rather than a complete offline shader asset pipeline.

## Visual capture

- Deterministic visual QA currently produces a dependency-free 32-bit BMP artifact.
- Capture is explicitly requested and synchronized; it is not a high-throughput video/frame-streaming system.
- Rendering pixels are QA evidence only. Authoritative gameplay assertions use structured simulation state rather than image inference.
- Engine simulation determinism does not imply pixel-identical output across every GPU, driver, or future backend.

## Physics, UI, audio, and networking

The first alpha does not provide production implementations of:

- physics/Box2D integration,
- semantic UI/runtime UI toolkit,
- audio,
- networking,
- navigation/pathfinding.

These are post-alpha breadth items and are not hidden release blockers.

## Performance claims

- Trace2D applies performance-oriented rules such as persistent renderer resources, no renderer-owned per-frame sprite-list growth, allocation-free culling/batching measurement, and evidence-driven batching.
- The Public Alpha sample demonstrates a specific draw-call reduction from seven visible sprites to two contiguous same-texture instanced draws.
- That sample is not a general FPS, scalability, or production workload benchmark.
- No arbitrary cross-engine performance claim should be inferred from the sample.

## Agent integration

- The protocol-independent inspection/query/testing facade is implemented.
- MCP or another remote protocol adapter is not implemented in Public Alpha.
- The intended workflow currently assumes normal source-control, command-line, process, and file access by the coding agent.

## Distribution and licensing

- `v0.1.0-alpha.1` is intended primarily as a source/repository release.
- Third-party source dependency licensing is reviewed in `THIRD_PARTY.md`.
- Compiled binary distribution requires an additional resolved-dependency notice pass before binaries are attached to a release.
- The Trace2D project license must be selected before repository visibility changes to Public.

## Security boundary

- Trace2D is developer tooling, not a sandbox for untrusted native code or untrusted projects.
- Public Alpha does not claim adversarial-input hardening, process isolation, or a plugin permission model.

These limitations should become follow-up issues only when they are concrete next priorities. They should not be expanded into release scope merely to make the first alpha appear more feature-complete.
