# Physics2D PHYS1 — Box2D substrate contract

Issue: #353, first implementation slice of #76.

## Dependency decision

Trace2D adopts **Box2D 3.1.1** from the exact vcpkg baseline already pinned by the repository. Box2D owns rigid-body integration, broad/narrow phase, collision shapes/filtering and ray traversal. Trace2D does not rebuild those capabilities.

The public engine contract does not expose `b2WorldId`, `b2BodyId`, `b2ShapeId`, Box2D callbacks or backend pointers. Those remain private implementation details of `Trace2D::Physics`.

## Authority split

PHYS1 has three explicit layers:

1. authored `RigidBody2D` / `Collider2D` components describe setup intent;
2. `PhysicsWorld2D` + Box2D own authoritative simulated body state during fixed stepping;
3. after each successful fixed step, root-entity position/rotation are written back to authoritative `Scene::Transform2D` for later presentation/interpolation systems to consume.

The renderer never advances or corrects physics state.

The authored `RigidBody2D` component keeps setup values such as initial velocity/damping. Runtime velocity/awake state is queried through `TryGetBodyState`; PHYS1 does not rewrite authored component fields every frame.

## Fixed-step contract

`PhysicsWorld2D::Step(fixedDeltaSeconds)` accepts an explicit positive finite delta and never reads a wall clock or owns a hidden accumulator. The caller decides the fixed simulation cadence. The Box2D sub-step count is fixed in `PhysicsWorldConfig2D` at world construction.

PHYS1 does **not** claim universal cross-platform bit-identical floating-point replay. Engine-owned observable ordering, lifecycle and semantic identity are deterministic where Trace2D explicitly controls them; third-party floating-point solver equivalence is a separate evidence question.

## Component vocabulary

### `trace2d.rigidbody2d` schema 1

- `body_type`: `static`, `kinematic`, `dynamic`
- `linear_velocity`: float2
- `angular_velocity`: radians/second
- `linear_damping`: non-negative float
- `angular_damping`: non-negative float
- `gravity_scale`: finite float
- `fixed_rotation`: bool
- `bullet`: bool; maps to Box2D's high-speed/CCD body flag

### `trace2d.collider2d` schema 1

PHYS1 intentionally supports one collider component per entity.

- `semantic_id`: stable 1..63 byte identity
- `shape`: `box` or `circle`
- `local_offset`: float2
- `half_extents`: positive float2; used by boxes
- `radius`: positive float; used by circles
- `layer_bits`: non-zero 32-bit bitset
- `mask_bits`: 32-bit bitset
- `sensor`: bool; collision response disabled by Box2D, semantic sensor events arrive in PHYS2
- `density`: non-negative float
- `friction`: non-negative float
- `restitution`: float in `[0,1]`

Box2D currently has 64-bit filter fields, but Trace2D PHYS1 exposes a 32-bit authored vocabulary so values round-trip without ambiguity through the current TOML signed-integer format. The values are losslessly widened when passed to Box2D.

## PHYS1 transform limitation

A PHYS1 physics entity must:

- be a Scene root entity,
- have unit scale `(1,1)`,
- have finite position/rotation/scale.

`AttachEntity` rejects violations. If a bound entity is later parented, given non-unit scale, or made non-finite, the binding is removed before the next solver step rather than silently interpreting local transform as world physics state.

Hierarchy-aware rigid-body composition is deferred until a concrete representative workload justifies the exact local/world ownership rule.

## Lifetime

`PhysicsWorld2D` stores generation-safe Scene `EntityId` for each binding. Before stepping it removes bindings whose owning entity was destroyed, then destroys the corresponding Box2D body. A recycled Scene slot therefore cannot receive writes from the stale physics binding.

Direct component edits after `AttachEntity` do not hot-patch the backend in PHYS1. Detach and attach again after changing authored setup. Runtime body-control operations are a later #76 slice.

## Bounded ray queries

`Raycast` consumes:

- origin + translation,
- query layer/mask bits,
- caller-provided output `std::span`.

The world owns a retained scratch buffer prepared by `Reserve`. Box2D explicitly permits ray callbacks in arbitrary traversal order, so Trace2D sorts successful observable hits by:

```text
fraction
 -> collider semantic_id
 -> entity index
 -> entity generation
```

If either retained scratch capacity or caller output capacity is smaller than the number of hits, the query returns `capacity_exceeded`, reports the required capacity, writes no partial authoritative result, and lets the caller reserve/retry. Steady queries within prepared capacity allocate no result objects.

## Cheap structural metrics

`PhysicsMetrics2D` exposes scalar/capacity evidence only:

- attached body count,
- retained body capacity,
- retained ray-hit capacity,
- fixed-step count,
- stale-entity prune count,
- unsupported-transform prune count,
- ray query count,
- ray capacity-failure count.

No JSON/string report is built during normal stepping. #91 can aggregate these values later.

## Explicit PHYS1 deferrals

PHYS1 does not pretend #76 is complete. Remaining work includes:

- contact begin/end/hit projection,
- trigger/sensor begin/end projection,
- overlap queries,
- shape casts/sweeps,
- compound/multiple colliders per body,
- runtime body force/impulse/teleport control surface,
- joints/constraints and semantic identity,
- explicit one-way platform decision,
- character movement/helper boundary,
- structured collision geometry/debug evidence,
- representative-game-driven CCD/material policy refinement.

Those continue under #76 after PHYS1 is green.
