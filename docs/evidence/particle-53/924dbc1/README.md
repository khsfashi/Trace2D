# Particle #53 final-gate evidence — 2026-08-10

This directory records the local hardware evidence used to close #53. The gate was executed against PR #114 head commit `924dbc19027a350c9bae819eea28789eea77bbdd`; later commits in the PR only record and interpret this evidence.

## Environment

- Machine label: `my-pc`
- CPU: AMD Ryzen 5 5600X 6-Core Processor (12 logical processors)
- GPU: NVIDIA GeForce RTX 3070
- GPU driver: `32.0.15.9186`
- Configure preset: `windows-msvc`
- Real-GPU test preset: `windows-debug`
- Timing build preset: `windows-release`
- Compiler: MSVC `193833145`
- Timing: 10 warmup iterations, 50 measured iterations, 240 deterministic frames per iteration

## Real-GPU gate

Both opt-in presentation-GPU tests passed and neither skipped:

- `ParticleGpuConformanceTests.ExplicitGpuExecutionTracksCpuOracleAcrossRandomSpawnMotionAndLifetime`
- `ParticleGpuSmokeTests.ExplicitGpuEmitterAdvancesCapturesAndReusesCapacity`

`gpu-tests.txt` is a repository-safe transcription of the gate output. Only local checkout/user-profile paths were replaced with `<checkout>` / `<user-temp>` so the public-release audit does not publish a private Windows user path. Test names, pass/fail status, durations, and all relevant evidence are unchanged. The source archive's original SHA-256 is retained in `manifest.json`.

## Release CPU-oracle calibration

The analyzer measures a 240-frame CPU-reference step window. Therefore the last column below is the measured p95 **window total divided by 240**, a normalized per-frame equivalent. It is not a histogram p95 of individual frames.

| Workload | Authored backend | Capacity | Peak alive | CPU updates / 240f | Prepared CPU bytes | p95 / 240f window | p95 normalized per-frame equivalent |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `workload_cpu_small` | `cpu` | 128 | 42 | 8,333 | 11,776 | 0.2286 ms | 0.0009525 ms |
| `workload_cpu_medium` | `cpu` | 1,024 | 856 | 179,234 | 94,208 | 6.9978 ms | 0.0291575 ms |
| `workload_gpu_scale` | `gpu` | 4,096 | 3,400 | 716,305 | 376,832 | 25.8222 ms | 0.1075925 ms |

All three report zero steady-state simulation allocations and zero dropped particles in the committed window.

`workload_gpu_scale` is authored as `backend = "gpu"`, but its timing object explicitly reports `analysis_execution_backend = "cpu_reference_oracle"`. Its `0.1075925 ms` normalized value is therefore **CPU-oracle cost evidence for the same semantic program, not GPU execution timing**. The real GPU path is proven here by conformance/runtime tests and structural metrics, not by a GPU wall-clock benchmark.

## V1 recommendation budget

For Trace2D V1 particle **recommendation guidance**, use the committed calibration gap as the threshold source on a representative target machine:

- `keep_cpu` comfort band: normalized p95 240-frame-window cost `<= 0.05 ms/frame`,
- judgment band: `> 0.05 ms/frame` and `< 0.10 ms/frame`,
- `consider_gpu`: normalized p95 240-frame-window cost `>= 0.10 ms/frame`, or required scale materially exceeds the measured CPU envelope.

The `0.05 / 0.10 ms` bands deliberately bracket the measured rich CPU-medium point (`0.0291575 ms`) and the heavier scale point (`0.1075925 ms`) rather than deriving a fake universal capacity threshold. They are an engine authoring/recommendation policy, not a claim that every effect with 1,024 particles is safe or every effect with 4,096 particles requires GPU execution.

On this recorded Ryzen 5 5600X Release run, the committed rich `1,024`-capacity / `856`-peak workload falls inside the `keep_cpu` comfort band. The `4,096`-capacity / `3,400`-peak semantic workload crosses the `consider_gpu` band. Different feature mixes and target machines must be measured rather than classified from capacity alone.

The analyzer may surface these bands only together with the raw measurement and threshold source. It must never rewrite authored backend text. The final CPU/GPU choice remains human-controlled.

## Files

- `manifest.json` — source gate commit, machine/GPU/build metadata, source hashes, and normalized summary
- `workload_cpu_small.json` — raw structural + Release timing evidence
- `workload_cpu_medium.json` — raw structural + Release timing evidence
- `workload_gpu_scale.json` — raw structural + Release CPU-oracle timing plus deterministic GPU artifact evidence
- `gpu-tests.txt` — path-sanitized real presentation-GPU test output
