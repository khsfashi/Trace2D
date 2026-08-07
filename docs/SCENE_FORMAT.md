# Trace2D Scene Text Format

Trace2D authored scenes use TOML and the file suffix:

```text
*.trace2d.toml
```

The format is intentionally small, readable, diffable, and deterministic. It is source data for humans and coding agents, not a generated cache format.

## Version 1 schema

A minimal scene:

```toml
format_version = 1

[scene]
id = "arena"
name = "Arena"

[[entities]]
id = "player"
name = "Player"
tags = ["controllable", "hero"]

[entities.transform]
position = [0.0, 0.0]
rotation_radians = 0.0
scale = [1.0, 1.0]
```

### Root fields

- `format_version` — required integer. Version 1 is currently supported.
- `[scene]` — required table.
- `[[entities]]` — optional array of authored entity tables.

Unknown fields are rejected instead of ignored so spelling mistakes produce actionable diagnostics.

### Scene fields

- `scene.id` — required, non-empty semantic scene ID.
- `scene.name` — optional human-readable name. Missing values load as an empty string.

### Entity fields

- `id` — required, non-empty semantic entity ID. IDs must be unique within the scene.
- `name` — optional human-readable name.
- `tags` — optional array of non-empty strings. Tags are normalized by sorting and removing duplicates.
- `[entities.transform]` — optional transform table.

Authored entities require semantic IDs. Runtime-spawned entities without semantic IDs remain valid runtime objects, but cannot be serialized back into authored scene text.

### Transform fields

All transform fields are optional and use these defaults:

```toml
position = [0.0, 0.0]
rotation_radians = 0.0
scale = [1.0, 1.0]
```

Coordinates and scale are two-element numeric arrays. Transform values must be finite and fit in a 32-bit float.

## Deterministic serialization rules

`SaveSceneToml` produces canonical text intended for stable Git diffs:

1. root fields use a fixed field order
2. scene fields use a fixed field order
3. authored entities are sorted lexicographically by semantic ID
4. entity fields use a fixed field order
5. tags use the normalized sorted order stored by `Scene`
6. transforms always serialize all version-1 fields
7. numeric formatting uses a locale-independent representation with sufficient precision for 32-bit float round trips
8. output ends with a newline

Serialization intentionally does not preserve comments, whitespace, or authored entity ordering. The serialized form is a canonical representation of semantic scene state.

## Diagnostics

`LoadSceneToml` returns structured diagnostics containing:

- semantic field path such as `entities[1].id`
- human-readable message
- source line when available
- source column when available

Syntax errors use path `$`. Validation errors identify the closest schema field.

Examples of rejected input include:

- missing `format_version`
- unsupported format versions
- missing or empty scene/entity IDs
- duplicate entity semantic IDs
- unknown fields
- wrong TOML value types
- malformed transform arrays
- non-finite or out-of-range transform values

## API boundary

The public scene API exposes Trace2D types only:

```cpp
trace2d::scene::SceneLoadResult load =
    trace2d::scene::LoadSceneToml(text, "level.trace2d.toml");

trace2d::scene::SceneSaveResult save =
    trace2d::scene::SaveSceneToml(scene);
```

TOML parser types do not appear in public headers. This keeps authored-format implementation details out of the runtime and future agent-facing API.

## Rationale

TOML was selected for the initial authored scene format because it is readable without an editor, supports comments, has a stable published grammar, and has a mature modern C++ implementation available through the project's pinned vcpkg workflow.

Trace2D uses `toml++` only at the scene text boundary. A custom scene DSL is deliberately avoided until there is a measured requirement that TOML cannot satisfy.
