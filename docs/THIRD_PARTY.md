# Third-Party Dependencies and License Review

Trace2D does not vendor third-party source code in this repository. Dependencies are resolved through vcpkg manifest mode using the baseline pinned in `vcpkg.json` and CI.

This document records the current source dependency review. It is not a substitute for the license files installed by vcpkg when distributing compiled binaries.

## Direct manifest dependencies

| Dependency | Purpose in Trace2D | Upstream license | Trace2D note |
| --- | --- | --- | --- |
| FreeType | Production font face access, glyph metrics, and CPU glyph rasterization in `engine/text` | FreeType License (FTL) or GNU GPL v2 | Trace2D uses the permissive FTL option. Public Trace2D headers do not expose FreeType types; the exported static `Trace2D::Text` target requires FreeType discovery downstream. F0 disables vcpkg default features because WOFF2/bzip2/PNG embedded-bitmap integrations are not required by the accepted TTF/OTF outline boundary. |
| SDL3 | Platform/window/input boundary and SDL GPU backend | zlib | Permissive; retain the upstream license notice when redistributing SDL binaries/source. |
| SDL3_shadercross | Runtime shader translation used by the renderer | zlib | Permissive; shader translation can involve additional upstream components depending on the selected backend/path. |
| stb | CPU-side texture image decoding in `engine/assets` | MIT or public-domain dedication at user option | Header-only decode dependency; Trace2D uses the MIT-compatible upstream terms and does not vendor the header. |
| toml++ | TOML parsing/serialization in Scene/UI/Particle/Agent authored/tooling boundaries | MIT | Header/library dependency; retain the MIT copyright/license notice when required by redistribution form. |
| nlohmann/json | JSON processing in `engine/assets` and `engine/mcp` | MIT | Header-only dependency; public Trace2D headers do not expose nlohmann types, but the exported static `Trace2D::Assets` target requires the CMake package to be discoverable for downstream static-link metadata. |
| GoogleTest | Automated tests only | BSD-3-Clause | Test/development dependency; not part of Trace2D's intended runtime API surface. |

The vcpkg repository/tooling used to resolve these ports is MIT-licensed. The libraries supplied by vcpkg ports remain under their respective upstream licenses.

## Font/text dependency boundary

#74 F0 uses FreeType only for the lower-level job FreeType itself owns: loading a font face, resolving glyphs/metrics, selecting size, and rasterizing glyph coverage. FreeType is not treated as a text-layout/shaping engine. Complex shaping, script clustering, bidirectional layout, and similar higher-level behavior remain deferred until a concrete #74 requirement justifies a separately reviewed dependency such as HarfBuzz.

The production font path opens canonical font bytes already owned by Trace2D's typed resource registry. It does not discover OS fonts or pass project file paths to FreeType during steady-state text work. Prepared faces retain the generation-safe FontResource so the in-memory bytes remain valid for the face lifetime.

The pinned vcpkg baseline resolves FreeType 2.14.3. Trace2D explicitly sets `default-features: false` for both the engine and representative external consumer. Support for WOFF2, bzip2-compressed fonts, PNG embedded bitmaps, or other optional integrations must be promoted only by a concrete asset requirement plus dependency/license review; F0 does not pull them in speculatively.

## SDL3_shadercross transitive dependencies

SDL3_shadercross's upstream documentation identifies SPIRV-Cross for SPIR-V translation and DirectX Shader Compiler components for DXIL paths. Exact transitive packages and notices can vary with the vcpkg port version, features, target triplet, and produced binary set.

For that reason Trace2D does not hard-code a possibly stale transitive-license table here. For any binary release, treat the resolved vcpkg installation as authoritative and collect the copyright/license material from:

```text
vcpkg_installed/<triplet>/share/<port>/copyright
```

The release process must review the complete resolved runtime distribution set, not only the direct entries in `vcpkg.json`.

## E1 SDK/package notice policy

#70 introduces an installable/CPack Trace2D SDK contract. The SDK package carries Trace2D's `LICENSE`, `THIRD_PARTY_NOTICES.md`, SDK metadata and the E1 external-project contract.

The E1 SDK package does not intentionally bundle the resolved vcpkg third-party runtime/development binaries or their complete license corpus. An external game resolves the required packages through its own manifest at the same pinned baseline. The packaged notice therefore stays attached to the SDK while the exact resolved `vcpkg_installed/<triplet>/share/<port>/copyright` material remains authoritative for a combined downstream binary distribution.

The CI CPack ZIP created by the E1 acceptance gate is provenance/consumer evidence rather than a user-facing GitHub Release binary. Publishing a combined executable/runtime SDK remains a separate binary-distribution event and triggers the full notice pass below.

## Binary distribution policy

The first `v0.1.0-alpha.1` release is primarily a source/repository release. Before attaching compiled runtime binaries or a combined dependency-bearing SDK to a GitHub release, perform a separate binary-distribution notice pass that:

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
- direct ports: `freetype` (default features disabled), `sdl3`, `sdl3-shadercross`, `stb`, `tomlplusplus`, `nlohmann-json`, `gtest`
- current Trace2D build linkage in platform/render/assets/text/scene/UI/particle/Agent/MCP/test targets
- #70 exported static SDK dependency-discovery contract
- #74 F0 FreeType face/metrics/rasterization boundary

Re-run this review whenever the vcpkg baseline, dependency list, features, triplet, exported target graph, or binary distribution contents change.