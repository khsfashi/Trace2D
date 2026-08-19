# Physics2D PHYS3 — bounded runtime control and stable shape-cast contract

Issue: #357, third and final V1 implementation slice of #76.

## Dependency and authority decision

PHYS3 continues the pinned **Box2D 3.1.1** backend adopted by PHYS1. It adds no second physics dependency and still exposes no `b2WorldId`, `b2BodyId`, `b2ShapeId`, backend pointer, or transient callback object through the public Trace2D API.

Runtime authority remains deliberately split:

1. authored `RigidBody2D` / `Collider2D` fields are creation/setup intent;
2. `PhysicsWorld2D` + Box2D own live rigid-body state after attach;
3. fixed stepping writes dynamic/kinematic position and rotation back to authoritative Scene transforms;
4. PHYS3 teleport is the one runtime command that updates both backend body transform and Scene transform immediately in the same call.

Runtime commands do **not** rewrite authored component fields. Rendering remains a consumer, never a second physics authority.

## Runtime body-control surface

PHYS3 adds five generation-safe `EntityId` operations:

- `SetLinearVelocity` — supported for kinematic and dynamic bodies;
- `SetAngularVelocity` — supported for kinematic and dynamic bodies;
- `ApplyForceToCenter` — supported for dynamic bodies only;
- `ApplyLinearImpulseToCenter` — supported for dynamic bodies only;
- `Teleport` — supported for attached static, kinematic, and dynamic bodies.

Static-body velocity and force/impulse requests fail explicitly with `unsupported_body_type`; kinematic force/impulse requests do the same. Non-finite command values fail with `invalid_input`. A missing Scene generation, an unattached body, an entity that became hierarchy/scale-invalid, and an invalid backend binding are distinct failures.

This slice intentionally does not add torque, angular impulse, or point-force variants. The center-of-mass operations cover the current gameplay proof without expanding the public surface speculatively.

### Wake policy

Wake behavior is explicit rather than inherited accidentally from backend implementation details:

- successful linear/angular velocity writes wake the non-static body;
- force and impulse calls use backend `wake=true`;
- teleport wakes kinematic/dynamic bodies after the transform change;
- static teleport does not perform a meaningless wake operation.

A zero finite force/impulse remains a valid command. Callers own whether issuing such a no-op is useful.

### Hot-path cost

At an already attached body, a command performs no semantic-string construction, reflection, JSON/object conversion, or heap allocation. It resolves the retained binding, validates the current Scene generation/transform contract, then calls the pinned backend.

Current binding lookup is linear in attached body count, matching the pre-PHYS3 `PhysicsWorld2D` binding store. PHYS3 does not add a second lookup authority solely for speculative scale. `bodyCommandCount` and `bodyCommandFailureCount` are exposed so #91 / representative products can justify an indexed lookup later if measured command volume warrants it.

## Teleport coherence

`Teleport(entity, position, rotation)` is an explicit discontinuous move, not an implicit interpolation path. On success it:

1. sets the Box2D body transform;
2. wakes non-static bodies;
3. writes exactly the same finite position/rotation into the authoritative root Scene transform before returning.

The next fixed step therefore starts from the teleported physics state while presentation code can observe the same Scene transform immediately.

Teleport still obeys the PHYS1 root-entity + unit-scale restriction. If an attached entity was parented, scaled, or made non-finite after attach, the binding is removed and the command fails closed rather than interpreting local transform as world physics state.

## Bounded circle and rotated-box shape casts

PHYS3 adds:

- `CastCircle`;
- `CastBox`, including explicit start rotation.

Both queries take a start shape, finite non-zero translation, 32-bit query layer/mask bits, and a caller-provided output span. `ReserveShapeCast(capacity)` prepares retained hit scratch separately from ray/overlap scratch.

Each successful hit contains only:

- generation-safe Scene `EntityId`;
- fixed-capacity collider semantic ID;
- hit point;
- hit normal;
- sweep fraction.

Backend callback order is never observable. Successful results are stable-sorted by:

```text
fraction
 -> collider semantic_id
 -> entity index
 -> entity generation
```

If either retained scratch or caller output is too small, the cast returns `capacity_exceeded`, reports the exact observed required capacity, publishes zero partial hits, and increments `shapeCastCapacityFailureCount`. Because casts do not advance simulation, callers may reserve and retry.

Stale/unsupported bindings are pruned before the cast, matching PHYS1 ray and PHYS2 overlap lifecycle rules.

At prepared capacity, Trace2D result projection performs no per-hit heap allocation. Sorting cost is `O(H log H)` for `H` successful observable hits.

## CCD policy at the V1 boundary

The PHYS1 world continues to create Box2D with continuous collision enabled. Authored `RigidBody2D::bullet` maps directly to Box2D 3.1.1 `b2BodyDef::isBullet`.

That backend contract treats a bullet as a high-speed body that performs continuous collision detection against dynamic and kinematic bodies, but not other bullet bodies. Box2D explicitly warns that bullet bodies should be used sparingly. Trace2D therefore keeps `bullet` as an explicit opt-in setup flag rather than silently making every dynamic body a bullet.

Sensors remain a separate policy: the pinned backend documents that sensors do not use continuous collision. High-speed semantic sensor probing should use ray/shape casts rather than pretending a sensor event is guaranteed for an arbitrarily fast pass-through.

PHYS3 does not add another CCD mode or custom swept solver.

## Friction / restitution policy at the V1 boundary

The existing typed collider scalar fields remain the V1 material boundary:

- `friction` is copied into the backend surface material friction;
- `restitution` is copied into backend surface material restitution;
- density remains a collider/interior mass property as established by PHYS1.

The pinned backend's default material-mixing policy remains authoritative. Trace2D does not add a separate `PhysicsMaterial2D` resource, mixing callback vocabulary, or material database until representative gameplay demonstrates a repeated need that scalar authored values cannot cover.

PHYS2's restitution-based contact regression plus PHYS3's retained runtime/cast regressions keep this boundary observable without inventing duplicate solver logic.

## Metrics added by PHYS3

`PhysicsMetrics2D` now also exposes:

- retained shape-cast hit capacity;
- body command count / command failure count;
- shape-cast query count / capacity-failure count.

These remain scalar/capacity evidence only. Normal stepping and command/query execution do not build diagnostic strings or reports.

## #76 V1 completion decision

With PHYS1 + PHYS2 + PHYS3, the public V1 surface can now:

- author static/kinematic/dynamic bodies and box/circle colliders;
- fixed-step authoritative simulation;
- observe stable contact begin/end/hit and sensor begin/end events;
- raycast, circle/box overlap, and circle/rotated-box shape cast with bounded fail-closed output;
- set runtime velocities, apply dynamic-body center force/impulse, and teleport coherently;
- opt into the pinned backend's bullet CCD path;
- use typed density/friction/restitution without a second material authority.

The remaining #76 wishlist items are deliberately **deferred**, not silently forgotten:

- compound/multiple colliders per body;
- joints/constraints and semantic joint identity;
- one-way platforms;
- a Trace2D-owned character controller/helper;
- hierarchy-aware rigid bodies;
- structured collision/debug geometry;
- custom hit-event threshold policy;
- torque/angular-impulse/point-force convenience commands;
- universal cross-platform bit-identical floating-point replay.

None is required by the already accepted representative products, and building them before the next gameplay proof would violate Trace2D's product-proof rule. After PHYS3 is green, #76 can close as the bounded Physics2D V1. The lane should advance to #77 Audio, then #329 Combat Product Proof; any concrete physics gap exposed there should reopen as the smallest evidence-backed follow-up rather than a speculative PHYS4.
