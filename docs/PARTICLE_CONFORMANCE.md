# Particle CPU/GPU conformance, workloads, and backend guidance

Issue: #53  
Parent: #46  
Last reviewed: 2026-08-10

This document closes the seventh and final particle slice. It connects text-authored effects, the deterministic CPU reference oracle, structured Agent verification, measured CPU cost, explicit human backend choice, deterministic GPU compilation, real GPU execution, and bounded CPU/GPU conformance.

The governing rule is:

> **Verify semantics on the CPU oracle, measure raw cost, let a human choose the backend explicitly, then prove the selected GPU path against the same program/seed/identity.**

Trace2D never silently changes `backend = "cpu" | "gpu"`, never treats GPU floating point as universally bit-identical, and never adds normal-frame GPU readback merely for diagnostics.

## 1. End-to-end contract

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

`PrepareParticleProgramCpuEmitter()` may execute the same semantic program through the CPU reference oracle for analysis even when the authored effect selects GPU. That analysis path does not mutate authored backend state and is not simultaneous CPU simulation during normal GPU execution.

## 2. Conformance boundary

### Exact structured facts

The following are exact where represented by Trace2D-owned integer/structural state:

- `ParticleProgram` fingerprint,
- deterministic GPU artifact/pipeline identity,
- authored backend and capacity,
- emission schedule and submitted spawn attempts,
- integer lifetime sampling/boundaries,
- feature/random-channel identity,
- minimized GPU layout/stride,
- retained GPU bytes,
- dispatch/draw/resource counters,
- buffer creation/growth/reuse facts,
- zero normal-frame particle readback/fence waits,
- unsupported GPU feature rejection with no CPU fallback.

### Floating/presentation facts

Trace2D does **not** claim universal cross-vendor bit-identical GPU particle floats.

The committed V1 real-GPU probe therefore combines the exact structured facts above with a bounded presentation invariant:

- same program, global seed, and emitter stable ID,
- same exact frame steps,
- CPU oracle provides expected world motion/lifetime,
- actual SDL GPU presentation is captured through the explicit `CaptureFrame` verification path,
- bright-particle center must remain within 2 pixels of the CPU-projected center in the fixed 128x128 probe,
- positive X velocity must move presentation in the expected direction,
- disappearance must occur on the CPU-sampled integer lifetime boundary.

The 2-pixel value is a fixed-probe raster tolerance, not a general world-space physics tolerance. A future direct GPU-state diagnostic belongs to #91 if evidence shows it is necessary.

## 3. Real-GPU proof

The opt-in hardware tests use:

```powershell
$env:TRACE2D_RUN_GPU_SMOKE = "1"
ctest --preset windows-debug -R "ParticleGpu(Smoke|Conformance)Tests" --output-on-failure
```

Hosted CI may build/discover these tests and skip them without a presentation GPU. A skip is not hardware evidence.

Final #53 evidence was captured on PR #114 commit `924dbc19027a350c9bae819eea28789eea77bbdd` on:

- AMD Ryzen 5 5600X,
- NVIDIA GeForce RTX 3070,
- driver `32.0.15.9186`,
- Windows/MSVC,
- Debug real-GPU tests + Release analyzer timings.

Both required tests passed, not skipped:

- `ParticleGpuConformanceTests.ExplicitGpuExecutionTracksCpuOracleAcrossRandomSpawnMotionAndLifetime`
- `ParticleGpuSmokeTests.ExplicitGpuEmitterAdvancesCapturesAndReusesCapacity`

Repository-safe evidence is committed under [`docs/evidence/particle-53/924dbc1/`](evidence/particle-53/924dbc1/README.md). Local checkout/user-profile paths in the textual GPU log are redacted only for public-release safety; pass/fail evidence and timing are unchanged.

## 4. Calibration workloads and structural evidence

The repository commits three deterministic 240-frame calibration effects:

| Workload | Authored backend | Capacity | Purpose |
| --- | --- | ---: | --- |
| `workload_cpu_small` | `cpu` | 128 | small rich CPU reference point |
| `workload_cpu_medium` | `cpu` | 1024 | representative rich CPU point |
| `workload_gpu_scale` | `gpu` | 4096 | heavier scale point for explicit GPU consideration |

The analyzer reports machine-independent structural evidence including program/backend identity, current/peak alive count, spawn/update/expire/drop totals, 92-byte CPU reference payload accounting, semantic operation counts, random channels, steady-state allocation count, planned GPU layout, and deterministic artifact identity when GPU is authored.

All three final-gate workloads reported:

- zero dropped particles,
- zero steady-state particle-step allocations,
- unchanged authored backend (`backend_changed_by_analyzer: false`).

## 5. Release timing evidence

Timing uses 10 warmup iterations and 50 measured iterations. Each measured iteration executes a deterministic 240-frame `particle_emitter_step_window`; setup/reset is outside the measured interval.

The analyzer reports average/median/p95 across those 240-frame windows. Therefore the normalized values below are **p95 window total / 240**, not an individual-frame latency histogram.

| Workload | Peak alive | Updates / 240f | p95 window | Normalized p95 / frame |
| --- | ---: | ---: | ---: | ---: |
| `workload_cpu_small` | 42 | 8,333 | 0.2286 ms | 0.0009525 ms |
| `workload_cpu_medium` | 856 | 179,234 | 6.9978 ms | 0.0291575 ms |
| `workload_gpu_scale` | 3,400 | 716,305 | 25.8222 ms | 0.1075925 ms |

Important: `workload_gpu_scale` is authored as GPU, but analyzer timing explicitly uses `analysis_execution_backend = "cpu_reference_oracle"`. Its timing is CPU-oracle cost for the same semantic program, **not GPU wall-clock timing**. GPU correctness/runtime behavior is proven by the real-GPU tests and structural metrics above.

Timing remains machine-dependent evidence and must not become a hosted-CI wall-clock gate.

## 6. V1 CPU recommendation budget

The final calibration creates an intentionally conservative Trace2D V1 recommendation policy for a representative target machine:

- **keep CPU comfort band:** normalized p95 240-frame-window cost `<= 0.05 ms/frame`,
- **human judgment band:** `> 0.05 ms/frame` and `< 0.10 ms/frame`,
- **consider GPU:** normalized p95 cost `>= 0.10 ms/frame`, or desired scale materially exceeds the measured CPU envelope.

These bands are derived from the committed calibration gap: the rich 1024-capacity / 856-peak workload measured `0.0291575 ms`, while the 4096-capacity / 3400-peak semantic workload measured `0.1075925 ms` on the recorded Ryzen 5 5600X Release run.

The bands are **recommendation thresholds, not automatic switching rules and not universal particle-count limits**. Feature mix, emitter count, target CPU, and game-wide CPU budget still matter. A target project should rerun the analyzer and may adopt a stricter budget.

Accordingly, the measured V1 reference guidance is:

- the committed rich medium workload is a safe `keep_cpu` reference envelope on the recorded machine,
- the committed scale workload is a valid `consider_gpu` reference envelope,
- capacity alone never determines the backend.

Tooling/LLMs may emit `keep_cpu` / `consider_gpu` only when they include the threshold source and raw measurements. They must never rewrite backend text.

## 7. Human backend decision

For an authored effect:

1. verify exact CPU semantics first,
2. inspect capacity, peak alive, state bytes, operation counts, drops, and allocations,
3. run Release timing on a representative target machine when cost matters,
4. compare normalized p95 evidence with the project budget/guidance bands,
5. keep CPU when comfortably inside budget for maximal observability/simplicity,
6. consider GPU when measured cost or desired scale is material,
7. change `backend` explicitly in authored text only after human review,
8. re-run GPU conformance and presentation QA.

Final backend ownership remains human-controlled and reviewable in text.

## 8. Agent authoring checklist

A coding Agent should:

1. author a bounded `.trace2d.particle.toml`,
2. normalize/validate it before runtime,
3. keep backend explicit,
4. verify exact CPU frames and bounded particle details,
5. run `trace2d_particle_analyze` for structural evidence,
6. distinguish deterministic metrics from local timing,
7. include raw timing + threshold source in any recommendation,
8. wait for the human backend decision,
9. compile deterministic GPU artifacts only for authored GPU effects,
10. run real-GPU conformance/capture QA,
11. keep perceptual style review separate from engine-owned semantic/cost evidence.

## 9. External reference decisions

The #53 design was refreshed against primary sources on 2026-08-10:

- SDL3 GPU API — **ADOPT** explicit verification-only synchronization and persistent resource reuse; normal particle frames remain readback/fence-free.
- Microsoft Direct3D floating-point rules — **ADAPT** exact discrete checks plus bounded float/presentation invariants; reject universal float-bit identity.
- Vulkan specification — **ADAPT** the same explicit numeric portability boundary.
- Google Benchmark user guide — **ADAPT** warmup/repetitions/statistics/environment labelling through the existing dependency-free analyzer; no new benchmark dependency.

## 10. #53 exit evidence

The final particle slice has evidence for:

- hosted configure/build/full tests,
- deterministic calibration workload tests,
- real Windows GPU smoke pass without skip,
- real Windows GPU conformance pass without skip,
- local Release raw timing on the committed workloads,
- measured recommendation bands with evidence and machine metadata,
- explicit human backend ownership,
- deterministic GPU artifact/layout evidence,
- zero normal-frame GPU particle readback/fence wait,
- zero normal GPU-mode CPU reference duplication by contract.

After PR #114 merges green, #53 and umbrella #46 may close and the fixed core lane advances to #97 machine-readable intent / Definition of Done.
