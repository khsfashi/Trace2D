# Particle system plan

## Purpose

Trace2D particles are designed around the engine's core product goal: a coding agent should be able to author an effect as text, run it deterministically without a renderer, inspect every supported semantic property, assert exact behavior, understand its performance cost, and only then compile it to a GPU runtime backend when a human decides that conversion is worthwhile.

The target workflow is:

```text
rich text-authored particle effect
        |
        v
CPU reference simulation
        |
        +--> exact-frame headless verification
        +--> complete structured Agent inspection/assertion
        +--> deterministic state fingerprint
        +--> deterministic structural CPU cost report
        +--> optional local Release timing evidence
        |
        v
human backend decision
   backend = cpu | gpu
        |
        +--> cpu: keep the reference/runtime backend
        |
        +--> gpu: deterministic ParticleProgram compile
                    -> minimized GPU runtime state
                    -> GPU execution/presentation
                    -> CPU/GPU conformance + visual QA
```

GPU conversion is never automatic in V1. Tooling and coding agents may recommend a backend using measured evidence, but the backend choice is explicit, reviewable text authored by a human.

## Fixed implementation sequence

Particle umbrella: GitHub Issue #46.

The owner-fixed child sequence begins only after renderer workloads Issue #41 is complete:

1. #47 — deterministic frame semantics and keyed randomness
2. #48 — rich deterministic CPU reference simulation
3. #49 — text-authored effect assets and `ParticleEmitter2D`
4. #50 — complete Agent verification over CPU reference state
5. #51 — CPU cost analysis, explicit human backend choice, and deterministic particle compiler
6. #52 — GPU runtime backend for explicitly GPU-selected effects
7. #53 — CPU/GPU conformance, workloads, safe budgets, build flow, and LLM guidance

Future agents must work on the first incomplete and unblocked item only. Each completed child PR updates `PROJECT_STATUS.md` and advances exactly one step.

## Semantic authority

The CPU reference backend is the canonical semantic oracle for the particle language.

It owns deterministic simulation truth such as:

- stable spawn ordinal
- position
- velocity
- acceleration or equivalent movement state where supported
- age and lifetime in simulation frames
- size / scale
- rotation / angular velocity
- color / alpha
- texture/sprite/frame selection when supported
- simulation-space state needed to make world/local behavior unambiguous

The supported field set is finite, versioned, documented, and compiler-understood. Rich state does not imply arbitrary scripts, callbacks, object bags, reflection, or a generic module graph.

Gameplay authority must not depend on visual particles. Gameplay logic may trigger an effect, but particle collisions or rendered pixels do not become the damage/gameplay source of truth in V1.

## Exact frame semantics

The first particle slice must lock one explicit frame contract and executable tests before broad behavior is added. The intended ordering is:

```text
frame N
  -> apply emitter commands/state changes scheduled for N
  -> update particles that existed before N
  -> expire particles at the documented lifetime boundary
  -> process deterministic emission/bursts for N
  -> newly spawned particles are observable with age 0
  -> inspect/assert CPU reference state
  -> optionally execute/extract the selected presentation backend
```

If implementation evidence requires a different ordering, the implementation, tests, and this document must change together.

Particle age/lifetime and deterministic emission use integer simulation frames, not wall-clock time.

## Deterministic randomness

Particle randomness is keyed rather than driven by one mutable sequential PRNG stream.

Conceptually each random value is derived from:

```text
(global seed, emitter stable identity, spawn ordinal, random channel)
```

Random channels have stable IDs such as spawn X, spawn Y, lifetime, speed, angle, rotation, or color. Adding a new random color field must not shift previously existing position/lifetime random values.

The implementation defines and tests the exact integer mixing algorithm and integer-to-float mapping. Authoritative behavior must not depend on implementation-defined `std::hash` or library distribution details.

## Rich CPU reference backend

The CPU backend exists primarily for complete observability, deterministic tests, and debugging. It is also a valid runtime backend when measured cost is acceptable.

Rich reference state is allowed, but the implementation remains disciplined:

- explicit bounded capacity
- no per-particle heap allocation
- no per-particle strings, maps, smart pointers, renderer handles, random-engine objects, or callbacks
- no virtual module dispatch in the update loop
- no filesystem/asset parsing in the update loop
- no JSON/snapshot generation during ordinary stepping
- no renderer/GPU requirement for headless simulation
- no custom allocator, job system, SIMD framework, or ECS rewrite without measured evidence

A structure-of-arrays representation should be preferred when it measurably improves rich-state memory/cache behavior or selective inspection. The selected representation must publish actual memory accounting rather than rely on qualitative claims.

Authored capacities are validated before simulation. Excessive LLM-authored capacities fail with actionable diagnostics rather than blindly allocating unbounded memory.

## Text-authored effect format

Effects use dedicated versioned files such as:

```text
effects/hit_spark.trace2d.particle.toml
```

Scenes reference them through `ParticleEmitter2D`; large effect definitions are not duplicated into every scene entity.

The exact V1 schema is finalized by #49 after #48 defines supported semantics. Candidate families include:

- bounded capacity
- explicit backend selection
- play/loop/duration frame semantics
- frame-indexed bursts
- integer-frame periodic emission
- point/box/circle spawn shapes when justified
- lifetime
- initial position/velocity/speed/angle
- acceleration/gravity/drag
- size/scale over life
- rotation/angular velocity
- color/alpha over life
- texture/sprite choice
- small explicit blend modes

Unknown fields and unsupported combinations are rejected. Project-relative asset identity reuses `engine/assets` rules.

Preferred backend spelling:

```toml
[effect]
backend = "cpu" # or "gpu"
```

Backend choice is explicit and version-controlled. The analyzer may recommend a change but must never rewrite this field automatically. An unsupported GPU selection fails clearly rather than silently falling back to CPU.

## Agent verification

Before GPU compilation is considered complete, an agent must be able to prove the CPU reference behavior without pixels.

Cheap aggregate emitter observation includes at least:

- effect/emitter identity
- playing/enabled state
- emitter age/frame state
- alive count
- configured capacity
- emitted total
- expired total
- dropped total
- deterministic state fingerprint

Ordinary inspection must not serialize every live particle.

Detailed particle inspection is explicit and bounded using stable spawn ordinal and/or offset/limit. For selected particles it exposes every supported authoritative V1 property.

Assertions follow the existing Trace2D testing philosophy: exact expected/observed state plus frame, seed, effect/emitter/entity context and bounded failure detail.

Fingerprint computation is explicit QA work. Do not add O(alive) hashing to every normal simulation frame simply to keep a fingerprint ready.

## CPU cost analysis

A completed CPU effect receives a transparent performance report before GPU conversion is considered.

### Deterministic structural metrics

These are machine-independent and suitable for CI/Agent reasoning:

- program/effect fingerprint
- configured capacity
- emitter count in the workload
- current/peak alive particles
- particles updated/spawned/expired/dropped
- CPU reference attributes stored
- bytes per particle or equivalent SoA capacity accounting
- total prepared CPU state bytes
- spawn random-channel evaluations
- semantic update operation counts by type
- over-life/curve/sample evaluation counts where supported
- steady-state allocation count attributable to particle stepping

Operation counts remain raw semantic counts. Trace2D must not pretend that every operation costs the same CPU cycles or expose an invented universal CPU percentage.

### Machine-dependent timing

An optional local Release benchmark may report average, median, p95, and useful per-particle timing over a fixed deterministic workload together with CPU/toolchain/build/workload metadata.

Hosted CI validates deterministic structure and allocation behavior, not wall-clock timing thresholds.

### Recommendation versus decision

Tooling may output labels such as:

```text
cpu_cost: low | moderate | high
recommendation: keep_cpu | consider_gpu
```

only when thresholds are documented from committed measurements and the raw data is included.

The final backend decision is always human-controlled.

## ParticleProgram compiler

The authored language is parsed into one deterministic semantic representation shared by both backends:

```text
ParticleEffectSource
        |
 parse + validate
        v
ParticleProgram
        |
        +--> CPU reference executor
        +--> cost/static analysis
        +--> Agent compile explanation
        +--> GPU lowering when backend=gpu
```

Do not maintain separate CPU and GPU interpretations of TOML.

Static analysis determines the feature mask, attributes read/written at spawn/update/render stages, constants, derivable attributes, random channels, CPU storage estimate, semantic operation counts, minimized GPU attributes, GPU stride/buffer estimate, and shader/pipeline variant.

Examples of safe GPU lowering:

- constant color -> effect/program constant
- linear alpha/scale over life -> derive from age/lifetime
- unused rotation -> no GPU rotation storage
- fixed texture -> effect/program resource, not per-particle handle

Unsupported or ambiguous GPU semantics produce a deterministic compile error.

Compilation is build/setup work and never runs in the normal frame path.

## GPU runtime backend

Only explicitly GPU-selected, verified effects execute on the GPU backend.

Normal GPU mode does not also run the CPU reference simulation. Dual execution is an explicit conformance/debug mode only.

GPU runtime rules:

- feature-minimized particle layout from compiler analysis
- persistent/capacity-reused GPU resources
- no normal-frame GPU readback
- no normal-frame fence wait solely for inspection
- no frame-time shader compilation
- no per-particle draw calls
- no global texture/material sorting that violates painter order
- resolved assets/resources before per-particle update
- clear diagnostics rather than silent CPU fallback

GPU culling is presentation optimization only and must not redefine particle semantic lifetime/update behavior.

## CPU/GPU determinism boundary

CPU reference simulation is the exact deterministic oracle on the supported deterministic toolchain.

V1 does not claim universal bit-identical floating-point GPU state across vendors, drivers, and backends.

The GPU backend must preserve exact discrete semantics where required, including emission frame/count, capacity/overflow rules, lifetime boundaries, program/feature identity, and random-channel mapping.

Floating-point conformance uses documented tolerances or higher-level invariants where exact cross-vendor equality cannot be guaranteed. Pixel equality is visual evidence, not the sole correctness oracle.

## Human backend decision workflow

The intended human decision process is:

```text
1. Author the effect with backend=cpu.
2. Validate exact behavior through the CPU reference backend.
3. Inspect structural cost and memory.
4. Run a local Release benchmark on a representative machine when cost matters.
5. Ask the agent for an evidence-backed recommendation if useful.
6. If CPU cost is acceptable, keep backend=cpu.
7. If measured cost or intended scale justifies it, change backend=gpu explicitly.
8. Compile the minimized GPU artifact.
9. Run CPU/GPU conformance and visual capture QA.
```

The backend change must remain a normal reviewable source diff.

## Performance claims and budgets

Safe per-effect and per-scene capacity guidance is not guessed in advance. #53 records representative small/medium/stress workloads, CPU memory/update costs, GPU layout/runtime costs, and optional machine-dependent timings.

Recommended LLM authoring budgets are derived from those measurements and include raw evidence.

## Explicit V1 non-goals

Unless a later measured issue deliberately adds them, the first pipeline does not include:

- automatic silent CPU -> GPU switching
- graphical particle editor
- arbitrary user shader/code injection
- generic node graph runtime
- gameplay-authoritative collision particles
- trails/ribbons
- sub-emitter recursion
- general turbulence/noise framework
- custom allocator framework
- job system
- ECS rewrite
- universal cross-vendor bit-identical GPU floating point

## Completion standard

The particle phase is complete when a fresh coding agent can, using repository documentation alone:

1. author a rich text effect,
2. execute it deterministically on CPU,
3. inspect/assert every supported property,
4. obtain reproducible structural cost and optional local timing evidence,
5. explain a CPU/GPU recommendation without changing the backend,
6. respect a human's explicit backend decision,
7. compile GPU-selected effects into minimized runtime state,
8. execute them without duplicate CPU simulation,
9. run documented conformance and visual QA,
10. continue to the next roadmap item from `PROJECT_STATUS.md` without previous chat history.
