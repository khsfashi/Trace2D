# Authored Particle Effects and `ParticleEmitter2D`

Issue #49 makes the deterministic CPU particle semantics from `docs/PARTICLE_REFERENCE.md` authorable without adding a second particle model.

## Design boundary

V1 deliberately has three layers:

1. a text-authored `.trace2d.particle.toml` source file,
2. one parsed and validated immutable `ParticleEffectAsset` shared through `ParticleEffectCache`,
3. one mutable `ParticleEmitter2D` simulation instance per live emitter.

The cache and parser are setup/tooling work. `ParticleEmitter2D::Step()` does not perform filesystem access, TOML parsing, path normalization, string/map lookup, cache discovery, hot reload, or renderer work. Its hot path delegates directly to the already-prepared deterministic CPU reference state.

`ParticleEmitter2D` keeps a `shared_ptr<const ParticleEffectAsset>` to the immutable authored meaning, but every emitter owns a separate `ParticleReferenceEmitter`. Mutable particle state is never shared between emitters.

## File identity

Particle effect sources use deterministic project-relative references and the suffix:

```text
effects/hit_spark.trace2d.particle.toml
```

Reference normalization follows the existing asset rules:

- `\` is normalized to `/`,
- empty and `.` path components are removed,
- absolute paths are rejected,
- drive-prefixed paths are rejected,
- every `..` traversal component is rejected,
- embedded null characters are rejected.

Therefore `effects/./hit_spark.trace2d.particle.toml` and `effects\hit_spark.trace2d.particle.toml` resolve to one cache key and one immutable cached object.

A source file is limited to 1 MiB in V1. This is an authoring safety bound, not a runtime performance recommendation.

## V1 schema

The schema is strict. Unknown fields, missing required fields, non-finite values, unsupported enum strings, invalid ranges, contradictory spawn-shape parameters, and configured safety-budget violations are errors.

```toml
format_version = 1

[effect]
id = "hit_spark"
backend = "cpu"
max_particles = 64
duration_frames = 12
loop = false
play_on_load = true
simulation_space = "world"

[emission]
start_frame = 1
count = 2
every_frames = 3

[spawn]
shape = "circle"
offset = [2.0, -1.0]
box_half_extents = [0.0, 0.0]
circle_radius = 4.0

[lifetime]
frames = [2, 6]

[motion]
speed = [0.5, 3.0]
angle_radians = [-3.0, 3.0]
acceleration = [0.0, -0.1]

[scale]
initial = [0.5, 2.0]
end_multiplier = 0.25

[rotation]
initial_radians = [-1.0, 1.0]
angular_velocity_radians_per_frame = [-0.2, 0.2]

[color]
initial_min = [0.2, 0.3, 0.4, 0.5]
initial_max = [1.0, 1.0, 1.0, 1.0]
end = [0.1, 0.2, 0.3, 0.0]

[presentation]
blend = "additive"
sprites = [
  "textures/particles/spark_a.png",
  "textures/particles/spark_b.png",
]

[[bursts]]
frame = 0
count = 4

[[bursts]]
frame = 6
count = 3
```

### Backend field

`backend = "cpu"` is executable in #49.

`backend = "gpu"` is a reserved, versioned value. It parses and serializes without being rewritten, but `ParticleEmitter2D::Prepare()` returns `BackendUnavailable` until the explicit GPU runtime slice (#52) exists. There is no silent GPU-to-CPU fallback.

The analyzer/compiler in #51 must consume this same field rather than inventing a separate backend-selection source.

### Semantic mapping

The authored fields normalize directly to the #48 CPU reference semantics:

- `max_particles` -> fixed emitter capacity,
- periodic `start_frame` / `count` / `every_frames` -> integer-frame periodic emission,
- ordered `[[bursts]]` -> frame-indexed burst schedule,
- point / box / circle -> `ParticleSpawnShape`,
- lifetime / motion / acceleration -> reference initialization and integration fields,
- scale and color tables -> initial ranges and over-life target values,
- rotation table -> initial rotation and angular velocity ranges,
- `sprites` -> deterministic bounded sprite-choice indices,
- `simulation_space` -> local/world semantic identity.

The texture strings are presentation references only in this slice. Renderer materialization remains outside the CPU reference simulation.

### Spawn-shape normalization

V1 writes all shape parameters explicitly so canonical text has one stable surface:

- `point` requires `box_half_extents = [0, 0]` and `circle_radius = 0`,
- `box` requires non-negative half extents and `circle_radius = 0`,
- `circle` requires `box_half_extents = [0, 0]` and a non-negative radius.

Contradictory combinations are rejected instead of being silently ignored.

## Lifecycle semantics

`duration_frames` is the exact number of CPU-reference steps in one effect cycle and must be greater than zero.

For a non-looping effect, `ParticleEmitter2D` executes exactly `duration_frames` successful steps and then stops stepping until explicitly played/restarted. The final frame remains observable.

For a looping effect, the final frame also remains observable. The reference state is reset immediately before the next requested step, then the next cycle starts at reference frame 0. This keeps the cycle boundary deterministic and avoids hiding the previous cycle's final observation.

`play_on_load` only selects the prepared emitter's initial playing state. It does not affect the authored semantic definition or backend choice.

Authors should choose a one-shot duration long enough to include the desired final particle lifetime; V1 lifecycle duration is an explicit effect-cycle boundary, not an implicit drain-until-empty policy.

## Scene reference

Particle-enabled scene loading accepts a small top-level authored reference array:

```toml
[[particle_emitters]]
entity = "fx_anchor"
effect = "effects/hit_spark.trace2d.particle.toml"
stable_id = 77
```

The ordinary scene entity still owns transform/name/tags. The particle reference only supplies:

- the target authored entity semantic ID,
- the normalized project-relative effect reference,
- an explicit numeric stable emitter ID.

V1 permits at most one `ParticleEmitter2D` reference per entity and requires `stable_id` values to be unique within the scene. This prevents pointer/allocation/container order from becoming random identity.

`LoadParticleSceneToml()` removes this explicit particle extension before delegating ordinary scene semantics to the existing strict scene loader. It then verifies that every particle reference targets an existing semantic entity.

This keeps #49 narrow: it does not add reflection, a generic component property bag, an editor importer architecture, or an arbitrary particle module graph.

## Safety budgets and diagnostics

`ParticleEffectCache` is constructed with `ParticleReferenceLimits`. Authored effects are checked before an emitter allocates simulation storage:

- `max_particles <= maxParticlesPerEmitter`,
- burst table count `<= maxBursts`,
- periodic count `<= maxSpawnAttemptsPerFrame`,
- each authored burst frame, including periodic emission on the same frame, stays within `maxSpawnAttemptsPerFrame`.

Failures contain a stable error code, semantic field path, message, and TOML line/column when source information is available. This is intended to be actionable by both humans and coding agents.

These limits are hard safety ceilings. They are not claims about a practical CPU budget. #51 and #53 must report measured/structural cost instead of inventing portable CPU percentages.

## Deterministic serialization

`SaveParticleEffectToml()` emits one canonical field order and locale-independent float text. Parsing canonical text and writing it again is stable for the supported V1 semantic surface.

The canonical serializer is useful for tests/tooling and does not run during particle stepping.

## Headless verification

The required flow works with no renderer initialization:

```text
scene TOML
  -> LoadParticleSceneToml
  -> normalized effect reference + stable_id
  -> ParticleEffectCache::Load
  -> shared immutable ParticleEffectAsset
  -> ParticleEmitter2D::Prepare(globalSeed, stable_id)
  -> deterministic CPU ParticleReferenceEmitter::Step
```

This is the semantic surface #50 should expose to Agent inspection/assertion/fingerprint commands. GPU compilation/runtime work remains explicitly deferred to #51/#52.
