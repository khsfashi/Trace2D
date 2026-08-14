# E1 — External Project, SDK Install/Package, and Environment Doctor

Issue: #70

E1 makes the E0 game/application boundary consumable as a normal external CMake project. A game no longer needs engine source copied into its project or undocumented edits inside `engine/`.

The representative proof remains `examples/e0_external_game`, but the directory now has its own `CMakeLists.txt`, `CMakePresets.json`, pinned `vcpkg.json`, project manifest, and startup content. The same game sources build in two modes:

```text
Trace2D source checkout
  -> add_subdirectory(examples/e0_external_game)

installed / extracted Trace2D SDK
  -> find_package(Trace2D CONFIG REQUIRED)
  -> Trace2D::Application / Agent / Platform / Render
```

The standalone mode is the supported external-consumer contract. The in-tree mode is a repository development convenience and must not be required by a real game.

## 1. Versioned project manifest

A Trace2D project root is identified by:

```text
trace2d.project.json
```

Manifest format version 1 owns only stable project/package facts that E1 can validate today:

```json
{
  "format_version": 1,
  "project_id": "trace2d.example.e0-external-game",
  "engine": { "minimum_version": "0.1.0" },
  "startup": { "scene": "content/scenes/main.trace2d.toml" },
  "content": { "root": "content" },
  "build": {
    "configure_preset": "windows-msvc",
    "build_preset": "windows-debug",
    "test_preset": "windows-debug",
    "headless_target": "trace2d_e0_external_headless",
    "windowed_target": "trace2d_e0_external_windowed"
  },
  "assets": { "texture": { "...": "..." } }
}
```

Rules:

- `project_id` is stable semantic identity and must not be derived from an absolute path or C++ allocation address.
- `startup.scene` and `content.root` are project-relative and may not escape the project root.
- the startup scene must exist beneath the declared content root.
- build/test preset names are references to versioned `CMakePresets.json`; they are not a second build database.
- E1 does not duplicate #97 `WorkSpec` intent/Definition-of-Done state inside the project manifest. A later project contract may reference a WorkSpec; the manifest does not mirror it.
- E1 deliberately does not invent runtime simulation/display/input fields that no runtime loader currently consumes. Those fields are added only when an owning implementation can validate and use them.

Project manifest parsing/discovery is setup/tooling work. The runtime frame loop does not parse this JSON.

## 2. Current project-owned texture/package policy

E1 makes the project/package ownership location explicit before later production work can accidentally hard-code an implicit strategy.

Manifest format 1 records:

```text
color_space       = srgb-authored
gpu_format_policy = rgba8-uncompressed-current-v1
mip_policy        = none-current-v1
max_size_policy   = reject-over-backend-limit
rescale_policy    = never-implicit
artifact_identity = sha256
```

This is an explicit **current limitation**, not a permanent production recommendation.

The current uncompressed RGBA8 path costs 4 bytes/texel before allocator/row/alignment overhead and does not receive block-compression savings. E1 therefore does not claim that arbitrary production textures should remain uncompressed RGBA8. Platform/package-specific GPU compression and mip generation/selection must be introduced by the owning asset/package stage with deterministic offline identity and measured quality/memory tradeoffs.

E1 also forbids silent rescaling. A source asset that exceeds the selected backend/package limit is rejected until an explicit authored/import policy says how to transform it. Any future derived texture artifact must have deterministic inputs/settings and SHA-256 identity so rebuilds can be compared.

This policy is separate from:

- #86 runtime CPU/GPU resource lifetime,
- #59 Sprite atlas semantics,
- future source-neutral Asset IR/import analysis.

## 3. Public CMake SDK surface

`TRACE2D_INSTALL_SDK=ON` is the default. Trace2D installs an exported CMake package under:

```text
<Trace2D root>/lib/cmake/Trace2D/
```

The supported E1 source-consumer targets are:

```text
Trace2D::Core
Trace2D::Input
Trace2D::Platform
Trace2D::Runtime
Trace2D::Scene
Trace2D::Particles
Trace2D::Assets
Trace2D::Render
Trace2D::Ui
Trace2D::Application
Trace2D::Agent
```

Headers for those targets are installed under `<Trace2D root>/include/trace2d/...`.

The SDK does **not** export repository test libraries, MCP implementation targets, benchmark tools, or internal executables as stable game-consumer targets. A consumer that needs game logic uses the public C++ headers/targets above instead of linking repository-internal target names such as `trace2d_application`.

Because Trace2D currently exports static libraries, a downstream final link still needs the imported targets used by those archives. `Trace2DConfig.cmake` therefore calls `find_dependency` for the direct external CMake packages required by the exported target graph:

```text
SDL3
SDL3_shadercross
nlohmann_json
tomlplusplus
```

The representative external project pins the same vcpkg baseline and declares those dependencies in its own `vcpkg.json`.

## 4. Source-tree vs installed/package semantics

### Repository development build

From a Trace2D checkout on the maintained Windows/MSVC path:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

This may see internal targets because it is building the engine repository itself. That is **not** the external-project compatibility contract.

### Install an SDK prefix

After the repository build:

```powershell
cmake --install build/windows-msvc --config Debug --prefix D:\Trace2D-sdk
```

The prefix contains public headers/static libraries, `Trace2DConfig.cmake`, version metadata, license/notices, this E1 contract, and the doctor script.

### Build a real external project

The external project needs only its own source/content plus the installed/extracted SDK and the pinned vcpkg toolchain:

```powershell
$env:VCPKG_ROOT = "D:\vcpkg"
$env:TRACE2D_ROOT = "D:\Trace2D-sdk"
cd examples/e0_external_game
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

The standalone `CMakeLists.txt` uses:

```cmake
find_package(Trace2D 0.1 CONFIG REQUIRED)
target_link_libraries(game PRIVATE Trace2D::Application)
```

No engine-source modification, hidden editor state, or path into `engine/*/src` is required.

### Create a package

Trace2D configures CPack ZIP packaging for the SDK install rules:

```powershell
cpack --config build/windows-msvc/CPackConfig.cmake -C Debug -G ZIP
```

Extracting that ZIP yields the same supported prefix layout. Set `TRACE2D_ROOT` to the extracted prefix and use the same external-project workflow.

E1 CI uses the **extracted CPack package** for the representative external configure/build/headless test, rather than letting the consumer see the Trace2D source target graph.

## 5. Environment doctor / preflight

The installed SDK includes:

```text
share/Trace2D/tools/trace2d_doctor.ps1
```

It can also be run from the repository `scripts/trace2d_doctor.ps1`.

Example:

```powershell
pwsh $env:TRACE2D_ROOT/share/Trace2D/tools/trace2d_doctor.ps1 -ProjectRoot .
```

The command emits JSON format version 1 to stdout and optionally `-OutputPath`. It discovers/validates the project and reports separate state for:

- project root and manifest,
- CMake availability/version,
- selected configure preset and maintained generator/toolchain identity,
- vcpkg root, expected baseline, actual checkout revision, and toolchain file,
- installed Trace2D SDK metadata/version/baseline,
- headless availability/eligibility/tested/supported state,
- actionable diagnostics.

The E1 doctor intentionally does **not** execute gameplay and therefore reports `headless.tested = false`. CI/test execution is the authority that later changes that evidence, not the doctor guessing from installed files.

Stable E1 process exit categories are:

```text
0   healthy external-consumer preflight
10  project root not found
11  manifest missing
12  manifest/schema/project-content invalid
20  CMake missing
21  CMake unavailable/unsupported query
30  vcpkg root/toolchain missing
31  vcpkg revision unverified or baseline mismatch
32  project vcpkg manifest invalid/missing baseline
40  configure preset contract missing/invalid
41  selected generator/toolchain unavailable
50  Trace2D SDK missing/incomplete/invalid
51  Trace2D SDK version/dependency baseline incompatible
```

A report may contain more than one diagnostic; the process exit code is the first blocking category encountered in the stable validation order. Diagnostic `category` strings carry the more specific classification.

The acceptance test covers both:

- a healthy installed/package environment, and
- a representative intentionally missing `VCPKG_ROOT`, which must be classified as `vcpkg.missing` / setup failure rather than causing source edits.

#78 extends maintained non-MSVC compiler/platform identity. #92 extends presentation GPU/backend/smoke eligibility and real-hardware evidence. E1 does not pre-claim those later states.

## 6. Availability, eligibility, tested, supported

These words are not synonyms.

- `available`: a tool/package/path exists and can be discovered.
- `eligible`: the known prerequisites for attempting the requested path are satisfied.
- `tested`: the path actually executed under a verification owner.
- `supported`: the project contract currently promises the path for the named maintained baseline.

The doctor may prove availability and local eligibility. It does not fabricate test evidence, real-GPU evidence, a license decision, or a human approval.

## 7. Package provenance and reproducibility baseline

`scripts/external_consumer_gate.ps1` writes `provenance.json` and package evidence containing:

- Trace2D source revision,
- configure preset and configuration,
- CMake/generator/platform/toolset/compiler-cache identity where available,
- pinned and actual vcpkg revision,
- canonical SHA-256 of the installed SDK tree,
- SHA-256 of the CPack ZIP,
- repeated install/package hashes,
- external consumer/doctor/test outcomes,
- explicit reproducibility boundaries.

The gate installs the same compiled tree twice and requires the canonical file-tree digests to match. It also generates the ZIP twice and records whether the archive bytes match. A ZIP mismatch is not silently normalized away; both hashes remain evidence and the known timestamp/archive-metadata boundary is recorded.

E1 does **not** claim independent compiler/linker bit-reproducibility yet. The current CI evidence repeats installation/package generation from one compiled build. Independent rebuild variance (object/archive timestamps, compiler/linker metadata, debug data), code signing, SBOM publication, and release artifact attestations remain explicit boundaries.

The CPack ZIP uploaded by CI is acceptance evidence, not a user-facing GitHub Release artifact. When Trace2D publishes a binary/SDK package for users to consume, release work must either add GitHub artifact/SBOM attestation or explicitly record why that release path cannot support it.

## 8. Dependency/license boundary

The SDK package installs:

```text
share/Trace2D/LICENSE
share/Trace2D/THIRD_PARTY_NOTICES.md
```

E1 does not bundle the resolved vcpkg third-party binaries/license corpus into the SDK ZIP. The external consumer resolves dependencies at the pinned baseline. Any combined binary distribution must collect the authoritative notices from the exact resolved vcpkg `share/<port>/copyright` files, including shadercross backend-specific transitive dependencies.

See `docs/THIRD_PARTY.md` and the packaged `THIRD_PARTY_NOTICES.md`.

## 9. Shader package policy

The current renderer still has runtime SDL_shadercross compilation paths. E1 does not add a generic asset/shader compiler merely to hide that fact.

Policy from this stage forward:

1. distributable shader artifacts should prefer deterministic offline build/validation and recorded artifact identity when the pinned SDL3/SDL_shadercross toolchain provides a clean maintained path,
2. generated shader/package artifacts must record their source/settings/toolchain identity and digest,
3. runtime-only shader compilation is an explicit current limitation, not evidence that offline packaging is unnecessary,
4. no generic asset compiler is introduced until an owning production requirement justifies it.

This preserves the issue's direction without inventing an unproven shader pipeline in E1.

## 10. Public extension and compatibility policy

External game modules extend Trace2D by compiling normal C++ against the supported `Trace2D::...` targets and implementing game-side types such as the E0 `Game`. They do not modify engine source and do not require a binary plugin ABI.

Compatibility rules:

- the supported source surface is the exported target list plus installed public headers; internal target names/source files are not API,
- Trace2D `0.x` is pre-1.0: source-breaking public API changes may occur at minor-version boundaries and must be documented; package compatibility is intentionally `SameMinorVersion`,
- compatible patch releases should not deliberately break the same-minor public source contract,
- once a mature stable API policy is justified, deprecation/removal windows can be strengthened rather than pretending 0.x already has a permanent ABI,
- authored formats/schemas remain separately versioned; a C++ library version bump does not silently migrate authored project/scene/asset data,
- a binary plugin ABI is explicitly out of scope until measured third-party distribution needs justify its cost.

#71 can therefore add game-defined typed components through the source-level project/game boundary without turning E1 into a reflection/plugin framework.

## 11. Performance and hot-path contract

E1 adds build/setup/package tooling, not frame-loop work.

Normal simulation/render frames do not:

- discover project roots,
- parse `trace2d.project.json`,
- execute PowerShell,
- query Git/vcpkg/CMake,
- hash package files,
- scan the filesystem for SDK metadata.

Those operations happen only on explicit setup/build/verification/package requests. Existing runtime allocation/resource/per-frame contracts are unchanged.

## 12. Acceptance mapping

E1 CI proves on a clean checkout that:

1. the E0 game remains buildable in the engine repository,
2. Trace2D installs an exported public SDK with stable `Trace2D::...` targets,
3. CPack produces a package carrying SDK metadata/license/notices,
4. a healthy project doctor returns versioned machine-readable success,
5. a deliberately missing vcpkg root returns a stable setup-failure classification,
6. the package is extracted and the E0 game configures without access to internal engine targets,
7. the external game builds, the windowed host compiles, and the headless test runs,
8. install/package SHA-256 and provenance/reproducibility evidence are recorded,
9. project texture/package policy and public source-compatibility ownership are explicit.

After #70 merges green, the fixed core lane advances to **#71 — deterministic scene hierarchy and typed authored component composition**.
