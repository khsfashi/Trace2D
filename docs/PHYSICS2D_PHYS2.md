# Physics2D PHYS2 — stable events and bounded overlap contract

Issue: #355, second implementation slice of #76.

## Dependency decision

PHYS2 continues to use the pinned **Box2D 3.1.1** backend adopted by PHYS1. It does not add another physics dependency or expose Box2D identifiers through the public Trace2D API.

Box2D 3.1.1 provides post-step contact/sensor event arrays and exact shape-overlap queries. Those backend arrays and shape IDs remain transient implementation details. Trace2D copies only generation-safe Scene identity, fixed-capacity collider semantic identity, and bounded contact geometry into self-contained engine-owned values.

PHYS2 also maps Box2D hit events because the pinned backend exposes unambiguous point, normal, and positive approach-speed data. Trace2D does not invent a second impact heuristic in this slice: Box2D's world hit-event threshold remains the backend policy, while the emitted semantic record carries `approachSpeed` for gameplay/presentation decisions.

## Fixed-step and event timing

`PhysicsWorld2D::StepWithReport(fixedDeltaSeconds)` is the detailed PHYS2 stepping entry point:

1. stale or unsupported Scene bindings are pruned;
2. Box2D advances exactly once using the caller-supplied fixed delta;
3. the transient Box2D contact/sensor arrays for that completed step are projected immediately into retained Trace2D values;
4. dynamic/kinematic authoritative transforms are written back to Scene;
5. a `PhysicsStepReport2D` describes the published event counts or any prepared-capacity failure.

The existing `Step(float)` API remains available and delegates to the same path, returning only `PhysicsStepResult2D`.

`ContactEvents()` and `SensorEvents()` expose spans over the current Trace2D-owned batch. The records contain no Box2D IDs, transient-array references, pointers, or heap-owned semantic strings.

A subsequent step replaces the current batch. A caller that needs longer retention may copy the small self-contained records.

## Stable contact semantics

PHYS2 projects non-sensor contact **begin**, **end**, and backend-qualified **hit** events.

Every `PhysicsContactEvent2D` contains:

- `entityA` / `entityB` as generation-safe Scene `EntityId`;
- the stable authored collider semantic ID for both participants copied into fixed-capacity storage;
- begin/end/hit kind;
- for begin events, the first Box2D manifold contact point and manifold normal when a contact point exists;
- for hit events, Box2D's hit point, normal, and positive `approachSpeed`.

Observable A/B ownership is canonicalized by:

```text
collider semantic_id
 -> entity index
 -> entity generation
```

The normal is defined from canonical A toward canonical B. When canonicalization swaps the backend A/B order, Trace2D negates the manifold normal.

The whole contact batch is stable-sorted by kind and canonical participant keys before publication. Backend traversal/storage order is not part of the Trace2D contract.

End events intentionally carry no contact geometry because Box2D's end-touch event only identifies the shapes. Begin events set `approachSpeed` to zero because the begin manifold does not expose a solved impact speed; hit events are the authority for that quantity.

## Stable sensor semantics

Sensor events preserve semantic roles rather than arbitrary pair order:

- `sensorEntity` + sensor collider semantic ID;
- `visitorEntity` + visitor collider semantic ID;
- begin/end kind.

Sensor event generation is enabled for all PHYS2-supported shapes because Box2D requires both the sensor and the visiting shape to opt into sensor events. Non-sensor contact and hit event generation is enabled for solid colliders.

The batch is stable-sorted by kind, sensor key, then visitor key.

## Event capacity and the non-retry rule

`ReserveEvents(contactCapacity, sensorCapacity)` prepares the retained fixed-size storage used by post-step event projection.

Within prepared capacity, PHYS2 performs no per-event heap allocation and constructs no semantic strings in the fixed-step path.

If a completed Box2D step produces more publishable contact or sensor events than the prepared Trace2D budget:

- `StepWithReport` returns `event_capacity_exceeded`;
- both published event spans are empty, so there is no partial authoritative batch;
- the report exposes the exact required contact and sensor capacities observed for that step;
- `eventCapacityFailureCount` increments.

**The physics solver has already advanced when this is discovered. Do not reserve and retry the same fixed delta.** Production callers should treat this as a hard world-budget violation: emit telemetry/fail the gameplay tick according to product policy, increase the prepared budget for future steps, and continue from the already-advanced authoritative world state.

This differs intentionally from spatial query capacity failure, because queries do not advance simulation state.

## Bounded circle and box overlap queries

PHYS2 adds precise Box2D shape-overlap queries for the same finite shape vocabulary established by PHYS1:

- `OverlapCircle`;
- `OverlapBox`, including explicit query rotation.

Each query takes explicit 32-bit layer/mask bits and a caller-provided output span. `ReserveOverlap(capacity)` prepares the retained hit scratch.

For successful observable results, Trace2D stable-sorts by:

```text
collider semantic_id
 -> entity index
 -> entity generation
```

Each result contains only generation-safe Scene `EntityId` and the copied collider semantic ID.

If either retained scratch or caller output is too small, the query returns `capacity_exceeded`, reports `requiredCapacity`, publishes no partial output, and increments `overlapCapacityFailureCount`. Because a query does not advance world state, callers may reserve and retry that query.

## Lifecycle

PHYS1 stale/unsupported binding pruning remains the authority boundary before simulation and queries.

Post-step event projection validates backend shape IDs before consulting user data. This matters especially for end events, which Box2D may produce because a shape/body was destroyed. Invalid/destroyed backend shapes are skipped rather than publishing a stale or recycled Scene participant.

Scene generation remains part of every public participant identity.

## Cost model

At prepared capacity:

- Box2D owns broad-phase/narrow-phase contact and overlap work;
- Trace2D copies fixed-size event/result records;
- contact and sensor publication sort cost is `O(E log E)`;
- overlap result sort cost is `O(H log H)`;
- no reflection, semantic string lookup, JSON/object graph conversion, or per-event heap object allocation occurs in the steady fixed-step/query path.

`PhysicsMetrics2D` adds retained overlap/contact/sensor capacities, currently published event counts, overlap query/failure counters, and event-capacity failures for later #91 aggregation.

## Explicit PHYS2 deferrals

PHYS2 still does not complete #76. It deliberately defers until later bounded slices / representative evidence:

- a Trace2D-owned override/configuration policy for Box2D's hit-event threshold;
- runtime force/impulse/teleport body control;
- shape casts/sweeps;
- compound/multiple colliders;
- joints/constraints and their semantic identity;
- one-way platforms;
- character movement helpers;
- hierarchy-aware rigid bodies;
- structured collision debug geometry;
- any universal cross-platform bit-identical floating-point replay claim.

The next candidate after PHYS2 is the PHYS3 runtime-body-control + shape-cast / production-depth decision slice described by #355.
