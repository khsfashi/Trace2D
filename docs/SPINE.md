# Spine Compatibility and License Gate

Status: **planned compatibility target; intentionally not included**

Operational issue: GitHub Issue #61.

Trace2D intends to support Spine as an optional compatibility integration after the native Sprite pipeline (#59) and generic Mesh2D foundation (#60) are complete. Spine is not part of Trace2D's native animation architecture and no Spine Runtime code or dependency is currently authorized for inclusion.

This document records the owner-approved technical direction and the mandatory license gate. It is not legal advice; the actual integration must follow the then-current official Esoteric Software license terms and an explicit repository-owner decision.

## 1. Current decision

Until SP0 is explicitly approved:

- do not vendor or copy `spine-cpp` into Trace2D,
- do not add the Spine Runtime as a package/submodule/downloaded build dependency,
- do not publish a Trace2D binary containing Spine Runtime code,
- do not add Spine-derived implementation source,
- do not claim shipped Spine support,
- do not infer permission merely because the Spine Runtime source is publicly visible.

Trace2D may continue building generic native infrastructure independently useful without Spine, including SpriteAsset, SpriteAnimator2D, TexturedMesh2D, blend modes, dynamic geometry buffers, semantic animation inspection, and renderer QA.

## 2. Why Spine is separated from the MIT core

Trace2D is an MIT-licensed open-source engine. Spine Runtimes use their own license and are intended to be integrated subject to Esoteric Software's licensing conditions. A game engine/SDK/toolkit distribution raises different questions from a single finished game product because downstream developers may use the runtime to create new products.

Therefore the architectural goal is:

```text
Trace2D native core / Sprite / Animation / Mesh2D
                 MIT
                  |
                  v
        optional compatibility boundary
                  |
                  v
        official Spine Runtime, if authorized
          separate license obligations
```

The optional boundary must not make users believe the MIT license grants rights to Spine Runtime code.

## 3. SP0 — mandatory HUMAN license gate

Issue #61 is blocked at SP0 until the repository owner records an explicit decision after obtaining sufficient confirmation for the intended model.

The confirmation should address at least:

1. Trace2D is a public MIT C++ game engine.
2. Spine compatibility would be optional rather than required for the engine core.
3. Whether public adapter/integration source may be distributed and under what terms.
4. Whether the official `spine-cpp` source may be referenced as a user-supplied dependency instead of vendored.
5. Whether automated dependency fetching is permitted/desirable and what user license disclosures are required.
6. Requirements for distributing source builds.
7. Requirements for distributing prebuilt binaries containing Spine Runtime code.
8. Requirements for GitHub Actions/CI compilation/testing involving the Spine Runtime.
9. Required copyright/license notices and documentation text.
10. Whether each downstream developer using the integration must possess an appropriate Spine Editor license.
11. Version/export compatibility and pinning requirements between Spine Editor data and the runtime.
12. Any restrictions on modifying or redistributing runtime/integration code.

Prefer a concrete written answer from official Esoteric Software sources/support for the exact open-source-engine scenario before approval.

## 4. Agent behavior at SP0

When the owner-fixed sequence reaches #61 and SP0 is not approved, a coding agent must stop implementation and report one clear user action instead of guessing:

```text
Human gate reached: Spine runtime-license integration approval is required.
No Spine code/dependency has been added.
Record the approved integration/distribution model before SP1 can begin.
```

The agent must not skip ahead to SP1-SP4 and must not silently move to an unrelated large feature unless the owner explicitly changes the execution order.

## 5. Planned architecture after approval

Only if SP0 is approved, proceed in this fixed order.

### SP1 — optional official `spine-cpp` adapter

Goals:

- use the official runtime rather than reimplementing Spine format/semantics,
- establish an explicit build option/integration boundary,
- pin/validate a supported runtime/export version according to official compatibility guidance,
- keep Spine types out of Trace2D core public APIs where practical,
- expose required third-party license/notices in the repository and binary-distribution workflow,
- fail clearly when Spine support is requested without the required dependency/license setup.

The exact dependency mechanism—user-supplied path, package, source checkout, or another approved form—must be chosen by the SP0 decision rather than assumed here.

### SP2 — loading and Mesh2D rendering

Integrate supported Spine skeleton/atlas data through Trace2D-owned boundaries and the generic Mesh2D renderer from #60.

The renderer path is expected to support the textured indexed geometry Spine needs, including as required by the selected official runtime version:

- positions,
- UVs,
- indices,
- vertex color,
- texture/page selection,
- blend modes,
- clipping/masking behavior,
- stable draw order.

Do not force arbitrary deformable geometry through the SpriteRenderer quad/9-slice path.

### SP3 — semantic animation state

Expose protocol-independent semantic state appropriate to the supported Spine Runtime surface, such as:

- active animations,
- animation tracks,
- playback time,
- loop state,
- queued/mixed animation state,
- skins,
- slots,
- attachments,
- events,
- selected bone/pose observations when they are useful and stable.

The official Spine runtime remains the implementation authority for Spine-specific semantics. Trace2D adds stable observation/action boundaries rather than duplicating the runtime.

### SP4 — Agent/MCP QA and workloads

Add Agent operations/assertions over the protocol-independent semantic surface, then expose them through CLI/MCP adapters where useful.

Headless QA should verify semantic state without needing pixels. Windowed Mesh2D capture remains visual evidence for deformation, clipping, blend and ordering behavior.

Provide reproducible workloads separating:

- Spine runtime update cost,
- derived mesh extraction cost,
- CPU upload/submission cost,
- draw calls/batch breaks,
- retained dynamic buffer capacities,
- GPU timing where measured.

## 6. Determinism and gameplay authority

Sprite-frame animation can target very strict deterministic timing/state contracts. Spine deformation involves floating-point interpolation and mesh output from a third-party runtime, so Trace2D must not make an unsupported promise of universal bit-identical deformed vertices across every platform/compiler/driver.

The intended hierarchy is:

```text
semantic state/events/tracks
  -> deterministic according to the supported runtime + Trace2D timing contract where proven

deformed CPU/mesh values
  -> strict on proven configurations or compared with documented numeric tolerance

rendered GPU pixels
  -> visual/conformance evidence, not universal gameplay truth
```

If gameplay uses bones/sockets/bounding attachments, those engine-facing semantics must be exposed explicitly. Gameplay should not infer authority from rendered mesh pixels.

## 7. Native Trace2D animation remains independent

Spine support must never become a prerequisite for ordinary Trace2D sprite animation.

The native path remains:

```text
Trace2D SpriteAsset
 -> SpriteAnimator2D
 -> Agent exact-frame QA
 -> production SpriteRenderer
```

The generic geometry path remains:

```text
Trace2D TexturedMesh2D
 -> renderer
```

Spine, if approved, becomes one external producer/consumer of those generic presentation/observation capabilities rather than the architecture around which they are designed.

## 8. User-facing documentation requirement after approval

If Trace2D later ships Spine compatibility, documentation must clearly distinguish:

- Trace2D's MIT license,
- the separate Spine Runtime license,
- what users must install/provide,
- any required Spine Editor license for developers using the integration,
- supported Spine/runtime versions,
- whether prebuilt Trace2D binaries include or exclude the Spine Runtime.

No Quick Start should accidentally install or enable a separately licensed runtime without making those conditions visible.

## 9. Completion rule

Trace2D may claim **shipped Spine compatibility** only when:

- SP0 has an explicit recorded owner approval,
- required license notices/distribution rules are implemented,
- SP1-SP4 are complete with tests/workloads,
- supported version constraints are documented,
- the normal MIT-native Trace2D build remains usable without Spine.

Before that point, the correct claim is:

> Spine compatibility is planned but intentionally not included pending explicit runtime-license integration review.
