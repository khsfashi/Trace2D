# Trace2D Scene Text Format

Trace2D authored scenes use TOML with the suffix `*.trace2d.toml`. The file is human/Agent source data, not a generated cache. The current canonical writer emits **format version 2**; version 1 remains readable and upgrades to version 2 on save.

## Version 2

```toml
format_version = 2

[scene]
id = "arena"
name = "Arena"

[[entities]]
id = "player"
name = "Player"
tags = ["controllable", "hero"]

[entities.transform]
position = [10.0, 0.0]
rotation_radians = 0.0
scale = [1.0, 1.0]

[[entities.components]]
type = "trace2d.visibility2d"
version = 1

[entities.components.data]
visible = true

[[entities.components]]
type = "game.health"
version = 1

[entities.components.data]
current = 75
maximum = 100

[[entities]]
id = "weapon"
parent = "player"

[entities.transform]
position = [2.0, 0.0]
rotation_radians = 0.0
scale = [1.0, 1.0]
```

### Root and entity fields

- `format_version` is required. Readers accept 1 and 2; writers emit 2.
- `[scene]` requires a non-empty semantic `id`; `name` is optional.
- `[[entities]]` requires a unique, non-empty semantic `id` for authored data.
- `name` and `tags` are optional. Tags are sorted and deduplicated by `Scene`.
- `parent` is an optional version-2 semantic entity reference. Missing targets, self-parenting and cycles are rejected.
- `[entities.transform]` is the authoritative **local** transform. Position and non-uniform scale are float2; rotation uses radians.
- `[[entities.components]]` is the version-2 typed authored component list.

Unknown schema fields are rejected rather than ignored.

## Typed authored components

A component entry contains:

- `type` — stable explicit `ComponentTypeId`, never an RTTI name or allocation address,
- `version` — positive schema version declared by the registered authored type,
- `[entities.components.data]` — adapter-owned semantic fields.

Before loading a scene that contains components, the caller supplies a `ComponentRegistry` and freezes it. Duplicate type IDs are rejected at registration. The loader resolves text IDs once during setup to numeric type indices; ordinary gameplay uses `ComponentTypeHandle<T>` / `ComponentHandle<T>` rather than repeated type-name lookup.

The baseline is one instance of a component type per entity. Runtime-only component types may be registered and attached from C++, but canonical authored serialization omits them.

The generic scene boundary only transports a bounded semantic authoring vocabulary (bool, signed/unsigned integer, finite scalar, text, float2, float4 and stable reference/enum text). The **typed adapter** owns field names, validation and conversion into the game/engine C++ type. There is no generic `map<string, Variant>` runtime truth model.

## Deterministic load lifecycle

For version-2 authored worlds, loading follows:

```text
parse schema
 -> create all entity identities
 -> construct components in authored entity/component order
 -> typed parse + validate
 -> resolve parent references
 -> establish local/world hierarchy
 -> publish Scene to Game/Application
```

Reference/component failures include the source entity/component path. A failed load does not publish a `Scene` in `SceneLoadResult`.

## Local/world hierarchy

`Entity::Transform()` remains the local authoritative transform for source compatibility and is equivalent to `LocalTransform()`. `Scene::TryGetWorldTransform()` composes parent transforms on demand. Reparenting is deterministic and supports `KeepLocal` and `KeepWorld`; cycles are rejected before mutation.

Child observation order is stable: semantic IDs sort lexicographically, with runtime-only IDs falling back to generation-safe entity handles. Destroying an entity destroys its subtree and invalidates all affected entity/component handles through generation changes.

World transform is currently O(hierarchy depth) and allocation-free. Trace2D intentionally does not maintain a hidden dirty cache while direct local-transform mutation remains public; a measured workload may justify a cache later.

## Canonical serialization

`SaveSceneToml` emits canonical version-2 text:

1. fixed root/scene field order,
2. entities sorted lexicographically by semantic ID,
3. normalized tag order,
4. semantic parent reference, when present,
5. complete local transform fields,
6. authored components sorted by stable component type ID (not registration index),
7. component data fields sorted by semantic field name,
8. locale-independent finite numeric formatting,
9. trailing newline.

Comments, input whitespace, authored entity order and registration order are not semantic state and are not preserved.

## Version 1 compatibility

Version 1 contains scene/entity identity, tags and local transform only. It remains accepted so existing content does not break. Saving any loaded version-1 scene writes the canonical version-2 representation.

## Public API

```cpp
trace2d::scene::ComponentRegistry registry;
auto sceneTypes = trace2d::scene::RegisterSceneComponents(registry);
auto health = registry.Register<Health>(healthRegistration);
registry.Freeze();

auto load = trace2d::scene::LoadSceneToml(text, registry, "level.trace2d.toml");
auto save = trace2d::scene::SaveSceneToml(*load.scene);
```

TOML parser types do not appear in public headers. `toml++` remains confined to the authored scene text boundary.
