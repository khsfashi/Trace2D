# Production capability gap register

Tracking issue: #101.

This document records objective gaps found while comparing Trace2D's implemented surface and existing roadmap with the capability expected from practical mature 2D game engines.

It does **not** change the active core order by itself. The purpose is to prevent missing or shallow contracts from being forgotten merely because the broader roadmap already names a subsystem.

## 1. Interpretation

A roadmap entry may be:

- **implemented** — executable behavior exists and is validated,
- **planned deeply** — contract/acceptance/performance/QA/integration are sufficiently specified,
- **planned shallowly** — the subsystem exists in the roadmap but important production behavior is unspecified,
- **missing/recorded later** — capability is not yet part of the core implementation lane.

This document focuses on the last two categories.

## 2. Production texture/import pipeline

The current texture asset slice establishes deterministic project-relative identity, CPU decode/cache and explicit invalidation. That is a good foundation but not yet a production texture pipeline.

The owning future asset/resource/package stages should explicitly define:

- color-space policy and sRGB/linear boundaries,
- straight/premultiplied-alpha conversion boundary,
- mip generation/use policy,
- max-size/rescale import policy,
- packaged/GPU texture compression and platform format policy,
- CPU decoded-copy retention/release policy,
- atlas-page format/compression policy,
- bulk/staging upload reuse,
- retained CPU/GPU memory evidence.

Do not keep every large atlas page as both an unnecessary permanent RGBA8 CPU copy and an uncompressed GPU texture by accident.

## 3. Particle semantic breadth after #53

The current particle architecture is intentionally strong on determinism, observability and explicit CPU/GPU authority. Its effect vocabulary remains finite.

After #53, add particle features only from representative effect evidence. Candidate later needs include:

- sub-emitters,
- trails,
- bounded modifiers/forces,
- collision interaction,
- animated sprite-region selection,
- material-driven effects,
- richer emission shapes.

Do not weaken the CPU semantic oracle or introduce opaque graph state merely to match a feature checklist.

## 4. Physics2D depth

#76 currently defines a practical body/collider/trigger/query baseline. Before production-oriented completion, explicitly decide the required subset of:

- joints/constraints,
- continuous collision / CCD policy,
- shape casts/sweeps,
- friction/restitution/material semantics,
- one-way platforms,
- character-controller/movement-helper boundary,
- physics debug/inspection visualization.

Prefer a mature reviewed dependency where it fits. Do not implement a bespoke physics engine for product identity reasons.

## 5. Tile authoring semantics

#73 correctly requires bounded storage, batching/culling and semantic Agent queries. It should additionally make explicit decisions about:

- terrain/autotiling workflow,
- per-tile semantic/custom metadata,
- gameplay/object markers,
- navigation/occlusion metadata handoff,
- large-map import/editing representation,
- deterministic authored/generated companion formats when raw dense text becomes impractical.

Retain the hard rule against one heap object/string/draw call per tile.

## 6. UI pointer and event semantics

#72/#75 should explicitly cover the interaction semantics required by real mouse/touch UI:

- deterministic pointer hit testing,
- hover/pressed state,
- pointer capture,
- focus scopes/modal behavior,
- event routing/consumption order,
- clipping/scissor semantics,
- coordinate conversion through Viewport2D.

Semantic identity remains the preferred Agent target even when physical pointer interaction is supported.

## 7. Audio production baseline

#77 should not leave common long-running game audio entirely to later optional decisions.

Explicitly decide a bounded V1 for:

- streamed music/large clips,
- groups/buses,
- fades,
- simultaneous voice limits/voice stealing,
- device suspend/resume,
- decoded/streaming buffer ownership and memory budgets.

A large DSP/effect graph remains out of scope without demonstrated need.

## 8. Skeletal/deformable 2D decision gate

The Sprite program deliberately excludes arbitrary deformable/skeletal rendering and Spine remains behind a license gate.

Before claiming broad production 2D animation support, make one explicit decision:

1. ship an optional approved Spine integration,
2. add a small native skeleton/deform path on Mesh2D,
3. document skeletal animation as deliberately outside the supported core scope.

Do not leave the capability accidentally undefined.

## 9. Human review / iteration tooling

Issue #96 and #99 own the product response to the absence of a broad editor.

The goal is not editor parity. The gap is a practical way to:

- see recent AI work,
- preview the current result,
- inspect structured verification,
- review perceptual quality,
- submit feedback,
- approve revisions.

Trace2D should close this with a result-first Workspace rather than silently forcing users back into manual source inspection for every visual iteration.

## 10. Extension and compatibility policy

Before stable third-party adoption, define:

- supported source-level engine/module extension boundary,
- external CMake integration conventions,
- public C++ API versioning/deprecation policy,
- authored schema compatibility/migration relationship,
- dependency/plugin licensing expectations.

A binary plugin ABI is not required unless real demand justifies it.

## 11. Remaining 2D render breadth

Beyond the currently planned Sprite/Material/Mesh/lighting path, explicitly decide representative needs for:

- parallax/canvas layers,
- render-to-texture/compositing,
- bounded custom line/polygon drawing,
- camera/background presentation layers.

Do not infer that these require a generic render graph.

## 12. Device breadth

After #72/#78 and broader-platform promotion, explicitly record needs for:

- controller hotplug/reconnect,
- touch/gesture input,
- haptics/rumble,
- app/mobile lifecycle events,
- sensor input only where a game requires it.

These feed the same semantic Input Actions boundary rather than introducing parallel gameplay APIs.

## 13. Existing implementation hardening

### Scene semantic-ID lookup

The current small Scene can use simple scans, but #71/#87 should avoid repeated O(N) semantic-ID lookup becoming a large dynamic-world spawn cost. Stable resolved IDs/handles and setup-time indexing are preferred once measured workloads justify them.

### Renderer texture handles

Current alpha renderer texture-handle tombstones should converge on #86 generation-safe resource-handle reuse instead of acquiring a conflicting renderer-only lifetime model.

### Bulk resource upload

Current explicit texture upload is setup-time work and should not be prematurely rewritten. When production atlas/resource workloads exist, measure bulk loading and add staging/upload reuse if the evidence requires it.

### Explicit inspection scans

O(N) work in explicit inspection/tooling is acceptable when bounded and requested. Do not optimize it by leaking persistent complexity into normal frame paths without evidence.

## 14. Promotion rule

When an owning subsystem becomes active:

1. read this gap register,
2. integrate the relevant requirements into the child issue/contract,
3. decide explicitly which items are V1, later, or out of scope,
4. create a separate child only when the scope would make the owning PR incoherent,
5. update this document so resolved gaps do not remain stale.

Issue #101 itself is not a reason for `Trace2D next/continue` to skip the current core order.