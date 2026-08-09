# Third-Party Dependencies and License Review

Trace2D does not vendor third-party source code in this repository. Dependencies are resolved through vcpkg manifest mode using the baseline pinned in `vcpkg.json` and CI.

This document records the current source dependency review. It is not a substitute for the license files installed by vcpkg when distributing compiled binaries.

## Direct manifest dependencies

| Dependency | Purpose in Trace2D | Upstream license | Trace2D note |
| --- | --- | --- | --- |
| SDL3 | Platform/window/input boundary and SDL GPU backend | zlib | Permissive; retain the upstream license notice when redistributing SDL binaries/source. |
| SDL3_shadercross | Runtime shader translation used by the renderer | zlib | Permissive; shader translation can involve additional upstream components depending on the selected backend/path. |
| stb | CPU-side texture image decoding in `engine/assets` | MIT or public-domain dedication at user option | Header-only decode dependency; Trace2D uses the MIT-compatible upstream terms and does not vendor the header. |
| toml++ | TOML parsing/serialization behind the scene-text boundary | MIT | Header/library dependency; retain the MIT copyright/license notice when required by redistribution form. |
| nlohmann/json | JSON-RPC/MCP parsing and serialization in `engine/mcp` only | MIT | Header-only transport dependency; it does not enter core/runtime/scene/input/UI/agent/testing public contracts or steady-frame simulation paths. |
| GoogleTest | Automated tests only | BSD-3-Clause | Test/development dependency; not part of Trace2D's intended runtime API surface. |

The vcpkg repository/tooling used to resolve these ports is MIT-licensed. The libraries supplied by vcpkg ports remain under their respective upstream licenses.

## SDL3_shadercross transitive dependencies

SDL3_shadercross's upstream documentation identifies SPIRV-Cross for SPIR-V translation and DirectX Shader Compiler components for DXIL paths. Exact transitive packages and notices can vary with the vcpkg port version, features, target triplet, and produced binary set.

For that reason Trace2D does not hard-code a possibly stale transitive-license table here. For any binary release, treat the resolved vcpkg installation as authoritative and collect the copyright/license material from:

```text
vcpkg_installed/<triplet>/share/<port>/copyright
```

The release process must review the complete resolved runtime distribution set, not only the direct entries in `vcpkg.json`.

## Binary distribution policy

The first `v0.1.0-alpha.1` release is primarily a source/repository release. Before attaching compiled runtime binaries to a GitHub release, perform a separate binary-distribution notice pass that:

1. records the exact vcpkg baseline, triplet, and resolved ports,
2. identifies which dynamic/static third-party binaries are actually shipped,
3. bundles or reproduces all notices required by those shipped components,
4. checks shadercross backend-specific dependencies that are packaged with the executable,
5. keeps Trace2D's MIT project license separate from third-party licenses.

## Project license compatibility

Trace2D is licensed under the MIT License. The direct dependency set reviewed above uses permissive licenses or permissive licensing options compatible with Trace2D's intended source distribution.

## Review status

Dependency review updated against:

- `vcpkg.json` baseline `d92484ed3c5020c6679d095ad3e5add907887b62`
- direct ports: `sdl3`, `sdl3-shadercross`, `stb`, `tomlplusplus`, `nlohmann-json`, `gtest`
- current Trace2D build linkage in the platform/render/assets/scene/MCP/test targets

Re-run this review whenever the vcpkg baseline, dependency list, features, triplet, or binary distribution contents change.
