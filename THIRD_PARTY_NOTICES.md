# Trace2D SDK third-party notices

This notice file is installed and packaged with the Trace2D C++ SDK.

Trace2D does not vendor third-party source code in this repository. The supported E1 external-consumer path resolves dependencies through vcpkg manifest mode at the exact `builtin-baseline` recorded in the installed `share/Trace2D/trace2d.sdk.json` and in the external project's `vcpkg.json`.

Direct dependencies relevant to the exported SDK include:

- **FreeType** — dual-licensed under the FreeType License (FTL) or GNU GPL v2; Trace2D uses the permissive FTL option for font face access, metrics, and glyph rasterization.
- **SDL3** — zlib license; platform/window/input and SDL GPU backend dependency.
- **SDL3_shadercross** — zlib license; shader translation dependency. Its resolved backend/transitive notices depend on the selected vcpkg triplet/features.
- **toml++** — MIT license; TOML parsing/serialization used by exported engine libraries.
- **nlohmann/json** — MIT license; JSON processing used by exported asset code.

Trace2D's build also uses `stb` for asset decoding and GoogleTest for repository tests. They are not imported targets required by the installed external-consumer CMake package, but their licensing remains documented in `docs/THIRD_PARTY.md`.

## What the E1 SDK package contains

The E1 CPack ZIP contains Trace2D headers, Trace2D static libraries, CMake package metadata, Trace2D's MIT `LICENSE`, this notice, SDK metadata, the external-project contract, and the environment doctor.

It does **not** intentionally bundle the third-party runtime/development dependency binaries or their full resolved license corpus. The external consumer resolves those packages through its pinned vcpkg manifest.

The authoritative license/copyright material for the exact resolved dependency set is the installed vcpkg material under:

```text
vcpkg_installed/<triplet>/share/<port>/copyright
```

Anyone redistributing a combined executable, third-party binary set, or other binary release must collect and ship the notices required by the exact resolved ports/features/triplet. In particular, SDL3_shadercross may introduce backend-specific transitive components that must be reviewed from the resolved installation rather than from a hard-coded list.

See `docs/THIRD_PARTY.md` for the repository dependency review and binary-distribution policy.
