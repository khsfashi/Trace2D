# Production Architecture Contracts

Status: **owner-approved architecture freeze**

Tracking issue: **#85**.

This document freezes the production-engine contracts that must be respected by the future Sprite and game-production programs. It exists because Trace2D's deterministic/Agent-verifiable alpha core is already deeper than several practical engine integration layers, and implementing those layers independently would create avoidable rework.

This is **not** an instruction to implement every subsystem immediately. The active core order still starts with the current particle work. The purpose of this document is to define the semantic seams now so later implementations can remain small, typed, measurable and compatible.

## 1. Ordering decision

The current particle sequence is preserved:

```text
#52 explicit GPU particle runtime
 -> #53 CPU/GPU conformance, workloads and guidance
```

The architecture freeze in #85 is a completed contractual predecessor of #59. It does not insert a large implementation program between particles and Sprite.

The intended future order after #59 is expanded from the existing game-production foundation to include the missing practical layers:

```text
#59 complete Sprite program
 -> #69 Game/Application boundary
 -> #70 Project manifest + external consumer/package
 -> #71 Scene hierarchy + engine/game typed components
 -> #86 unified typed resource lifecycle
 -> #87 reusable scene templates + deterministic world lifecycle
 -> #88 Camera2D + Viewport2D
 -> #72 Input Actions + devices/text/IME
 -> #73 TileSet/TileMap
 -> #74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI hierarchy/layout/widgets
 -> #89 Material2D + Shader2D
 -> #90 deterministic resolved-property tween animation
 -> #76 Physics2D
 -> #77 Audio
 -> #91 unified Agent-readable profiler/diagnostics
 -> #78 Linux/compiler/toolchain hardening
 -> #92 tiered real-GPU conformance/release validation
 -> #79 persistence + authored schema migration
 -> #12 flagship external game proof
 -> #60 Mesh2D
 -> #61 Spine SP0 license gate
```

Issue #93 records genuine later gaps that are **not** automatically promoted into the core lane: 2D lighting/shadows, navigation/pathfinding, broader platforms, networking and safe hot reload.

## 2. Cross-cutting authority model

Every future subsystem must classify its state into one of these categories.

### 2.1 Authoritative state

Authoritative state determines gameplay, deterministic test results, save/persistence semantics or semantic Agent assertions.

Examples:

- entity/component state,
- current fixed-step transforms,
- animation frame/event state,
- input actions,
- physics semantic state within the documented physics determinism boundary,
- semantic audio commands/events,
- material parameter values when authored/game-controlled,
- camera state when game-controlled,
- tween timeline state.

Authoritative state must remain available without a renderer and must not depend on the OS window, GPU timing, wall-clock presentation interpolation or screenshot inference.

### 2.2 Presentation state

Presentation state is derived for rendering/audio output and may depend on viewport size, interpolation alpha, GPU resources or backend capabilities.

Examples:

- interpolated render transforms,
- normalized UVs,
- resolved GPU handles,
- compiled shader/pipeline objects,
- viewport render targets,
- batch runs,
- camera view/projection coefficients,
- GPU particle buffers,
- physical speaker output.

Presentation state must never silently overwrite authoritative state.

### 2.3 Tooling/observation state

Tooling state exists only when explicitly requested.

Examples:

- Agent snapshots,
- JSON/MCP payloads,
- deterministic fingerprints,
- import/QA reports,
- profiler reports,
- migration reports,
- GPU capture/readback artifacts.

Tooling operations may allocate and may perform O(N) scans when bounded and explicit. Those costs must not appear automatically in the normal simulation/render frame path.

## 3. User-defined typed gameplay components

Issue #71 must support practical game-specific state such as `Health`, `EnemyBrain`, `Inventory`, `QuestState`, `WeaponState` or `Interactable` without turning Trace2D into a generic reflection engine or requiring game code to live inside `engine/`.

### 3.1 Stable component identity

Every registered game component type has a stable text identity equivalent to:

```text
ComponentTypeId = "game.health"
SchemaVersion   = 1
```

The exact C++ spelling may differ, but the semantic requirements are fixed:

- type identity is explicit and stable across process runs,
- allocation address, C++ RTTI name and registration order are not authored identity,
- duplicate type IDs are rejected at registration,
- the registration set is frozen before authored project/scene loading begins,
- ordinary gameplay access uses a resolved type index/typed handle rather than repeated string lookup.

### 3.2 Component classes

A registered game component is one of:

1. **authored component** — may appear in versioned scene/template text and therefore supplies explicit parse/validate/serialize behavior,
2. **runtime-only component** — created by game code and not required to serialize back into authored scene data.

Both may opt into Agent inspection. Only an authored component is required to provide authored schema behavior.

### 3.3 No generic field reflection requirement

Registration uses explicit typed adapters/callbacks. Conceptually a descriptor may provide:

```text
construct / destroy
parse authored data
validate
serialize canonical authored data
inspect semantic fields
optional writable-property bindings for #90
```

The engine must not require a generic `std::map<string, Variant>` property bag, compiler RTTI reflection, arbitrary field offset mutation or a scripting VM.

The concrete storage implementation remains an implementation decision, but it must preserve:

- strongly typed access from game C++,
- generation-safe invalidation,
- deterministic observable order,
- no per-frame type-name lookup,
- no mandatory heap allocation per component if workloads show that representation is costly.

### 3.4 Multiplicity

The baseline is **one instance of a given component type per entity**. A specific engine subsystem may later justify multiplicity through an explicit keyed/multi-component contract. Do not make every component a vector merely because multiple colliders or audio sources might someday be useful.

### 3.5 Load lifecycle

For authored world creation, the semantic order is:

```text
create entities and stable identities
 -> construct attached component instances in deterministic authored order
 -> decode/validate authored component state
 -> resolve entity/resource references
 -> establish hierarchy/world transforms
 -> publish scene-ready state to game/application lifecycle
```

A component must not depend on another component's allocation address as persistent identity.

Reference resolution failures report the source entity/component/reference and do not silently substitute another target.

### 3.6 Destruction lifecycle

Entity/world destruction occurs at a documented deterministic safe point. Any resolved component/entity handle becomes invalid through generation checking before stale memory can alias a replacement object.

Game/application shutdown or scene-stop notification occurs before component storage is destroyed when the game needs deterministic cleanup. Exact callback names are implementation details; lifecycle order is not.

### 3.7 Agent inspection

User components opt into structured Agent observation through an explicit inspector adapter. Observed values use a bounded Trace2D-owned semantic value vocabulary, for example:

- bool,
- signed/unsigned integer,
- floating scalar,
- text,
- `float2`,
- color/`float4`,
- stable entity/resource references,
- small enums represented by stable semantic names.

The adapter may allocate copied observation strings only during explicit inspection. JSON/MCP types remain outside game/scene modules.

Agent assertions target stable component type + semantic field identity. Runtime hot paths never construct these snapshots automatically.

## 4. Fixed-step authoritative state and render interpolation

Trace2D's deterministic fixed-step simulation remains authoritative. Smooth interactive rendering is a separate presentation concern.

### 4.1 Transform history

A presentation-interpolatable transform maintains logically:

```text
previous_fixed
current_fixed
```

At the beginning of each successful fixed simulation step, the previous sample becomes the prior current sample. Gameplay then updates current authoritative state.

Reset, scene load, teleport/warp and explicitly non-interpolated state changes synchronize previous/current so a visual smear is not introduced accidentally.

### 4.2 Interactive alpha

For normal wall-clock-driven presentation:

```text
alpha = accumulated_wall_time / fixed_step
```

with the runtime's existing accumulator semantics and a documented clamp/range. Rendering derives presentation state from previous/current + alpha. It does not mutate either authoritative sample.

### 4.3 Exact-frame capture

An explicit capture of simulation frame `N` must not depend on whatever wall-clock remainder happened to exist. Exact-frame deterministic capture uses an explicit **authoritative-current presentation mode** equivalent to rendering the current fixed state directly.

If a future tool wants an interpolated sub-frame capture, it must supply the interpolation alpha explicitly and record it in artifact metadata.

### 4.4 Interpolation math

For 2D transform presentation:

- position: linear interpolation,
- non-uniform scale: linear interpolation,
- rotation: documented shortest-arc angular interpolation under the engine angle convention,
- flip state and other discrete semantic state: selected by an explicit non-blended rule; do not interpolate booleans,
- parented entities: interpolate **local** transform samples first, then compose the interpolated hierarchy, so rigid parent/child relationships remain coherent.

Culling and final sprite geometry use the resolved presentation transform during interactive rendering. Agent/gameplay queries continue to use current authoritative transforms.

### 4.5 Performance

Interpolation data is retained alongside the relevant runtime presentation state and reused. No transient list, semantic selector lookup or heap allocation is required per sprite to interpolate a steady-state frame.

## 5. Unified typed asset/resource lifecycle (#86)

Trace2D already has deterministic project-relative texture identity. Future resources must converge on one semantic contract without requiring one monolithic editor database.

### 5.1 Authored identity

Canonical authored references are typed project-relative identities conceptually equivalent to:

```text
AssetRef<TextureAsset>
AssetRef<SpriteAsset>
AssetRef<FontAsset>
AssetRef<AudioClip>
AssetRef<Material2D>
```

Rules:

- absolute paths and traversal outside the project root remain invalid,
- path normalization/canonical identity happens during load/import/setup,
- authored identity remains text/diff friendly,
- hot paths do not repeatedly hash or compare authored path strings.

### 5.2 Resolved runtime handle

A runtime resource reference is a small typed, generation-safe resolved handle equivalent to:

```text
slot + generation + type domain
```

A stale handle must fail clearly rather than aliasing a new resource that reused the slot.

The exact bit layout is not frozen until implementation; generation safety and typed-domain safety are.

### 5.3 Ownership

CPU canonical asset state and GPU/backend resources remain separate.

Example:

```text
SpriteAsset (CPU canonical metadata)
        |
        v
resolved Sprite GPU presentation resources
```

Destroying/replacing a GPU texture must not destroy the authored `SpriteAsset` identity automatically.

### 5.4 Cache policy

- successful immutable loads are cached/reused,
- duplicate references to the same canonical asset do not decode duplicate immutable content,
- cache capacity/object reuse is preferred over repeated create/destroy churn,
- cache-owned lifetime is explicit; no tracing GC,
- no mandatory atomic reference-count increment/decrement on every gameplay/render access,
- explicit unload/release operations occur at safe points.

### 5.5 Dependency graph

Strong resource dependencies are resolved during setup and recorded for diagnostics/unload decisions. Strong dependency cycles are rejected with a chain diagnostic unless a concrete asset class explicitly defines a safe cycle model.

A soft reference, if later required, must be semantically distinct from a strong load dependency and must not silently force loading.

### 5.6 Unload

Baseline operations conceptually include:

```text
Unload(asset)
ReleaseUnused()
ClearProjectResources()
```

Exact API names may differ.

Unload must fail or follow an explicit cascade policy when active strong dependents/world instances still require the resource. It must never free a resource still reachable through a valid generation-safe handle.

`ReleaseUnused` may scan caches because it is explicit tooling/lifecycle work, not a frame-loop requirement.

### 5.7 Memory evidence

Resource diagnostics report separately:

- known retained CPU bytes,
- engine-created GPU resource byte estimates/sizes where known,
- retained capacities,
- resource/dependency counts.

Do not report unknown driver-side allocation or allocator overhead as exact GPU memory.

## 6. Reusable scene templates, instancing and world lifetime (#87)

Traditional games need reusable enemies, projectiles, NPCs, effects and level chunks. This must use the same world/component model rather than subsystem-specific factories.

### 6.1 Reusable authored hierarchy

The working semantic name is `SceneTemplate`. The final public name may be changed once, but the concept is fixed:

- one versioned text-authored reusable entity hierarchy,
- stable template-local entity IDs,
- engine and registered game components,
- resource references through #86,
- deterministic canonical ordering.

No binary editor-only prefab database is required.

### 6.2 Runtime instance identity

Every template instance has a stable explicit instance identity. Child semantic identity derives from:

```text
scene/world instance
 + template instance ID
 + template-local entity ID
```

not from pointer/address/allocation order.

The serialized textual spelling of the composed identity is an implementation detail; deterministic uniqueness is mandatory.

### 6.3 Overrides

Baseline instantiation accepts:

- explicit instance semantic ID,
- root/local transform,
- optional explicit typed authored component overrides validated through registered component schema adapters.

Do not introduce a generic free-form property override dictionary. Game code may mutate strongly typed component state after creation.

### 6.4 Deterministic spawn/despawn phase

Spawn/despawn requested while a fixed-step update is executing is applied at one documented structural-change safe point, preserving deterministic request order.

Setup code outside a running step may create initial authored world state directly.

The command representation should retain/reuse capacity where practical. It must not grow unbounded history.

### 6.5 Pooling

The engine does **not** silently pool arbitrary gameplay entities. Pooling changes lifecycle semantics and should remain a game/system choice unless a measured engine workload later justifies a specific pool.

Resource/cache reuse still applies automatically through #86. Games can build object pooling using explicit enable/reset/instantiate contracts without an engine-wide GC.

### 6.6 World/scene load and unload

Support explicit scene/world instances with stable semantic IDs.

Baseline loading may be synchronous. Additive loaded worlds/scenes have deterministic update/observation order, based on an explicit stable load/order key rather than container/hash iteration.

Unload occurs at a safe point and invalidates runtime entity/component handles deterministically.

Background streaming is a later optimization and must preserve these semantics.

## 7. Camera2D and Viewport2D (#88)

The current renderer orthographic camera math becomes a practical engine contract without making the renderer authoritative.

### 7.1 Camera2D

`Camera2D` is a typed world component. Baseline semantic state includes:

- entity/world transform position,
- 2D rotation when enabled by implementation,
- positive orthographic `vertical_size` preserving the current renderer convention,
- enabled state,
- deterministic priority/selection key,
- target viewport identity,
- optional world bounds/clamp policy,
- optional fixed-step follow/smoothing state only when explicitly configured.

A separate ambiguous `zoom` variable is not required if `vertical_size` provides the canonical projection scale. User-facing convenience helpers may convert zoom intent into that state.

### 7.2 Active-camera selection

For a viewport, active-camera selection is deterministic. A simple baseline is:

1. enabled cameras targeting the viewport,
2. greatest explicit priority,
3. stable semantic/order tie break.

No unordered-container iteration may choose a camera.

### 7.3 Viewport2D

A viewport has stable identity and separates logical game rendering from OS-window pixels.

Baseline properties include:

- logical width/height,
- presentation rectangle/target size,
- scaling mode,
- active camera,
- renderer-owned persistent target when an offscreen target is required.

Supported scaling semantics must be explicit. A practical baseline may include:

- `fit` — preserve aspect, letterbox/pillarbox,
- `fill` — preserve aspect while cropping overflow,
- `stretch` — map full logical viewport to target with non-uniform scale.

Pixel-perfect mode narrows these guarantees further and is governed by the Sprite pixel-perfect contract.

### 7.4 World/screen conversion

World-to-screen and screen-to-world transforms are backend-independent CPU math using resolved viewport/camera state. Round-trip behavior and edge conventions are headless-testable.

No renderer/GPU initialization is required to answer a semantic coordinate conversion query.

### 7.5 Camera smoothing and shake

If Trace2D owns camera follow/smoothing, it advances on fixed simulation steps and is authoritative/inspectable.

Camera shake is modeled as an explicit presentation offset/timeline and must not silently modify the followed gameplay target transform. When Trace2D generates shake noise, seed/timeline semantics are deterministic.

### 7.6 Capture identity

A capture identifies:

- authoritative simulation frame,
- viewport identity,
- selected camera identity,
- presentation interpolation mode/alpha when not authoritative-current.

## 8. Material2D and Shader2D (#89)

Trace2D needs programmable 2D effects but does not need a material graph.

### 8.1 Shader2D

`Shader2D` is a project-relative resource compiled/validated through the pinned SDL3 GPU/shadercross toolchain.

The public contract is Trace2D's shader ABI, not D3D/Vulkan/Metal ownership.

The first programmable surface keeps the standard Sprite vertex/geometry transform path and allows a custom **fragment stage first**. This supports common 2D visual effects while preventing custom vertex deformation from invalidating Sprite culling/pivot/trim contracts.

Arbitrary vertex deformation belongs to later Mesh2D or a separately approved Sprite extension.

### 8.2 Material2D

`Material2D` contains:

- one `Shader2D` reference,
- explicit blend/sampling state where the shader contract allows it,
- a finite typed parameter layout,
- default parameter values,
- resolved resource bindings.

A default built-in Sprite material follows the same resolved renderer path so custom materials do not create a parallel renderer.

### 8.3 Parameter types

Initial types remain deliberately finite, for example:

- float,
- integer/bool only where backend ABI behavior is explicit,
- `float2`,
- color/`float4`,
- texture/resource reference,
- sampler choice through a small cached renderer-owned set.

Do not expose arbitrary byte blobs as a public authored parameter model.

### 8.4 Setup-time binding

Shader reflection/contract validation resolves authored parameter names to compact parameter/binding indices and packed offsets during explicit material preparation.

Ordinary render submission performs no string lookup, no filesystem shader discovery and no shader compilation.

### 8.5 Per-instance overrides

Sprite/material instance overrides use a bounded resolved representation. Do not create one `unordered_map<string, Variant>` or heap object per sprite.

Common instance data such as tint/opacity remains in the optimized Sprite instance path rather than automatically becoming generic material parameters.

### 8.6 Batching

Renderer compatibility keys may include:

```text
texture/resources
material/pipeline
sampler
blend
mask state
```

but only compatible **contiguous** work may merge unless a future separately proven order-preserving scheme is adopted. Material identity never authorizes global painter-order sorting.

### 8.7 Agent semantics

Agents inspect canonical material/shader IDs and authored/game-controlled parameter values without requiring a GPU.

GPU shader pixels are presentation evidence. Unsupported backend/shader capability produces a structured explicit error rather than silent default-material fallback.

## 9. Deterministic resolved-property tween animation (#90)

Sprite frame animation does not cover UI fades, camera moves, transform tweens, color transitions or other property animation.

### 9.1 Timeline

Tween progress is based on integer fixed-step ticks / deterministic simulation time. Wall clock is never authoritative.

Each tween has explicit:

- delay,
- duration,
- playing/paused/completed/cancelled state,
- repeat count or infinite-repeat marker,
- loop/ping-pong mode,
- easing function,
- target binding,
- start/end values,
- deterministic completion/event semantics.

Boundary behavior at tick 0, duration end, repeat wrap and reset must be tested exactly.

### 9.2 Property binding

Authored semantic target information is resolved during load/setup to a compact typed binding equivalent to `PropertyBindingId`.

Runtime stepping must not repeatedly resolve:

```text
entity selector -> component type string -> property string
```

Engine components expose explicit writable-property adapters. User game components opt in through their #71 registration descriptor.

This is **not** generic field reflection.

### 9.3 Supported value types

Only types with exact interpolation semantics are admitted. Initial baseline:

- float,
- `float2`,
- color/`float4`.

Additional types require documented interpolation semantics before becoming authorable.

### 9.4 Easing

Use a finite named easing set with committed formulas/tests. The timeline progress numerator/denominator is derived from integer ticks; floating result math follows the engine's documented float determinism boundary.

### 9.5 Conflicts

Two active tweens targeting the same binding may not produce unspecified update-order behavior. The API/authored schema exposes an explicit policy such as reject, replace or sequence. The baseline implementation must choose and document one deterministic default.

### 9.6 Invalid targets

Entity/component destruction invalidates the binding generation. The tween transitions to a documented cancelled/error state rather than accessing stale storage.

## 10. Unified profiler and diagnostics (#91)

Trace2D already measures individual renderer/particle behavior. A practical engine needs one machine-readable observation surface.

### 10.1 Metric categories

Never mix these into one fake score:

1. **deterministic structural metrics** — counts, capacities, bytes, semantic operation counts,
2. **CPU machine timing** — wall-clock duration with environment metadata,
3. **GPU timing** — device/backend-dependent timestamp evidence,
4. **resource memory evidence** — known CPU bytes and engine-created GPU resource sizes/estimates.

### 10.2 Profiling surface

The intended CLI/API shape is equivalent to:

```text
trace2d profile <project/workload> --frames N --json
```

Output vocabulary includes, as implemented:

- frame/fixed-step counts,
- subsystem CPU scopes,
- entity/component/world counts,
- resource counts and retained bytes by type,
- renderer sprites/draws/material switches/upload bytes/capacities,
- particle alive/capacity/backend/structural cost,
- UI/tile/physics/audio counts,
- GPU timing/support state.

### 10.3 Scope IDs

Profiler scope names are registered/resolved outside measured hot loops to compact IDs.

Disabled profiling must have a measured minimal predictable cost. Expensive aggregation/timing may be disabled by build/runtime configuration.

### 10.4 Storage

Per-frame timing history is bounded, ring-buffered or capacity-reused. Do not retain one heap record per frame forever.

Report/JSON construction happens only when requested.

### 10.5 Allocation metrics

A custom allocator or global malloc interception is **not** required. First report Trace2D-owned capacities/resources and any explicit allocation counters already available. Broader allocation tracing requires separate measured justification.

### 10.6 Regression policy

Hosted shared CI may enforce deterministic structural budgets.

Wall-clock performance thresholds require a stable dedicated machine/runner and recorded environment. GitHub-hosted timing is evidence, not portable truth.

## 11. Tiered GPU conformance (#92)

Backend-independent correctness tests are necessary but cannot validate all real GPU state interactions.

### 11.1 Tier A — always-on hosted/headless validation

Must cover all behavior that can be made backend-independent:

- transform/pivot/UV/atlas math,
- culling/order/batch-run derivation,
- camera/viewport/interpolation math,
- material parameter layout and shader validation/compilation where headless tooling permits,
- canonical CPU artifact encoders,
- particle compiler/layout semantics.

### 11.2 Tier B — maintained real-GPU baseline

Before Trace2D claims a stable production-oriented release, at least one maintained real GPU environment must exercise representative actual GPU conformance for the primary supported rendering path.

Reports record:

- OS,
- GPU vendor/device,
- driver/runtime,
- renderer backend,
- build configuration,
- test/capture comparison mode.

### 11.3 Tier C — support-claim matrix

Claims spanning multiple vendors/backends/platforms require explicit release-matrix evidence. If infrastructure does not cover a target, documentation narrows the claim instead of implying validation.

### 11.4 Comparison rules

- cross-vendor bit-identical GPU floating-point output is not assumed,
- exact equality is used only where a specific path/environment proves it,
- otherwise committed tolerances define acceptable per-channel/pixel/semantic-probe error,
- golden changes require reviewable artifact/environment updates,
- gameplay correctness never becomes screenshot-only.

### 11.5 Required eventual fixtures

As those features exist, real-GPU conformance covers representative:

- Sprite transforms/atlas/trim/flip,
- blend/sampling/masking/9-slice/tiled/pixel-perfect paths,
- interactive interpolation and camera mapping,
- Material2D custom fragment effects,
- GPU particles against the CPU semantic oracle/tolerance contract,
- Mesh2D after #60.

### 11.6 Explicit validation cost

GPU readback, fences, image comparison and artifact encoding remain explicit test/capture work. They do not authorize synchronization/readback in ordinary gameplay frames.

## 12. Changes required of the Sprite program (#59)

The complete Sprite program remains responsible for canonical Sprite assets, production SpriteRenderer, SpriteAnimator, processing/generation QA and performance. It gains the following frozen integration requirements.

### 12.1 World/component compatibility

`SpriteRenderer2D` and `SpriteAnimator2D` public semantic state must be representable as finite typed components under the #71 model. They must not require a private Sprite-only entity graph.

The Sprite implementation may proceed before #71 is implemented, but its authoritative types/IDs/ownership must be designed so #71 attaches those same semantics rather than translating to a second model later.

### 12.2 Interpolation stage

The Sprite renderer must implement the fixed-step presentation interpolation contract from section 4 before claiming production-complete moving-sprite presentation.

This includes exact-frame capture behavior and backend-independent interpolation tests.

### 12.3 Material-ready batch seam

#59 does not need to implement #89 custom shaders, but its renderer contract must reserve a resolved material/pipeline compatibility identity so #89 can extend the batch key without replacing the Sprite submission architecture.

The #59 default sprite path behaves as one built-in material.

### 12.4 Camera-ready view seam

#59 uses a backend-independent resolved 2D view/presentation structure rather than embedding permanent assumptions that only one renderer-owned camera can ever exist. #88 later supplies world/project camera selection into that seam.

### 12.5 Resource-ready asset seam

Sprite canonical asset identity must remain project-relative and typed, with CPU asset truth separate from GPU handles, so #86 can unify lifecycle without changing authored Sprite semantics.

## 13. Changes required of game-production #71

Issue #71 is expanded beyond engine-owned components.

It must prove both:

1. engine component attachment through the same Scene world model,
2. at least one external user/game-defined authored component registered by an external game module, loaded from text, inspected/asserted by Agent and accessed strongly typed from game code.

The acceptance sample should include a semantic gameplay component such as health/state rather than proving only renderer components.

## 14. Later genuine gaps that remain deferred (#93)

These are real capabilities present in mature engines, but they do not block the first external-game proof enough to justify inserting them into the current sequence now.

### 14.1 2D lighting/shadows

Future contract should build on #89 material/shader and #60 geometry foundations. Prefer a small engine-owned `Light2D`/occluder model, explicit masks/layers and workload-driven light culling. Do not require PBR or deferred rendering simply to add 2D lights.

### 14.2 Navigation/pathfinding

After TileMap and a real workload, prefer deterministic grid/A* with explicit tie breaking before generic navmesh infrastructure. Semantic path/query results must be headless-testable.

### 14.3 macOS/mobile/Web/other platforms

After #78 proves a second toolchain/platform, promote targets only with build/test/runtime ownership. Touch/sensor/lifecycle input must feed the same semantic Input Actions model.

### 14.4 Networking

Do not build speculative replication. Choose authority/rollback/serialization semantics only from a concrete networked-game requirement.

### 14.5 Safe hot reload

Requires #86 resource lifetime and #79 migration/versioning first. Resource/data replacement must be transactional and generation-safe. Native C++ binary hot reload remains a separate ABI/lifecycle problem and is not implied.

## 15. Global performance rules

All future implementations in this document follow these requirements unless a measured workload and owner-approved contract explicitly changes them.

- resolve authored strings/paths/selectors during setup when practical,
- steady hot paths use typed/resolved handles/indices,
- prefer persistent/capacity-reused storage over repeated allocation,
- no tracing GC,
- no mandatory `shared_ptr` ownership graph in hot runtime code,
- no per-frame filesystem access,
- no per-frame shader compilation,
- no automatic Agent snapshot/JSON/report construction,
- no normal-frame GPU readback/fence wait for verification,
- no generic property maps on every entity/material/component,
- direct O(N) deterministic scans remain acceptable until measured workloads justify indexing,
- indexing/caches introduced later must preserve deterministic observable order,
- structural metrics and machine timings are never collapsed into a universal performance score.

## 16. Global diagnostics rules

New subsystems use stable structured diagnostic categories. Exact final enum names are frozen by their implementation PR, but every failure should identify:

- subsystem/category,
- semantic resource/entity/component/material/camera target where applicable,
- authored source path/location when available,
- expected versus observed value for assertions,
- frame/seed/world context when runtime-relevant,
- unsupported capability versus malformed data versus stale handle versus backend failure as distinct causes.

Silent fallback is prohibited when it could change semantics, rendering intent, resource identity or backend choice.

## 17. External flagship proof implications

Issue #12 remains the practical proof that these contracts compose.

Before #12 is considered complete, the external game should exercise a coherent subset of the expanded game-production sequence, including:

- external C++ game module,
- a user-defined typed gameplay component,
- project manifest/build/package path,
- hierarchy and reusable scene-template instancing,
- resource reuse/lifetime inspection,
- Camera2D/Viewport2D,
- semantic Input Actions,
- TileMap,
- production text/UI,
- at least one Material2D effect,
- deterministic tween behavior,
- Physics2D,
- semantic audio,
- profiler output,
- the maintained non-MSVC platform/toolchain,
- applicable GPU conformance evidence,
- persistence/migration,
- headless semantic Agent QA and exact-frame visual evidence.

It does not need to demonstrate every possible feature. It must use ordinary external/public contracts with no sample-only engine shortcuts.

## 18. Explicit non-goals preserved

This architecture freeze does not authorize:

- generic archetype ECS,
- generic runtime reflection system,
- browser DOM/CSS clone,
- material graph,
- render graph,
- visual scripting,
- scripting VM,
- custom allocator framework,
- lock-free infrastructure,
- speculative job system,
- binary plugin ABI,
- full graphical editor,
- PBR/deferred renderer,
- universal automatic object pooling,
- automatic CPU/GPU backend switching,
- broad cross-vendor bit-identical GPU claims.

The design goal remains a small explicit C++ 2D engine whose state is deterministic and machine-observable, whose presentation is measurable and verifiable, and whose coding-agent workflow does not depend on hidden editor state.