# Particle Program, Cost Analysis, and GPU Compile Contract

Issue #51 adds the explicit analysis/compiler gate between the fully Agent-verifiable CPU particle reference backend and the later GPU runtime.

The owner-controlled workflow is:

```text
versioned .trace2d.particle.toml
  -> canonical ParticleEffectAsset
  -> deterministic ParticleProgram
       |-> existing CPU reference semantics
       |-> deterministic structural cost report
       |-> optional local Release timing
       |-> static attribute/random/lifetime analysis
       `-> minimized GPU artifact when backend = "gpu"
  -> HUMAN backend decision remains authored text
  -> #52 executes explicitly GPU-selected artifacts
```

The analyzer never edits an effect, never changes `backend`, and never silently falls back from GPU to CPU.

## One canonical semantic source

`ParticleProgram` is compiled from the already-validated canonical `ParticleEffectAsset`. It does not parse TOML a second time and does not maintain a second interpretation of particle behavior.

For CPU oracle execution, analysis creates an ephemeral CPU execution view from the `ParticleProgram` and prepares the existing `ParticleEmitter2D` / `ParticleReferenceEmitter`. Therefore the workload executes the same lifetime, spawn, keyed-random, update, overflow, and lifecycle implementation already proven by #47-#50.

This setup may allocate because it is explicit tooling/setup work. `ParticleReferenceEmitter::Step()` and `ParticleEmitter2D::Step()` are unchanged by #51.

## Program fingerprint V1

The deterministic program fingerprint is 64-bit FNV-1a over canonical little-endian numeric bytes and exact float bit patterns.

It includes normalized semantic effect data:

- semantic effect ID,
- lifecycle,
- complete authored reference definition,
- ordered bursts,
- blend mode,
- canonical sprite references.

It deliberately excludes:

- runtime-bound global seed,
- runtime-bound emitter stable ID,
- filesystem location / canonical asset path,
- the selected CPU/GPU backend.

Backend selection is a human-controlled shipping choice rather than a different particle semantic program. Changing only `backend = "cpu"` to `backend = "gpu"` therefore preserves the semantic program fingerprint while creating a normal reviewable source diff.

## Static program analysis

The compiler records deterministic masks for:

- features,
- attributes written at spawn,
- attributes read during update,
- attributes written during update,
- presentation-only attributes,
- all rich CPU-reference stored attributes,
- constant attributes,
- values derived instead of stored in the planned GPU representation.

It also records the exact keyed-random channels required by the authored program. Channel discovery follows the existing CPU reference behavior; a fixed range does not consume a random sample, while box/circle spawn consumes the two stable spawn-position channels.

## CPU reference memory

The rich CPU reference remains the semantic oracle and intentionally stores all V1 observable state.

The current reference SoA payload is **92 bytes per admitted particle**, across the existing 13 storage blocks. The structural report obtains prepared payload bytes and steady-state allocation count directly from `ParticleReferenceEmitter::MemoryReport()` instead of maintaining a competing estimate.

`steady_state_step_allocations` is therefore the reference backend's existing measured structural contract. #51 does not add allocation, strings, hashing, filesystem access, or compiler work to ordinary particle stepping.

## CPU semantic operation counts

The structural cost report counts semantic operations rather than pretending that every operation has the same CPU-cycle cost.

For each admitted spawn it counts the keyed-random evaluations actually required by the canonical ranges/shape. Capacity-dropped attempts do not count random evaluation because the current reference backend rejects them before sampling.

For each existing particle update the current CPU reference performs:

```text
apply_acceleration
integrate_position
integrate_rotation
advance_lifetime
```

Particles that survive the lifetime boundary additionally perform:

```text
evaluate_size_over_life
evaluate_color_over_life
```

The report exposes raw totals for every operation plus:

- configured capacity,
- emitter count,
- observed frames,
- current and peak alive count,
- spawn attempts,
- admitted spawns,
- updates,
- expirations,
- drops,
- random evaluations,
- bytes per particle,
- prepared CPU state bytes,
- steady-state simulation allocations.

Looping effects are accumulated across internal `ParticleEmitter2D` reference resets so the report describes the selected analysis window rather than only the final lifecycle cycle.

## Planned GPU layout

The GPU artifact is a deterministic **runtime-layout description for #52**, not a GPU runtime implementation.

The planned packed fields use 4-byte-aligned scalar/vector/color values and omit state that can remain in program constants or be derived from stable runtime values. Examples:

- constant acceleration stays in effect/program constants,
- fixed lifetime is not stored per particle,
- constant initial size is not stored per particle,
- current size is derived from initial size + age/lifetime + end multiplier,
- current rotation is derived from initial rotation + angular velocity + age rather than storing the CPU reference's current rotation field,
- current color is derived from initial color + age/lifetime + end color,
- a fixed sprite is an emitter/program resource rather than a per-particle index,
- spawn ordinal is used as stable spawn identity/random input but is not automatically retained as shipping GPU state after spawn.

A rich CPU effect may therefore use 92 bytes per admitted reference particle while compiling to a materially smaller GPU stride. This is intentional: complete observability on CPU must not force unnecessary shipping GPU bandwidth.

The artifact contains:

- semantic program fingerprint,
- deterministic artifact fingerprint,
- pipeline/layout variant ID,
- capacity,
- ordered fields with offsets/sizes,
- stride,
- planned buffer bytes.

`CompileParticleGpuArtifact` succeeds only when the authored program already says `backend = "gpu"`. A CPU-selected effect produces `backend_not_selected`; the compiler never changes that choice itself.

Until #52 is complete, normal `ParticleEmitter2D::Prepare` still rejects GPU-selected effects with `BackendUnavailable`. There is no silent CPU runtime fallback.

## Analyzer CLI

Build the normal tools target, then run structural analysis from a project root:

```text
trace2d_particle_analyze \
  --project-root <project-root> \
  --effect effects/hit_spark.trace2d.particle.toml \
  --frames 120 \
  --seed 1 \
  --stable-id 77
```

The output is machine-readable JSON with `metric_source = "deterministic_structure"` and includes program/static analysis, the exact CPU reference workload counters/memory/operation counts, and the planned GPU layout.

The analyzer explicitly restarts one playback at frame zero for its workload. `play_on_load` therefore does not make an authored effect impossible to benchmark; lifecycle duration/loop semantics still come from the canonical program.

### Optional local timing

Timing is deliberately opt-in and environment-labelled:

```text
trace2d_particle_analyze \
  --project-root <project-root> \
  --effect effects/heavy.trace2d.particle.toml \
  --frames 240 \
  --timing \
  --warmup 20 \
  --iterations 100 \
  --machine-label "dev-pc" \
  --cpu-model "<CPU model>"
```

Use a **Release build** when timing is intended as performance evidence.

Timing reports include:

- timing scope,
- warmup iterations,
- measured iterations,
- frames per iteration,
- average workload nanoseconds,
- median workload nanoseconds,
- p95 workload nanoseconds,
- nanoseconds per deterministic particle update when updates occurred,
- machine label,
- CPU model,
- logical processor count,
- OS,
- compiler/version,
- build configuration.

Wall-clock timing is explicitly machine-dependent. Hosted CI validates deterministic compiler/report behavior and CLI availability; it must not fail on timing thresholds.

## Recommendation and human decision boundary

#51 intentionally does **not** invent portable `low/moderate/high` or `keep_cpu/consider_gpu` thresholds.

An LLM or developer can explain a recommendation from the raw structural report and optional target-machine timing, but the source remains unchanged until a human deliberately edits:

```toml
[effect]
backend = "cpu"
```

or:

```toml
[effect]
backend = "gpu"
```

#53 owns representative CPU/GPU workloads and safe-budget guidance. Only after measured budgets exist may tooling add a convenience classification, and any such classification must expose the raw evidence and threshold source beside it.

## Performance boundary

All #51 compiler/report/timing work is explicit setup or tooling work. None of it is performed by the ordinary particle frame path.

In particular, #51 adds no:

- automatic backend mutation,
- per-frame program compilation,
- per-frame filesystem/TOML work,
- per-frame JSON/report generation,
- CPU percentage guess,
- generic optimizer/SSA framework,
- expression compiler,
- shader graph,
- reflection framework,
- custom allocator,
- job system.

#52 consumes the minimized GPU artifact/runtime-layout contract. #53 then measures CPU/GPU conformance and establishes evidence-based guidance.
