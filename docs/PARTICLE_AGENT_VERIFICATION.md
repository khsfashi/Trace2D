# Particle Agent Verification

Issue #50 completes structured Agent verification over the deterministic CPU particle reference state introduced by #47-#49.

This layer is intentionally renderer-independent. It does not perform GPU readback, pixel inference, filesystem work, TOML parsing, or background snapshot maintenance. All non-trivial inspection work is explicit Agent/QA work requested by the caller.

## API boundary

`trace2d::agent::AgentFacade` exposes four particle operations over an explicit `ParticleEmitterBinding`:

```text
InspectParticleEmitter(binding)
InspectParticles(binding, offset, limit)
InspectParticle(binding, spawn_ordinal)
AssertParticle(binding, assertion)
```

A binding contains:

- the authored entity semantic ID supplied by the scene/runtime owner,
- a non-owning pointer to the prepared `ParticleEmitter2D`.

The Agent layer does not own emitter lifetime and does not create a second particle simulation model. It reads the already-authoritative `ParticleEmitter2D` / `ParticleReferenceEmitter` state.

## Aggregate emitter inspection

`InspectParticleEmitter` returns one scalar-only `ParticleEmitterSnapshot` containing:

- entity semantic ID,
- effect semantic ID and canonical effect asset ID,
- stable emitter ID,
- prepared and playing state,
- lifecycle cycle frame and completed loop count,
- next CPU-reference simulation frame,
- alive count and configured capacity,
- spawn-attempt, emitted, updated, expired, dropped, and peak-alive counters,
- explicit deterministic CPU-reference state fingerprint.

The aggregate snapshot type contains no per-particle vector. Calling aggregate inspection therefore never materializes an alive-particle snapshot array. Fingerprinting still reads the live reference state explicitly in O(alive) time; it is not maintained during normal stepping.

## Bounded particle detail

`InspectParticles(binding, offset, limit)` is the only range materialization path.

Rules:

- `limit` must be `1..1024` (`MaxParticleInspectionCount`),
- `offset > alive_count` is `invalid_range`,
- `offset == alive_count` is valid and returns an empty page,
- allocation reserves only `min(limit, alive_count - offset)` entries,
- dense reference order is preserved.

#48 already guarantees stable compaction of survivors and monotonic append of accepted spawn ordinals, so dense detail order is stable spawn order.

`InspectParticle(binding, spawn_ordinal)` performs a single stable-ordinal lookup and returns at most one particle snapshot.

Every V1 authoritative particle field exposed by the CPU reference backend is copied:

```text
spawn_ordinal
position.x / position.y
velocity.x / velocity.y
acceleration.x / acceleration.y
age_frames / lifetime_frames
initial_size / size
rotation_radians
angular_velocity_radians_per_frame
initial_color.r/g/b/a
color.r/g/b/a
sprite_index
simulation_space
```

No field is hidden based on how a future GPU backend may pack, derive, or omit runtime storage.

## Fingerprint V1

The Agent fingerprint is a 64-bit FNV-1a digest over canonical CPU-reference bytes. It is a fast regression oracle, not a replacement for structured detail.

Canonical numeric encoding is explicit:

- unsigned integers are appended least-significant byte first,
- enums are appended by their fixed underlying integer value,
- floats are appended by their exact IEEE-754 `float` bit pattern via `std::bit_cast<uint32_t>`,
- no locale, string formatting, `std::hash`, pointer value, allocation order, or unordered-container iteration participates.

The V1 property order is:

1. fingerprint contract version,
2. complete prepared `ParticleReferenceDefinition` in declaration/semantic order,
3. ordered burst schedule (`frame`, `count`),
4. next simulation frame, alive count, next spawn ordinal,
5. reference counters (`spawnAttempts`, `spawned`, `updated`, `expired`, `dropped`, `peakAlive`),
6. every live particle in stable dense/spawn order,
7. for each particle, every `ParticleReferenceParticle` field in the documented V1 field order.

The fingerprint deliberately excludes non-reference presentation/identity/lifecycle state such as:

- entity/effect strings,
- renderer/GPU resources,
- camera/capture state,
- `ParticleEmitter2D` playing/loop counters when they do not change the CPU reference state.

The seed, stable emitter ID, simulation-space definition, emission definition, burst schedule, mutable reference frame/counters, and all live particle fields are included. Therefore adding or running an unrelated emitter cannot alter an existing emitter fingerprint.

Fingerprint computation happens only when aggregate inspection or a fingerprint assertion explicitly requests it. `ParticleEmitter2D::Step()` and `ParticleReferenceEmitter::Step()` do not hash, stringify, allocate snapshots, or serialize state.

## Typed assertions

`ParticleAssertion` combines:

- one finite `ParticleAssertionField`,
- an optional stable `spawnOrdinal`, required only for per-particle fields,
- one typed expected `ParticleValue` (`bool`, `uint64`, `float`, or `string`).

Aggregate assertion fields include lifecycle/reference counters and the fingerprint. Per-particle fields cover every scalar component of the V1 particle snapshot.

Assertion behavior is stable:

- aggregate field + spawn ordinal -> `invalid_assertion`,
- particle field without spawn ordinal -> `invalid_assertion`,
- missing live ordinal -> `particle_not_found`,
- wrong expected value kind -> `type_mismatch`,
- correct kind but unequal exact value -> `state_mismatch`.

Float assertions are exact because the CPU reference state is the deterministic oracle on the supported deterministic CPU toolchain. Future CPU/GPU conformance tolerances do not weaken CPU-reference assertions.

Every assertion result preserves the structured expected value and, when resolvable, observed value. Failure context contains:

- entity semantic ID,
- effect semantic and asset IDs,
- stable emitter ID,
- exact next-reference-frame boundary,
- emitter cycle frame,
- global seed,
- alive count,
- at most one selected particle snapshot for per-particle failures.

This bounded context is sufficient to reproduce a mismatch without dumping all live particles.

## Stable error vocabulary

```text
emitter_unavailable
emitter_not_prepared
invalid_range
particle_not_found
invalid_assertion
type_mismatch
state_mismatch
```

The public enum is authoritative; messages are human-readable diagnostics and should not be parsed as protocol identifiers.

## Performance contract

Ordinary particle stepping remains unchanged from #48/#49:

- no Agent snapshot allocation,
- no JSON/string construction,
- no fingerprint maintenance,
- no renderer/GPU work,
- no per-particle heap allocation.

Agent/QA costs are explicit:

- aggregate scalar copy: O(1),
- aggregate fingerprint: O(alive), no particle-array materialization,
- bounded detail: O(returned detail), capacity proportional to the requested bounded page,
- stable ordinal lookup: current reference implementation O(alive), with no speculative index added before measurement,
- per-particle assertion: current ordinal lookup plus one bounded snapshot,
- aggregate non-fingerprint assertion: O(1),
- fingerprint assertion: O(alive).

The direct O(N) ordinal scan is intentional under the repository rule to avoid speculative indexing until real Agent workloads justify one.

## Handoff to #51

The CPU particle language is now fully machine-verifiable before compiler/GPU work.

Issue #51 may consume the same authoritative definitions, counters, memory report, and explicit fingerprint to add deterministic structural CPU cost analysis and the human-controlled `cpu|gpu` backend decision/compiler flow. It must not move analysis, hashing, allocation, compilation, or backend selection into the normal particle update loop.
