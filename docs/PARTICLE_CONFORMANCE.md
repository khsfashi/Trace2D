# Particle CPU/GPU conformance, workloads, and backend guidance

Issue: #53  
Parent: #46  
Last reviewed: 2026-08-10

This document closes the design contract for the seventh particle slice. It connects the existing text-authored effect, CPU reference oracle, Agent verification, structural cost analyzer, explicit GPU compiler/runtime, and real-GPU presentation path without adding automatic backend switching or normal-frame diagnostic work.

The governing rule is:

> **Verify semantics on the CPU oracle, measure raw cost, let a human choose the backend explicitly, then prove the selected GPU path against the same program/seed/identity.**

The particle phase does not introduce a profiler framework, a generic GPU state debugger, a hidden CPU fallback, or a heuristic that rewrites authored `backend` text.

## 1. End-to-end workflow

```text
.trace2d.particle.toml
  -> validated ParticleEffectAsset
  -> deterministic ParticleProgram
  -> CPU reference execution
  -> exact-frame Agent inspection/assertions
  -> deterministic structural cost report
  -> optional environment-labelled Release timing
  -> human backend decision
  -> authored backend = "cpu" | "gpu"
  -> deterministic GPU artifact when gpu is selected
  -> explicit GPU execution/presentation
  -> conformance + visual QA
```

The CPU reference remains available as the semantic oracle for analysis even when the authored effect selects `backend = "gpu"`. `PrepareParticleProgramCpuEmitter()` deliberately clones the program definition into a CPU-selected oracle; it does not mutate the authored program.

## 2. What conformance means

Trace2D does **not** claim cross-vendor bit-identical floating-point GPU particle state.

Conformance is layered instead of reduced to a screenshot hash.

### 2.1 Exact deterministic checks

These are exact where they are represented by Trace2D-owned integer/structural state:

- `ParticleProgram` fingerprint,
- compiled GPU artifact fingerprint,
- pipeline variant and minimized layout identity,
- authored capacity,
- step count,
- submitted spawn-attempt count,
- burst/periodic emission schedule,
- spawn-ordinal progression implied by submitted attempts,
- integer lifetime sampling and lifetime boundary behavior,
- explicit backend selection,
- unsupported-feature rejection with no fallback,
- retained-buffer creation/reuse counters,
- no normal-frame GPU readback or fence wait.

The existing GPU runtime owns scheduler control on the CPU side and the GPU compute path owns particle state. Conformance tests therefore compare the CPU oracle's deterministic counters against the GPU runtime's submitted structural facts and then verify the resulting presentation behavior.

### 2.2 Floating-point checks

Position, motion, rotation, size, and color are not required to be universally bit-identical across GPU vendors/drivers.

For the committed V1 real-GPU conformance probe, floating-point motion is checked as a **presentation-space invariant**:

- same `ParticleProgram`, global seed, and emitter stable ID,
- same exact frame steps,
- CPU oracle supplies the expected world position,
- actual GPU presentation is captured through the existing explicit `CaptureFrame` path,
- the bright-particle center must land within **2 pixels** of the CPU-projected center in the fixed `128x128`, vertical-size `4.0` probe,
- positive fixed X velocity must move the observed center in the expected direction,
- the particle must disappear on the exact CPU-sampled integer lifetime boundary.

The 2-pixel value is a **test-space rasterization tolerance**, not a world-space physics tolerance and not a general engine rendering quality threshold. Future workloads that need tighter numeric state diagnosis should add a dedicated explicit diagnostic owned by #91 rather than adding readback to normal particle frames.

### 2.3 Why no particle-buffer readback API is added here

A storage-buffer readback would require an explicit GPU download plus synchronization before CPU inspection. That is useful for deep diagnostics, but #53 can prove the current V1 runtime with the already-supported explicit capture path plus structural metrics.

Decision for #53: **REJECT a new public particle-buffer readback surface for the ordinary particle API.**

Reasons:

- it would add a new synchronized diagnostic path solely to close this stage,
- it is unnecessary for the representative V1 conformance proof,
- it risks blurring the hard rule that normal GPU execution performs no readback/fence wait,
- #91 already owns unified profiler/diagnostic expansion if future failures justify direct GPU-state inspection.

This does not prohibit an explicit future diagnostic readback. It prohibits making it part of normal particle execution or inventing it before evidence requires it.

## 3. Real-GPU conformance test

`tests/render/ParticleGpuConformanceTests.cpp` is opt-in under the same hardware gate as the existing GPU smoke test:

```powershell
$env:TRACE2D_RUN_GPU_SMOKE = "1"
ctest --preset windows-debug -R ParticleGpuConformanceTests --output-on-failure
```

The probe uses:

- one GPU-selected effect,
- one CPU oracle prepared from the same `ParticleProgram`,
- fixed global seed and emitter stable ID,
- random box spawn position,
- random integer lifetime,
- fixed positive-X motion,
- actual SDL GPU execution,
- actual offscreen/render capture,
- structural runtime metrics.

The test proves that random-channel mapping is observable in the same location/lifetime outcome, that GPU presentation follows CPU motion within the declared raster tolerance, and that the same retained particle buffer is reused without normal-frame readback/fence synchronization.

Hosted CI may compile/discover this test and skip it when the opt-in environment variable is absent. A skip is **not** real-GPU evidence.

## 4. Deterministic calibration workloads

The repository commits three calibration effects. They are measurement points, **not guessed safe limits**.

| Workload | Backend text | Capacity | Purpose |
| --- | --- | ---: | --- |
| `workload_cpu_small` | `cpu` | 128 | small observable rich effect |
| `workload_cpu_medium` | `cpu` | 1024 | medium rich CPU scaling point |
| `workload_gpu_scale` | `gpu` | 4096 | heavier rich effect for explicit GPU consideration |

All three use deterministic 240-frame windows and exercise variable lifetime, randomized spawn/motion/presentation attributes, acceleration, over-life data, and bounded storage. The GPU-scale fixture avoids V1-unsupported variable `SpriteChoice`.

Normal CI executes structural analysis for all three and validates that analysis never mutates the authored backend. Wall-clock timing remains unscored.

## 5. Structural evidence

Run the analyzer without `--timing` when you need machine-independent evidence:

```powershell
.\build\windows-msvc\tools\particle_analyze\Debug\trace2d_particle_analyze.exe `
  --project-root . `
  --effect tests/particles/fixtures/workload_cpu_small.trace2d.particle.toml `
  --frames 240 `
  --seed 1311768467463790320 `
  --stable-id 1234605616436508552
```

Repeat with `workload_cpu_medium` and `workload_gpu_scale`.

The JSON authority includes:

- effect/program identity and selected backend,
- feature/random-channel/attribute sets,
- configured capacity,
- current/peak alive count,
- spawn/update/expire/drop counters,
- exact 92-byte CPU reference particle payload accounting,
- prepared CPU-state bytes,
- steady-state simulation allocations,
- semantic operation totals,
- planned minimized GPU stride/buffer bytes,
- GPU artifact status/fingerprint when the authored backend selects GPU,
- `backend_changed_by_analyzer: false`.

Structural evidence can be compared exactly across machines where the supported deterministic contracts hold.

## 6. Local Release timing evidence

Timing is machine evidence, never a deterministic CI gate.

Build Release:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-release --parallel
```

Then run each calibration workload with truthful environment labels:

```powershell
.\build\windows-msvc\tools\particle_analyze\Release\trace2d_particle_analyze.exe `
  --project-root . `
  --effect tests/particles/fixtures/workload_cpu_small.trace2d.particle.toml `
  --frames 240 `
  --seed 1311768467463790320 `
  --stable-id 1234605616436508552 `
  --timing `
  --warmup 10 `
  --iterations 50 `
  --machine-label "<machine-label>" `
  --cpu-model "<cpu-model>"
```

The timing object reports average, median, p95, and nanoseconds per particle update together with OS/compiler/build/environment metadata. Reset/setup is outside the measured step window.

Do not compare two machines as if their wall-clock numbers were deterministic truth. Do not fail hosted CI because a shared runner crosses a timing number.

## 7. Human backend decision

Trace2D V1 intentionally emits raw evidence rather than an automatic backend recommendation threshold.

Use this process:

1. Author and validate the effect on the CPU oracle.
2. Inspect capacity, peak alive count, prepared bytes, operation counts, drops, and steady-state allocation evidence.
3. If runtime CPU cost matters, run the local Release timing workload on a representative target machine.
4. Compare the measured effect/workload against the **project's explicit CPU frame budget**, if the game has one.
5. If the cost is comfortably inside that budget, keep `backend = "cpu"` for maximal observability and simplicity.
6. If measured cost or required scale is material, inspect GPU compiler/runtime support and change the authored text to `backend = "gpu"` deliberately.
7. Re-run the real-GPU conformance test and capture visual QA.
8. Keep the final backend choice in reviewable authored text.

No analyzer, Agent, or build step may rewrite the backend merely because an effect resembles one of the calibration workloads.

### Safe-budget publication gate

The 128/1024/4096 fixtures are calibration points only. A public Trace2D statement such as "CPU particles are safe up to X" must not be written until:

- the raw Release measurements are recorded,
- the target machine/build/workload is identified,
- the project CPU budget used for the recommendation is stated,
- the recommendation is conservative relative to measured p95 rather than inferred from capacity alone.

Until then, the correct guidance is **measure, do not guess**.

## 8. Agent authoring checklist

A fresh coding Agent should follow this order:

1. author a bounded `.trace2d.particle.toml`,
2. load/normalize it before runtime,
3. keep `backend` explicit,
4. verify exact CPU frames with Agent inspection/assertions,
5. request bounded particle details only when needed,
6. run `trace2d_particle_analyze` for deterministic structural cost,
7. distinguish structural evidence from optional local timing,
8. present raw measurements to the human rather than silently changing backend,
9. if GPU is selected, compile the deterministic artifact and reject unsupported features explicitly,
10. run the real-GPU smoke/conformance gate,
11. use capture/multimodal review only for presentation/style questions,
12. keep final creative/backend approval human-controlled.

## 9. External reference review

The design was refreshed against primary sources on 2026-08-10.

- SDL3 GPU API — https://wiki.libsdl.org/SDL3/CategoryGPU
  - Lesson: GPU download/readback requires an explicit copy/download and synchronization; resources should be reused rather than churned.
  - Decision: **ADOPT** the explicit-verification-only synchronization boundary and persistent-resource rule.
  - Trace2D evidence: normal GPU particle metrics remain readback/fence-free; conformance uses the already-explicit capture path.

- Microsoft Direct3D floating-point rules — https://learn.microsoft.com/en-us/windows/win32/direct3d11/floating-point-rules
  - Lesson: shader floating-point evaluation is not a portable basis for universal bit-identical claims.
  - Decision: **ADAPT** into exact discrete checks plus bounded floating/presentation invariants.
  - Trace2D evidence: CPU oracle owns semantic expectation; real-GPU probe uses a declared raster tolerance instead of float-bit equality.

- Vulkan specification — https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html
  - Lesson: shader floating-point capabilities/controls are device-dependent enough that portability claims require an explicit contract.
  - Decision: **ADAPT**; no universal cross-vendor float-bit identity claim.
  - Trace2D evidence: same layered conformance contract as above.

- Google Benchmark user guide — https://google.github.io/benchmark/user_guide.html
  - Lesson: warmup/repetitions/statistics and environment context matter for performance evidence.
  - Decision: **ADAPT**, not add as a dependency.
  - Trace2D evidence: the existing dependency-free analyzer already reports warmup, repetitions, average/median/p95 and environment metadata while keeping timing out of deterministic CI equality.

## 10. Exit evidence for #53

#53 can close only when the final PR records all of the following from its actual final head:

- hosted configure/build/full tests green,
- calibration workload structural tests green,
- real Windows presentation-GPU `ParticleGpuSmokeTests` pass, not skip,
- real Windows presentation-GPU `ParticleGpuConformanceTests` pass, not skip,
- local Release raw timing for the committed calibration workloads,
- conservative backend guidance derived from those measurements and an explicitly stated budget basis,
- no automatic backend mutation,
- no normal-frame GPU readback/fence wait,
- final `PROJECT_STATUS.md` handoff advanced to #97 only after those gates are satisfied.
