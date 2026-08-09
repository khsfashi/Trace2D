# Particle CPU Reference Simulation

Issue #48 implements Trace2D's canonical deterministic CPU reference/validation backend for V1 particles. It builds directly on the frame, lifetime, spawn-ordinal, and keyed-random contracts in [`PARTICLE_DETERMINISM.md`](PARTICLE_DETERMINISM.md).

This layer is deliberately SDL-free and renderer-independent. It is semantic state for headless validation, replay, future Agent inspection, and later compiler/GPU conformance. It is also a valid CPU runtime backend when measured cost is acceptable.

## Boundary

`ParticleReferenceEmitter` owns one prepared emitter instance and its mutable particle state. It does not parse authored files, own scene components, submit rendering, create GPU resources, expose MCP/JSON, or choose CPU versus GPU automatically.

Those later steps remain ordered as:

```text
#48 CPU reference
  -> #49 authored effect assets + ParticleEmitter2D
  -> #50 Agent verification
  -> #51 cost analysis + human backend choice + compiler
  -> #52 GPU runtime
  -> #53 conformance/workloads/guidance
```

## Prepared SoA storage

The reference backend uses 13 fixed-capacity structure-of-arrays blocks prepared once per emitter:

- spawn ordinal
- position
- velocity
- acceleration
- age frames
- lifetime frames
- sampled initial size
- current size
- rotation
- angular velocity
- sampled initial color
- current color
- sprite index

Simulation space is immutable emitter-definition state and is returned with particle inspection without duplicating it into every SoA slot.

The semantic per-particle payload is **92 bytes** on the supported type contract. `MemoryReport()` reports:

- admitted capacity,
- bytes per particle payload,
- prepared particle-storage bytes,
- copied burst-schedule bytes,
- total prepared payload bytes,
- number of retained storage blocks,
- steady-state simulation allocations.

A representative capacity of 4096 therefore prepares **376,832 bytes** of particle payload (`4096 * 92`) before allocator bookkeeping. The committed workload test advances this capacity for 240 frames and requires the prepared memory report to remain unchanged.

The `steadyStateSimulationAllocations` contract is zero: `Step()` contains no storage growth, file parsing, snapshot creation, string/map work, renderer work, or particle-object allocation. Preparation may allocate the fixed SoA blocks and a copied burst schedule.

## Safety limits

`Prepare()` validates capacity and emission structure before allocating or simulating. The current default hard guards are:

- `maxParticlesPerEmitter = 65,536`
- `maxBursts = 4,096`
- `maxSpawnAttemptsPerFrame = 65,536`

These are **safety ceilings, not recommended performance budgets**. Issue #53 must derive practical authoring guidance from committed CPU/GPU workloads rather than treating these guards as a performance promise.

Invalid capacity, unsorted burst frames, invalid periodic cadence, malformed ranges/colors/shapes, zero sprite choices, and per-frame attempt totals above the configured guard fail deterministically before simulation.

## Exact frame behavior

`Step()` preserves the #47 phase semantics:

```text
UpdateExisting
  -> ExpireExisting via stable compaction
  -> Emit current-frame bursts
  -> Emit current-frame periodic cadence
  -> caller may Observe
```

There are currently no authored runtime commands in #48, so `ApplyCommands` is empty. There is no renderer/backend extraction in #48. #49 and later layers must compose around the locked outer phase order instead of redefining it.

A newly emitted particle has `ageFrames = 0` and is not updated on its spawn frame. Existing particles update first, then age advances. If the new age reaches lifetime, the particle expires before observation.

## Emission order and overflow

Burst entries must be ordered by frame. On a frame where both kinds fire, emission order is:

1. burst entries for that frame, preserving authored order,
2. periodic emission for that frame.

Every attempt consumes exactly one 64-bit `ParticleSpawnOrdinal`, including an attempt dropped because capacity is full. Accepted particles store that ordinal.

When particles expire, survivors are compacted in-place in ascending previous dense order. Because emission appends monotonically increasing ordinals, stable compaction preserves survivor spawn order. Newly accepted particles reuse the free dense tail without reusing old ordinals.

## Spawn initialization

All randomized initialization uses the #47 pure key:

```text
(globalSeed, emitterStableId, spawnOrdinal, randomChannel)
```

V1 reference initialization supports:

- point, axis-aligned box, and uniform-area circle spawn shapes,
- integer inclusive lifetime range,
- speed and angle range,
- fixed acceleration,
- initial size range plus deterministic size-over-life target multiplier,
- initial rotation range,
- angular velocity range,
- per-channel initial RGBA range plus deterministic color-over-life target,
- bounded sprite choice,
- local/world simulation-space identity.

Integer range mapping uses the high half of a 32x32->64 multiply so it does not depend on `std::uniform_int_distribution` behavior.

Circle spawn uses one stable channel for angle and one for radial-area sampling. Trigonometric/square-root evaluation belongs to the supported CPU reference toolchain contract; #47's fixed integer/keyed-random vectors remain the portable semantic key contract.

## Existing-particle update order

For each existing particle, one frame performs:

```text
velocity += acceleration
position += velocity
rotation += angularVelocity
age += 1
expire if age >= lifetime
if surviving:
    evaluate size over life
    evaluate color over life
```

Size/color over-life interpolation uses the observable lifetime domain. For `lifetimeFrames > 1`:

```text
t = ageFrames / (lifetimeFrames - 1)
```

so age 0 observes the sampled initial value and the final observable age (`lifetime - 1`) observes the authored end value. A one-frame particle is observable only at age 0 and therefore only exposes its initial value.

## Reset and replay

`Reset()` does not reallocate prepared storage. It resets:

- next simulation frame to 0,
- next spawn-attempt ordinal to 0,
- burst cursor,
- alive count,
- aggregate counters.

Inactive SoA bytes are not semantic state. Replaying the same prepared definition, seed, burst schedule, and frame count must reproduce every live authoritative field exactly on the supported deterministic CPU toolchain.

## Read access

#48 intentionally exposes allocation-free scalar particle reads for future #50 Agent adapters:

- dense alive-index lookup,
- stable spawn-ordinal lookup,
- complete `ParticleReferenceParticle` value containing every supported V1 authoritative field or immutable simulation-space derivation.

Ordinary stepping does not build snapshots, strings, fingerprints, JSON, or unbounded particle arrays. #50 will add explicit bounded Agent-facing inspection/assertion/fingerprint operations over this reference surface.

## Counters

The emitter retains cheap scalar counters for:

- spawn attempts,
- accepted spawns,
- particle updates,
- expirations,
- dropped attempts,
- peak alive count.

These are semantic/structural evidence only. Issue #51 will turn them and the static program/layout information into explicit cost reports; it must not pretend that operation counts are portable CPU percentages.

## Handoff to #49

Issue #49 may now make this semantic surface text-authored and scene-referenced.

It must preserve the CPU reference meaning rather than create a second particle model. Authored definitions should normalize into immutable shared effect data, while each `ParticleEmitter2D` owns independent mutable `ParticleReferenceEmitter` state. Paths/parsing/cache lookup must be resolved before stepping; no filesystem, TOML, strings, maps, or cache discovery belong in the per-particle update loop.
