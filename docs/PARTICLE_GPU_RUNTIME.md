# Particle GPU runtime contract

Issue: #52

This document defines the first real GPU execution backend for Trace2D particle effects. The CPU reference backend remains the semantic oracle. The GPU backend exists only for effects that a human explicitly authors with `backend = "gpu"` after CPU validation and cost analysis.

## Ownership and entry gate

A GPU emitter may be created only from a validated `ParticleProgram` whose selected backend is `gpu` and whose deterministic `ParticleGpuCompileArtifact` passes the runtime support check.

The runtime never:

- changes an authored CPU effect to GPU automatically,
- silently falls back to CPU,
- constructs or advances a `ParticleReferenceEmitter` in normal GPU mode,
- reparses the particle TOML in the frame loop,
- recompiles shaders in the frame loop.

The CPU side keeps only cheap emitter control state required to submit deterministic lifecycle/emission work: cycle frame, loop/reset state, next burst index, next spawn ordinal and the conservative render instance upper bound. Particle position, velocity, age, lifetime and other per-particle runtime fields live only in the GPU particle buffer.

## Compiler artifact authority

`CompileParticleProgram()` and `CompileParticleGpuArtifact()` from #51 remain the only layout authority.

The runtime consumes exactly:

- program fingerprint,
- GPU artifact fingerprint,
- pipeline variant identity,
- capacity,
- field kinds and byte offsets,
- stride,
- `capacity * stride` particle buffer byte count.

The runtime validates the artifact before GPU allocation. It does not widen the minimized per-particle payload into a second full runtime structure. Raw byte-address access is used so layouts such as the 12-byte constant-effect `Position + AgeFrames` payload remain valid.

Per-emitter immutable program constants are uploaded once to a separate cached GPU storage buffer. They are not part of the compiler-reported per-particle payload.

## Supported V1 surface

The #52 GPU backend executes the finite particle semantics already defined by the CPU reference/compiler:

- point, box and circle spawn,
- deterministic keyed lifetime/speed/angle/size/rotation/angular-velocity/color sampling,
- acceleration and position integration,
- frame-based lifetime expiry,
- size/color derivation over life,
- initial rotation plus angular velocity,
- local and world simulation space,
- periodic emission and authored bursts,
- looping/non-looping lifecycle,
- alpha and additive presentation.

### Explicit current limitation: variable sprite choice

A GPU effect with more than one authored sprite currently fails runtime support with `GpuParticleRuntimeError::UnsupportedFeature` and a `SpriteChoice` feature bit. There is no fallback and no implicit choice of the first sprite.

This keeps #52's runtime resource binding finite and reviewable. #53 may only change this after conformance/workload evidence proves a deterministic, batching-compatible resource model.

## Deterministic frame semantics

For a playing GPU emitter, one fixed simulation step performs:

```text
reset at loop boundary when required
 -> update particles that existed before this frame
 -> expire by the same age/lifetime boundary as the CPU oracle
 -> determine authored burst/periodic spawn attempts for this frame
 -> submit deterministic keyed spawn work
 -> advance emitter cycle/lifecycle state
```

Newly spawned particles have age `0` and are visible after that step. Spawn ordinals advance by attempted spawns, including attempts that cannot be admitted because capacity is full, matching the CPU key contract.

The keyed random hash reproduces the CPU integer mixing/channel domains in the GPU shader. Floating-point transcendental results are not claimed to be universally bit-identical across GPU vendors; #53 owns tolerance/invariant conformance policy.

## Persistent GPU resources

Emitter creation allocates and retains:

1. one particle storage buffer of exactly the compiler artifact byte count,
2. one immutable program-constant storage buffer.

Normal stepping reuses these resources. It does not allocate a new particle buffer, transfer buffer, report object or shader/pipeline per frame.

`GpuParticleEmitterMetrics` exposes:

- program/artifact/pipeline fingerprints,
- capacity, stride and compiler payload bytes,
- retained GPU bytes known to this backend,
- submitted steps/spawn attempts,
- clear/update/spawn dispatch counts,
- draw/instance counts,
- conservative instance upper bound,
- particle-buffer creation/growth counts,
- normal-frame readback and fence-wait counts.

The initial backend has a fixed-capacity particle buffer, so buffer growth remains zero after creation.

## No normal-frame readback

The runtime deliberately does not maintain an exact CPU `aliveCount` for GPU emitters.

Instead, rendering uses a conservative `instanceUpperBound`: the highest range of slots that may contain live particles since the last reset. Dead/expired slots are rejected in the vertex shader. This avoids a GPU-to-CPU counter readback or fence wait in the normal simulation/presentation path.

Consequences:

- exact live-particle inspection remains a CPU-reference/conformance concern,
- the upper bound may be larger than the current live count until reset,
- it is still bounded by authored capacity,
- `normalFrameReadbacks` and `normalFrameFenceWaits` must remain zero.

Explicit capture is separate: existing renderer capture may wait on a fence and read pixels because capture is an explicitly requested tooling operation, not normal GPU particle inspection.

## Compute submission

The backend uses separate compute passes where dependent storage writes require ordering:

- clear: parallel age-sentinel initialization,
- update: parallel update over the conservative occupied range,
- spawn: deterministic admission into dead/free slots.

Spawn admission currently uses one GPU invocation that walks bounded spawn attempts and slots in deterministic order. This deliberately favors semantic clarity for V1; #53 workload evidence determines whether a more parallel admission/compaction algorithm is justified.

No per-particle CPU update loop is introduced.

## Presentation and painter order

Each GPU emitter is rendered with at most one instanced draw call for its conservative instance range. There is no per-particle draw-call path.

Mixed Sprite/GPU-particle rendering uses the existing `(layer, stableOrder)` painter contract:

- callers provide sorted Sprite and GPU-particle streams when mixing them,
- the renderer merge-walks those two streams,
- it does not globally sort by texture/material,
- contiguous same-texture Sprite runs are preserved only while painter order allows it,
- a particle emitter boundary ends the current Sprite run,
- one particle emitter remains one painter-order item.

Within a GPU emitter, slots are drawn in deterministic slot order. Dead slots produce no visible contribution. Slot reuse may therefore differ from CPU storage compaction order; particle simulation semantics remain CPU-oracle driven and #53 owns GPU visual/conformance evidence.

## Resource lifetime

GPU emitter creation receives an already resolved renderer `TextureHandle`. The renderer validates that handle at creation and presentation. The texture must remain live until every GPU emitter that references it is destroyed or rebound by a future explicit API.

GPU particle handles are renderer-owned opaque handles. SDL GPU types do not escape the render implementation.

Renderer cleanup destroys GPU particle resources before renderer textures and the SDL GPU device.

## Headless and CI validation

Hosted CI does not require a real presentation GPU to validate the deterministic contract. Headless tests verify:

- a supported GPU program reports exactly the #51 compiler artifact fingerprints/layout,
- a CPU-selected program never qualifies for GPU runtime creation,
- variable sprite choice fails explicitly,
- an invalid/corrupt minimized artifact fails before GPU allocation.

A real GPU smoke/conformance matrix is hardware-dependent evidence. #53 expands conformance/workloads, and the later production architecture sequence retains the dedicated real-GPU validation gate in #92.

## Performance invariants

The following are hard #52 invariants unless a later measured change updates code, tests and this document together:

- no automatic backend switching,
- no silent fallback,
- no duplicate full CPU particle simulation in normal GPU mode,
- no normal-frame GPU readback,
- no normal-frame fence wait for particle inspection,
- no normal-frame shader compilation,
- no per-particle heap allocation,
- no per-particle CPU draw submission,
- persistent capacity-sized particle storage,
- no global texture/material sorting that breaks painter order.
