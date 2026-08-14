# E2 Deterministic Scene Composition

Issue: **#71**

E2 turns `Scene` from identity + `Transform2D` into the single authoritative authored world model shared by game code and Agent inspection.

## Authority model

- `Scene` owns entity identity, hierarchy, local transforms and attached component instances.
- `Transform()` remains local authoritative state; world transforms are derived.
- A `ComponentRegistry` is setup/lifecycle state. It is frozen before authored loading.
- Stable text component IDs and schema versions are authored identity. Resolved numeric indices are runtime access keys. C++ RTTI names, allocation addresses and registration order are not semantic identity.
- Agent snapshots are explicit tooling allocations. No component snapshot/JSON/TOML work occurs automatically in a fixed step.

## Typed component boundary

A user type registers explicit typed callbacks for authored parse, validation, canonical serialization and optional semantic inspection. Runtime-only types may omit authored callbacks. The engine type-erases storage only behind the registry; game code retrieves the original C++ type through `ComponentTypeHandle<T>`.

Baseline storage is one instance/type/entity. Component objects are explicitly owned and destroyed; no tracing GC, mandatory atomic shared ownership or generic reflection/ECS is introduced. Type strings are resolved during authored setup or explicit Agent queries, not ordinary gameplay access.

The initial engine-authored registered component is `trace2d.visibility2d` (schema 1). The external consumer registers `game.health` (schema 1) from `examples/e0_external_game`, outside `engine/`, proving installed-SDK composition.

## Hierarchy semantics

- parent/child mutation rejects missing IDs, self-parenting and cycles before commit,
- child observation order is deterministic,
- `KeepLocal` and `KeepWorld` reparenting are explicit,
- world TRS composes parent scale/rotation/translation with local TRS,
- a parent with zero scale cannot support `KeepWorld` inversion,
- subtree destruction invalidates descendant generations before slot reuse can alias stale handles.

World transform resolution is allocation-free O(depth). No hidden world cache is maintained in E2 because direct mutable local transforms remain a supported API; workload evidence should precede adding dirty propagation/caching complexity.

## Authored lifecycle

Version-2 TOML follows the frozen order:

```text
create stable entities
 -> construct registered authored components in source order
 -> typed parse/validate
 -> resolve semantic parent references
 -> establish hierarchy/world transforms
 -> publish the completed Scene
```

Canonical output sorts entities and components by their stable semantic IDs, so serialization does not depend on slot allocation or registration order.

## External proof

The existing external E0/E1 game fixture is upgraded without moving game code into `engine/`:

- `content/scenes/main.trace2d.toml` authors `game.player` + child `game.weapon`,
- both use the engine `trace2d.visibility2d` component,
- the external project registers and authors `game.health`,
- fixed-step game logic accesses `Health` with a resolved typed handle,
- the windowed host reads world transform + visibility from the same `Scene`,
- the headless host performs save/load/save canonical round-trip and verifies `type:game.health`, hierarchy and world transform through the existing Agent query surface.

This deliberately reuses `Query`/`QueryOne` instead of adding an E2-specific Agent tool, preserving the Agent Complexity Budget introduced after B1.

## Performance boundary

Normal frame work added by E2 is limited to game-requested component access and hierarchy/world-transform queries. There is no mandatory per-frame filesystem parsing, semantic type-name lookup, TOML/JSON serialization, Agent snapshot construction, GPU readback or shared-ownership churn.

Component instances currently use explicit per-instance allocation at structural setup/lifecycle boundaries. This is not a frame-loop allocation path. If representative world workloads show that representation is material, storage pooling/chunking can be introduced behind the same typed-handle contract without changing authored identity.
