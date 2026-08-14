# W0 Scene Template and World Lifecycle

Issue: #87

This document freezes the first production contract for reusable authored hierarchy, runtime template identity, explicit world lifetime, and structural safe points.

## Authority

W0 does not add a prefab ECS or a second gameplay database.

```text
SceneTemplateResource (#86 identity/lifetime)
        |
        v
versioned Scene TOML -> one compiled template cache entry per exact resource handle generation
        |
        v
WorldLifecycle structural operation
        |
        v
existing E2 Scene (#71) = only entity/component/hierarchy authority
```

`WorldLifecycle` stores lifecycle metadata needed to find instances and release resources. All live transforms, hierarchy, authored external components, runtime components, and generation-safe entity handles remain owned by `Scene`.

## SceneTemplate resource

`SceneTemplateResource` is its own #86 resource domain. It is not encoded as a Sprite, Texture, editor prefab, or opaque binary database.

The resource owns canonical versioned TOML and may carry #86 strong dependencies such as immutable Texture/Sprite resources. Its retained TOML bytes are included in `ResourceMemoryEvidence`.

Compilation is explicit structural work:

1. resolve the exact generation-safe template handle,
2. parse with the frozen #71 `ComponentRegistry`,
3. capture authored entity order, local transforms, hierarchy, and authored typed components,
4. cache the compiled result by exact `(slot, generation, domain)`,
5. reuse the compiled result for later instances.

A stale resource generation is never satisfied from an old compiled cache entry.

Ordinary fixed updates over existing instances perform no TOML parsing, path canonicalization, filesystem access, or template lookup.

## Semantic identity

World IDs, template instance IDs, and template-local entity IDs are stable semantic segments. W0 rejects `/`, `\\`, control characters, and empty segments so the public derivation is unambiguous.

```text
instance root: world/instance
child:         world/instance/template-local-entity
```

Vector indices, addresses, allocation order, and compacted storage positions never define public identity.

The instance root is an explicit structural anchor in the canonical `Scene`. It carries the requested root/local transform and optionally attaches to an external parent. Authored template roots attach below that anchor.

## Transactional instantiation

Instantiation is synchronous structural work and may allocate. The semantic order is:

```text
validate world / IDs / parent / exact template resource
 -> resolve or compile template
 -> preflight semantic conflicts and typed override targets
 -> retain the template resource through #86
 -> create reserved semantic entity identities
 -> construct authored components in deterministic template order
 -> validate and apply typed authored overrides through #71
 -> establish hierarchy
 -> publish the instance record
```

If component construction, hierarchy, or another step fails before publish, every entity created by the attempt is destroyed and the template retain is released. No half-published instance remains queryable through `WorldLifecycle`.

Overrides are typed `AuthoredComponentSnapshot` values targeting an existing template-local component and exact schema version. W0 does not introduce a free-form property/Variant override dictionary.

## Structural safe point

Initial setup before simulation may call structural operations directly.

During gameplay, structural changes use the FIFO command queue. `Application` freezes the one engine-owned W0 safe point as:

```text
Input frame advance
 -> Runtime fixed step
 -> Game::OnFixedUpdate
 -> WorldLifecycle::CommitStructuralChanges
 -> next fixed step or presentation
```

Therefore a spawn/despawn/reparent/world-load request made inside `Game::OnFixedUpdate` is not observable as committed state until that callback returns. Headless and windowed applications both use `Application::StepFrames`, so they cannot select different W0 phase locations.

Command sequence numbers expose deterministic FIFO order when semantics depend on it. The pending command vector and last-commit result vector retain capacity across commits; committed history is not accumulated without bound.

## Despawn and stale handles

A live instance records every template-local `EntityId` plus its structural root. Despawn destroys the recorded entities even if gameplay reparented one away from the original subtree, then destroys the root and releases the per-instance template retain.

`Scene` generation counters provide stale-handle rejection. W0 deliberately does not pool arbitrary gameplay entities. Explicit game-specific pooling can be built later when profiling demonstrates a need and a reset contract exists.

## World lifecycle

A world has:

- stable semantic ID,
- display name,
- explicit signed `orderKey`,
- one canonical `Scene`,
- zero or more template instance lifecycle records.

Loaded world traversal order is `(orderKey, semanticId)`. `OrderedWorldCount`, `OrderedWorldId`, and `OrderedWorld` are allocation-free after structural setup.

Unload is structural. It despawns instances, releases their #86 ownership, destroys remaining entities, and removes the world from loaded order.

World records retain their `Scene` incarnation as a tombstone. Reloading the same owned semantic world reuses that `Scene` object's slot generations rather than constructing a fresh generation-1 database. This prevents an `EntityId` captured before unload from aliasing a new entity after reload.

Externally owned `Scene` instances may be attached; they must use the same frozen `ComponentRegistry` and must outlive `WorldLifecycle`.

Background streaming is not part of W0. Any later streaming implementation must preserve the same semantic identity, safe-point, stale-handle, and ordering rules.

## Inspection

Protocol-independent inspection exposes:

- world semantic ID/name/order,
- instance semantic ID,
- source template canonical #86 reference,
- template-local entity ID,
- derived live semantic entity ID,
- generation-safe `EntityId`,
- pending structural command sequence/kind/result.

Existing `Scene`/Agent inspection remains authoritative for hierarchy and typed component values.

## Performance boundary

Structural operations may allocate in proportion to authored work. W0 intentionally optimizes the steady state instead of adding hidden generic pooling:

- compiled templates are cached by exact resource generation,
- immutable assets are shared through #86 strong dependencies,
- no filesystem or TOML work occurs during ordinary updates,
- deterministic loaded-world traversal is allocation-free,
- structural command/result capacities are retained,
- no background thread, generic object factory, renderer handle, or GPU readback enters the lifecycle contract.

The external fixture under `examples/e0_external_game` is the acceptance gate. It publishes one reusable multi-entity template, instantiates it twice, validates a typed `game.health` override and hierarchy transform, proves shared #86 ownership, exercises in-step spawn/despawn/unload through the Application safe point, rejects duplicate/stale references, and unloads/reloads an additive world without stale-handle aliasing.

## Handoff

After #87 is merged with exact-head CI and extracted-SDK external consumer gates green, the owner-fixed core lane advances to #88.
