# XSTRESS1 — P2 cross-platform structural/profile proof

Parent: #330  
Slice: #393

## Purpose

XSTRESS1 reuses the already accepted P2 combat/game-feel workload instead of inventing a benchmark-only scene. The same seed (`329`), fixed timestep (`16,666,667 ns`) and scheduled input sequence run for 90 authoritative frames on Windows/MSVC and Ubuntu/Clang through the installed Trace2D SDK.

The slice asks one bounded question:

> Does the same real public Physics2D + Audio product workload retain the same portable gameplay/structural work across the two maintained compiler/platform paths while producing #91 profile evidence?

It does not claim identical wall-clock performance across unrelated hosted runners and it does not claim Linux real-GPU support.

## Reused authority

- P2 / #371 owns the combat scenario, gameplay assertions and owner-accepted product loop.
- #91 owns `CpuProfiler2D`, `trace2d.profile.report.v1` and `ProfileAdapters2D`.
- #78 owns the maintained Ubuntu 24.04 + Clang 18 build/SDK path.
- #92 owns the separate real-GPU support/evidence contract.

No engine runtime or public API is added for XSTRESS1.

## Workload/profile boundary

`trace2d_p2_combat_gamefeel_profile` profiles only the 90 explicit `Application::StepFrames(1)` calls. It uses one pre-registered scope, retained profiler storage and `steady_clock` timestamps.

After the measured loop completes, and only then, the proof:

1. validates the existing P2 combat result;
2. reads Physics2D, Audio and ResourceRegistry metrics;
3. performs explicit ResourceRegistry inspection;
4. composes the existing 97-metric structural profile vocabulary;
5. builds/writes `trace2d.profile.report.v1`.

There is no per-frame JSON, filesystem output, resource inspection or cross-subsystem profile-name construction in the gameplay hot path.

## Portable comparison

The cross-platform gate compares a frozen subset whose units and semantics are platform-independent counts/capacities, including representative:

- attached body/capacity and fixed-step counts;
- body-command/query/failure counts;
- retained semantic audio capacities and command/step/failure counts;
- ready/error/filesystem-query/resource-snapshot counts.

The gate also requires both reports to carry the same source revision/workload/fixed timestep/sample count, CPU capture availability and an explicit headless `gpu: not_supported` state.

The following evidence is deliberately **not** compared for numeric equality across hosted machines:

- CPU duration nanoseconds;
- operating-system/compiler identity;
- allocator/STL/container byte observations;
- physical GPU timing (not available in this hosted headless slice).

Timing remains environment-labelled evidence only. A later #330 slice may consume the owner real-GPU path, but XSTRESS1 does not weaken #92 by substituting hosted software/headless execution for Tier B evidence.

## External reference decisions

### wgpu testing

Source: `gfx-rs/wgpu` `docs/testing.md`, reviewed 2026-08-24.

Decision: **ADAPT** the separation between validation tests that do not require real hardware and real-GPU integration tests. Hosted Linux remains a portability/structural path rather than a fabricated GPU support claim.

### Godot benchmarks

Source: `godotengine/godot-benchmarks`, reviewed 2026-08-24.

Decision: **ADAPT** one reusable workload with machine-readable JSON results. **REJECT** cross-machine wall-clock thresholds: unrelated hosted runners are not a stable performance budget.

### Direct reuse decision

Reuse P2 + #91 directly. Do not create a second combat workload, profiler, metric registry, timing score or benchmark-specific engine subsystem.

## Completion boundary

XSTRESS1 is complete when exact-head CI proves both installed-SDK platform runs, uploads both reports, and the compare job validates portable structure while retaining timing only as contextual evidence.

#330 remains open afterward for the same-workload owner real-GPU/render stress slice and final product-proof verdict.
