# Particle Determinism Contract

Issue #47 locks the semantic primitives that every later Trace2D particle backend must share. This slice is deliberately CPU-only and contains no particle container, authored effect parser, renderer integration, compiler, or GPU execution.

The broader particle program is documented in [`PARTICLES.md`](PARTICLES.md).

## Module boundary

`engine/particles` is SDL-free and renderer-independent.

The first public surface is `trace2d/particles/ParticleDeterminism.hpp`. It owns only:

- integer frame-phase semantics,
- age/lifetime boundary helpers,
- stable particle-random key types,
- explicit random-channel IDs,
- pure keyed random helpers.

There is no mutable PRNG object, heap allocation, string lookup, `std::hash`, virtual dispatch, reflection, filesystem work, renderer handle, or GPU type in these helpers.

## Exact frame order

Every particle simulation frame uses this order:

```text
frame N
  1. ApplyCommands
  2. UpdateExisting
  3. ExpireExisting
  4. Emit
  5. Observe
  6. ExtractBackend
```

Meaning:

1. **ApplyCommands** — apply play/stop/reset/state changes and frame-N authored commands before simulation work for N.
2. **UpdateExisting** — update only particles that already existed before frame N began. A particle emitted later in frame N is not updated on its spawn frame.
3. **ExpireExisting** — remove existing particles whose updated integer age reaches the lifetime boundary.
4. **Emit** — process deterministic frame-N periodic emissions and bursts. Newly accepted particles have `ageFrames = 0`.
5. **Observe** — Agent/testing inspection for frame N sees the post-expiration, post-emission CPU reference state. A newly emitted positive-lifetime particle is therefore observable at age 0 on its spawn frame.
6. **ExtractBackend** — presentation/backend extraction happens after authoritative observation state is established. Extraction must not mutate CPU semantic truth.

The ordered phase list is executable through `ParticleFrameOrder()` and regression-tested. Later particle code must compose around this order rather than invent a second backend-specific frame sequence.

## Integer lifetime boundary

Particle age and lifetime are simulation-frame integers, not wall-clock durations.

For a positive `lifetimeFrames = L`:

- a particle is observable while `ageFrames < L`,
- a new particle is emitted at age 0,
- on each later frame it is updated first and its age advances by one,
- if the updated age reaches `L`, it expires before that frame's observation phase.

Example for a burst on frame 37 with `lifetimeFrames = 2`:

```text
frame 37 Emit     -> age 0 -> Observe: alive
frame 38 Update   -> age 1 -> Expire: no -> Observe: alive
frame 39 Update   -> age 2 -> Expire: yes -> Observe: absent
```

Therefore a one-frame lifetime is visible only on its spawn frame:

```text
frame N   Emit   -> age 0 -> observable
frame N+1 Update -> age 1 -> expires before observation
```

Authored effect validation in a later slice should reject zero lifetime where the schema requires a live particle. The low-level helper defines zero lifetime defensively as never observable and immediately expired, so malformed input cannot accidentally create an immortal particle.

## Spawn ordinal contract

`ParticleSpawnOrdinal` is a 64-bit per-emitter deterministic **spawn-attempt ordinal**.

Later emitters must follow these rules:

- start from a documented reset value, normally 0,
- assign/increment the ordinal in deterministic emission order,
- consume one ordinal for every deterministic spawn attempt, including an attempt dropped because bounded capacity is full,
- store the assigned ordinal on an accepted CPU-reference particle so detailed inspection can use it as stable particle identity,
- reset it only when the owning emitter/effect reset semantics explicitly say so.

Consuming ordinals for dropped attempts prevents capacity pressure from shifting the random values of later scheduled spawn attempts.

## Emitter stable identity

Randomness receives a numeric `ParticleEmitterStableId` (`uint64_t`).

The ID must be stable for the deterministic authored/runtime context. It must not be derived from:

- a raw pointer,
- allocation address,
- transient vector index,
- unordered-container iteration order,
- `std::hash` output.

Issue #49 will define how authored `ParticleEmitter2D` state supplies/derives this stable numeric identity during load/setup. Random helpers never parse or hash strings in the spawn path.

Two emitters with the same global seed and spawn ordinal but different stable IDs intentionally receive different random bits. Evaluating one emitter can never advance or perturb another emitter because no shared mutable random stream exists.

## Random key

Every authoritative spawn random value is a pure function of exactly:

```text
(globalSeed, emitterStableId, spawnOrdinal, randomChannel)
```

Represented by `ParticleRandomKey`:

```text
globalSeed       : uint64
emitterStableId  : uint64
spawnOrdinal     : uint64
channel           : explicit uint32 ID
```

Calling a random helper has no side effects. Query order therefore does not affect results.

## Stable random channels

Channel IDs are explicit numeric constants; enum declaration order is not used as an implicit sequence.

| Channel | Stable ID |
| --- | ---: |
| `SpawnPositionX` | `0x00010001` |
| `SpawnPositionY` | `0x00010002` |
| `Lifetime` | `0x00020001` |
| `Speed` | `0x00030001` |
| `Angle` | `0x00030002` |
| `Rotation` | `0x00040001` |
| `AngularVelocity` | `0x00040002` |
| `Size` | `0x00050001` |
| `ColorR` | `0x00060001` |
| `ColorG` | `0x00060002` |
| `ColorB` | `0x00060003` |
| `ColorA` | `0x00060004` |
| `SpriteChoice` | `0x00070001` |

New semantic random properties must receive new explicit IDs. Existing IDs must never be renumbered merely because a field is inserted or reordered.

This makes channel isolation structural: adding a color channel cannot shift position, lifetime, speed, or angle values.

## Exact 64-bit mixing

All arithmetic below is unsigned 64-bit arithmetic modulo `2^64`.

The local `Mix64(x)` finalizer is exactly:

```text
x ^= x >> 30
x *= 0xBF58476D1CE4E5B9
x ^= x >> 27
x *= 0x94D049BB133111EB
x ^= x >> 31
```

Domain constants are:

```text
SeedDomain    = 0x243F6A8885A308D3
EmitterDomain = 0x13198A2E03707344
OrdinalDomain = 0xA4093822299F31D0
ChannelDomain = 0x082EFA98EC4E6C89
```

`ParticleRandomBits(key)` is exactly:

```text
seedPart    = Mix64(globalSeed      XOR SeedDomain)
emitterPart = Mix64(emitterStableId XOR EmitterDomain)
ordinalPart = Mix64(spawnOrdinal    XOR OrdinalDomain)
channelPart = Mix64(uint64(channel) XOR ChannelDomain)

bits = Mix64(seedPart XOR emitterPart XOR ordinalPart XOR channelPart)
```

There is no hidden state and no library random distribution involved.

Committed test vector:

```text
globalSeed      = 0x0123456789ABCDEF
emitterStableId = 0xFEDCBA9876543210
spawnOrdinal    = 42
channel         = SpawnPositionX (0x00010001)

ParticleRandomBits = 0xE2B5E492311156F8
ParticleRandomU32  = 0xE2B5E492
```

A future CPU/GPU compiler/backend must preserve the documented integer key/channel mapping. V1 still does not claim universal bit-identical floating-point simulation across every GPU/vendor/driver.

## Exact `[0,1)` float mapping

`ParticleRandomUnitFloat` uses only the upper 24 random bits:

```text
upper24 = uint32(bits >> 40)            // 0 .. 2^24-1
unit    = float(upper24) * (1 / 2^24)   // [0, 1)
```

The scale constant is exactly representable as binary floating point. Every 24-bit integer is exactly representable in IEEE-754 binary32, so the committed CPU mapping avoids implementation-dependent standard-library distributions.

For the committed key above:

```text
upper24       = 0xE2B5E4
unit float bits = 0x3F62B5E4
```

`ParticleRandomFloatRange(key, minInclusive, maxExclusive)` is:

```text
minInclusive + (maxExclusive - minInclusive) * unit
```

Callers are responsible for schema-level validation of finite ordered range endpoints before spawn execution. The CPU reference uses this exact expression and tests a fixed range bit-vector on the supported toolchain.

## Isolation guarantees proven by tests

The #47 tests prove:

- the same full key reproduces exact 64-bit and 32-bit results,
- fixed CPU `[0,1)` and range mappings reproduce exact float bits,
- changing one spawn ordinal does not mutate results for any other ordinal,
- changing one emitter stable ID does not mutate another emitter,
- evaluating additional channels does not shift existing channel values,
- rebuilding the same key sequence after a reset to the same global seed reproduces the complete sequence,
- frame phase order is exact,
- burst-at-frame-N and one-/two-frame lifetime boundaries follow the documented observation semantics.

These are pure CPU tests and require no SDL platform, renderer, GPU, asset parser, or particle storage.

## Performance boundary

All #47 random operations are constant-time fixed-width integer/float arithmetic and are:

- allocation-free,
- string-free,
- lock-free without introducing a concurrency abstraction,
- free of virtual dispatch,
- independent of emitter count and other particle random calls.

The helpers are intended primarily for spawn/setup evaluation. Issue #51 may count random-channel evaluations for cost analysis, and Issue #52 may lower the same semantics for GPU execution, but neither may silently replace this CPU-reference contract.

## Handoff to #48

Issue #48 may now build the rich deterministic CPU particle reference storage/update layer on top of these contracts.

It must not change frame ordering, lifetime boundaries, spawn-ordinal meaning, channel IDs, or random mixing merely for implementation convenience. Any deliberate semantic change requires updating this document and the fixed regression vectors in the same PR.
