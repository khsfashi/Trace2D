# Runtime Inspection Contract

Trace2D P3 exposes authoritative runtime and scene state through the protocol-independent `Trace2D::Agent` facade.

The facade is deliberately independent from JSON, JSON-RPC, MCP, a particular coding agent, and SDL. Protocol adapters serialize the same Trace2D-owned snapshot types at their own boundary.

## Engine API

The initial inspection entry point is:

```cpp
trace2d::agent::AgentFacade facade{&runtime, &scene};
trace2d::agent::InspectionResult result = facade.Inspect();
```

`AgentFacade` keeps non-owning pointers to the active runtime and scene. Callers may rebind either source when lifecycle ownership moves to a higher-level runtime in a later phase.

Inspection creates an owned snapshot only when requested. It does not add per-frame copying, logging, JSON generation, or temporary inspection collections to the simulation hot path.

## Snapshot schema

`InspectionSnapshot` contains:

- runtime frame
- deterministic seed
- fixed timestep in nanoseconds
- simulation time in nanoseconds
- scene semantic ID and human-readable name
- entities in the scene's deterministic observable runtime order

Each `EntitySnapshot` contains:

- generation-safe runtime handle: slot index + generation
- semantic ID
- human-readable name
- normalized sorted tags
- `Transform2D`
- nullable 2D bounds
- generic component snapshots and typed fields

The initial generic component list contains `Transform2D` with these fields in fixed order:

```text
position.x
position.y
rotation_radians
scale.x
scale.y
```

Component fields carry an explicit value kind (`bool`, `int64`, `uint64`, `float`, or `string`) so later components do not need to encode values as human-formatted text.

## Bounds policy

`bounds` is always part of the inspection schema, but is currently `null` because P3 has no renderer or physics component that can provide authoritative object bounds.

Trace2D intentionally does not invent bounds from transform scale, screen coordinates, or guessed sprite dimensions. A later renderer/physics phase can populate the same field without changing the meaning of current snapshots.

## Determinism

For the same runtime and scene state:

- runtime values are copied directly from deterministic runtime state
- entity order follows `Scene::ForEachEntity`, which is ascending live slot index
- tags use the scene's normalized sorted order
- component order is fixed by the facade
- component field order is fixed by the facade
- CLI JSON uses a fixed property order
- floating-point values use locale-independent `to_chars` formatting with enough precision for 32-bit float round trips

Inspection is an observation boundary. It may allocate to own the returned snapshot, but those allocations happen only when inspection is explicitly requested.

## Structured errors

The protocol-independent facade currently defines these stable error codes:

```text
runtime_unavailable
scene_unavailable
```

They are represented as `InspectionErrorCode` in the engine API and converted to stable strings through `trace2d::agent::ToString`.

Adapters may add adapter-specific errors, but must not replace or reinterpret engine inspection errors.

## CLI

The initial CLI adapter is:

```text
trace2d inspect --scene PATH [--frames N] [--seed N] [--json]
```

The CLI loads the authored scene, constructs the deterministic runtime, advances exactly the requested frame count without sleeping, and then invokes the same `AgentFacade::Inspect()` API used by tests and future adapters.

Example:

```powershell
trace2d inspect --scene tests/data/inspection.trace2d.toml --frames 12 --seed 42 --json
```

The JSON document is emitted as one deterministic line. Its top-level shape is:

```json
{
  "command": "inspect",
  "status": "ok",
  "runtime": {},
  "scene": {
    "entities": []
  }
}
```

JSON is not part of `engine/agent`; it exists only in `tools/trace2d`.

## CLI exit codes

`inspect` uses stable non-zero categories suitable for scripts and coding agents:

| Exit code | Meaning |
| ---: | --- |
| `0` | inspection succeeded |
| `2` | invalid command-line usage |
| `3` | scene file I/O failed |
| `4` | scene TOML syntax/schema validation failed |
| `5` | inspection or inspection serialization failed |

With `--json`, failures include a stable `code` and actionable `message`. Scene-load failures also include the scene loader's structured field-path diagnostics and source line/column where available.

## Dependency rule

The dependency direction remains:

```text
tools / protocol adapters
          |
          v
      engine/agent
       /       \
 runtime     scene
```

`engine/runtime` and `engine/scene` must never depend on the agent facade, JSON, MCP, or CLI code.
