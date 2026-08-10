# Open-Source Game Production Foundation

Issue umbrella: **#67**.

This document defines the owner-approved phase that turns Trace2D from a strong deterministic/agent-verifiable engine core into an open-source 2D engine that a third party can actually use for a real game without editing engine internals.

The phase begins only after the complete Sprite program (#59). It deliberately precedes flagship proof #12, generic Mesh2D #60, and Spine compatibility #61.

## Why this phase exists

Trace2D already has unusually strong foundations for deterministic execution, semantic inspection, headless testing, structured Agent interaction, measurement-driven rendering, assets/UI/MCP, and the particle/Sprite direction.

The largest remaining architectural gap is different:

> Where does user game code live, how is a project identified and consumed, how is authored world state composed, and how does a third party build/run/test/package a real game through supported contracts instead of editing Trace2D itself?

An open-source engine is not complete merely because its subsystems are sophisticated. The user-facing project/game/world lifecycle must be explicit.

## Entry gate and global order

The owner-fixed core sequence is:

```text
#50 -> #51 -> #52 -> #53
 -> #59 complete Sprite program
 -> #67 game-production foundation
      #69 -> #70 -> #71 -> #72 -> #73 -> #74 -> #75 -> #76 -> #77 -> #78 -> #79
 -> #12 flagship external sample game
 -> #60 Mesh2D
 -> #61 Spine SP0 license gate
```

Do not begin #67 before #59 is complete. Do not begin #12, #60, or #61 before #67 is complete.

## Non-negotiable architecture rules

- User game code must not require modifying files under `engine/`.
- Prefer a simple C++ source/static-library consumer contract before inventing a binary plugin ABI.
- Do not introduce a generic ECS, reflection system, custom allocator framework, job system, render graph, material graph, visual scripting system, lock-free framework, or full graphical editor merely because a new subsystem needs composition.
- Authored content remains versioned, text-first, diffable, and deterministic where practical.
- Stable semantic identity beats pointers, allocation addresses, or unspecified container order.
- Structured engine-owned state/events remain the semantic correctness oracle. Pixels and audible output are presentation/QA evidence.
- Expensive inspection, migration, asset processing, capture, reporting, and Agent serialization remain explicit work rather than mandatory frame-loop work.
- Hot paths resolve strings/paths/resources during setup where practical and reuse persistent/capacity-managed state.
- Performance claims require workload evidence. Machine timing and deterministic structural metrics remain distinct.
- External dependencies require then-current license/distribution review before inclusion.

## Fixed child order

### #69 — E0: external Game/Application module boundary

Define the lifecycle and ownership relationship between Trace2D and user C++ game logic.

Required direction:

- a game lives outside engine internals,
- one explicit application/game lifecycle owns startup, fixed-step game update, shutdown, and access to required engine services,
- headless and windowed execution compose the same game logic,
- SDL/MCP/backend types do not leak into user gameplay contracts,
- an external minimal sample proves the boundary,
- no plugin ABI or generic service locator/reflection framework without demonstrated need.

This stage answers the most important open-source-engine question: **where does the game go?**

### #70 — E1: project manifest + external consumer build/install/package

Make a Trace2D game a first-class project.

Expected capabilities include:

- versioned project manifest,
- stable project ID,
- startup scene/content,
- fixed-step/display/asset/input configuration only where justified,
- documented external CMake consumer path,
- install/export/package support as appropriate,
- clean build/run/test commands discoverable from the project root,
- distribution notices for resolved dependencies,
- no source-tree-only undocumented shortcuts required for users.

Distribution-facing shader policy belongs here. The current renderer compiles embedded HLSL through SDL3_shadercross during setup. For distributable builds, prefer reproducible build/offline shader validation or artifacts when the pinned backend/toolchain supports a clean solution. Do not build a generic asset compiler solely to eliminate one runtime setup step.

### #71 — E2: scene hierarchy + typed authored component composition

Evolve Scene from identity + `Transform2D` into the coherent authored world model used by practical engine components.

Required direction:

- deterministic parent/child hierarchy,
- local/world transforms,
- cycle rejection,
- deterministic reparent/child ordering,
- stable semantic identity,
- finite typed authored component composition,
- versioned component serialization/validation,
- Agent visibility over authoritative hierarchy/components,
- subsystems reuse one world model rather than creating parallel entity graphs,
- prefab/reusable composition only after a real authored sample demonstrates the requirement.

A generic ECS/property bag is not a prerequisite.

### #72 — E3: semantic Input Actions + practical devices/text input

Keep the current deterministic low-level input foundation, but expose gameplay semantics rather than requiring direct physical-key coupling.

Expected capabilities:

- digital actions and analog axes,
- project-authored bindings/rebinding,
- gamepad buttons/axes and explicit deadzones,
- mouse position, delta, wheel,
- virtual Agent/test actions feed the same gameplay-facing path,
- deterministic frame/reset/replay behavior,
- proper text-input/IME boundary for production UI.

Normal per-frame action reads should be resolved/indexed and allocation-light after setup.

### #73 — E4: TileSet / TileMap / TileLayer

Tilemaps are a first-class 2D-engine capability and especially well suited to text/structured Agent authoring.

Expected capabilities:

- versioned `TileSet` identity and atlas/source metadata,
- deterministic map/layer/cell conventions,
- stable flip/rotation semantics,
- bounded dimensions/chunking chosen from real workloads,
- painter-order integration,
- culling/batching without per-tile draw calls,
- animated tiles only when justified,
- collision metadata handoff,
- semantic Agent query/assert by map/layer/cell/tile identity without pixel inference.

Do not represent a large map as one heap object/string per tile.

### #74 — E5: production UTF-8 font/text/localization foundation

The existing 5x7 ASCII-oriented path remains useful as a deterministic narrow fixture, but is not sufficient as the production text system.

Expected capabilities:

- project-relative font asset identity,
- UTF-8 text,
- glyph/font cache or atlas with explicit ownership,
- text measurement,
- wrapping/alignment,
- fallback policy,
- Korean/CJK-capable output,
- integration with E3 text/IME input,
- localization-facing layout/string hooks,
- kerning/shaping only through concrete requirements and mature reviewed dependencies.

Do not rerasterize unchanged glyph/text work every frame without reason.

### #75 — E6: practical deterministic UI hierarchy/layout/widgets

Preserve the current semantic `UiDocument` and Agent-first targeting while adding the minimum composition needed for real HUD/menu work.

Expected capabilities:

- UI hierarchy,
- anchors/pivot,
- resolution scaling,
- small deterministic layout primitives,
- margin/padding where justified,
- image/progress/scroll or similarly demonstrated widgets,
- keyboard/gamepad focus/navigation,
- production text from #74,
- layout results inspectable/assertable headlessly.

Do not clone DOM/CSS or introduce a browser layout engine.

### #76 — E7: Physics2D

Add practical 2D physics through a reviewed mature dependency such as Box2D when then-current API/license/performance requirements fit.

Expected engine contract:

- finite typed body/collider/trigger components,
- stable entity/fixture identity,
- collision layers/masks,
- raycast/overlap queries,
- fixed-step integration,
- structured contacts/triggers/query results,
- deterministic observable ordering where Trace2D owns the ordering,
- explicit determinism boundary for third-party floating-point simulation.

Do not claim cross-platform bit-identical physics unless separately proven.

### #77 — E8: Audio

Add a small practical 2D audio system.

Expected capabilities:

- project-relative audio clip identity/cache,
- source/playback state,
- play/stop/pause as required,
- volume, pitch, loop,
- small groups/buses if demonstrated,
- optional simple 2D pan/attenuation only when justified,
- semantic audio commands/events/state available in headless tests,
- physical audio output treated as presentation.

Tests should be able to prove that a clip/event was requested at the correct semantic point without recording speaker output.

### #78 — E9: Linux/compiler/toolchain hardening

Before stable external adoption, continuously validate more than one implementation environment.

Expected direction:

- Windows/MSVC remains supported,
- add Linux with Clang or GCC based on current dependency/backend support,
- headless engine and external consumer paths build/test in CI,
- backend-independent renderer code compiles/tests even when hosted interactive GPU presentation is unavailable,
- compiler-appropriate warnings,
- targeted ASan/UBSan where reliable,
- evaluate clang-tidy/static analysis and enable only high-signal checks,
- fix portability/UB rather than broadly suppressing diagnostics.

Do not add platforms as badges. Every supported platform needs ownership and validation.

### #79 — E10: save/persistence + authored schema migration

Close two long-term external-user lifecycle gaps.

#### Game persistence

Provide versioned settings/checkpoint/save-slot persistence without requiring generic reflection.

Required direction:

- stable user/game schema,
- semantic data rather than raw runtime handles/pointers,
- safe writes,
- structured corrupt/unsupported diagnostics,
- explicit save-version migration,
- deterministic serialization when ordering matters,
- headless save/reset/load verification.

#### Authored schema migration

Trace2D's alpha project/scene/component/UI/effect formats will evolve. External users need explicit migration rather than silent breakage.

Expected direction:

- declared migration policy,
- tool workflow equivalent to `trace2d migrate <project>` where useful,
- deterministic reviewable rewritten text,
- clear source/version/field diagnostics,
- no silent data loss,
- documented supported migration range.

## Existing implementation hardening explicitly tracked by this program

These observations are real but do **not** justify immediate speculative rewrites before the relevant stage:

### Renderer texture-handle tombstones

Current destroyed texture handles leave tombstones and are not recycled. This is safe for the present workload. Introduce generation-safe resource-handle reuse only if realistic long-running resource churn demonstrates unbounded table growth as a practical problem.

### Runtime shader compilation

The current renderer performs shadercross compilation during renderer setup rather than inside `RenderFrame`. This is already outside the frame hot path. #70 owns the question of whether distributable builds require offline/reproducible shader artifacts.

### ASCII UI raster path

The current dependency-free 5x7 path remains valuable for deterministic tests and minimal fallback behavior. #74 adds the production text system rather than deleting a useful test fixture.

### Schema migration

Current alpha formats may change incompatibly and have no migration tool. #79 must close this gap before Trace2D makes stronger external format compatibility promises.

## Flagship proof after #67

When #69-#79 are complete, #12 builds one deliberately small real game through the exact public/external contracts established by this program.

The game must not use sample-only engine shortcuts. It should prove a coherent subset of:

- external C++ game module,
- project manifest/consumer build,
- scene hierarchy/components,
- Input Actions,
- TileMap,
- real UTF-8 text/UI,
- Physics2D,
- semantic audio,
- persistence/migration,
- headless Agent QA,
- exact-frame visual capture,
- Windows plus the added non-MSVC platform/toolchain path.

Only after this proof should the fixed roadmap continue to Mesh2D #60 and Spine #61.

## Core continuation vs community contribution

The strict owner-fixed order exists so `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, and equivalent continuation prompts are deterministic.

That rule applies to the **core continuation lane**.

Independent open-source contributions may proceed when they are narrowly scoped fixes, tests, documentation, portability improvements, or isolated enhancements that:

- do not overlap/compete with an active core implementation,
- do not preempt a future owner-fixed subsystem design,
- do not add unreviewed dependencies/license obligations,
- do not violate determinism/performance/ownership contracts,
- do not silently alter product/architecture goals.

Architecture, product-goal, dependency/license, Particle backend, and Spine SP0 decisions remain explicit human/owner gates.

## Completion condition

#67 closes only when #69 through #79 are all merged green and repository docs/status agree on the resulting contracts.

The handoff after #67 is:

```text
#12 flagship external game proof
 -> #60 Mesh2D
 -> #61 Spine SP0
```
