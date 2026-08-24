# XSTRESS2 — P2 owner real-GPU product proof

Parent: #330  
Slice: #395

## Purpose

XSTRESS2 reuses the exact P2 combat/game-feel workload already frozen by P2/#371 and XSTRESS1/#393. It does not create another benchmark scene. The owner Tier B Windows GPU runner executes seed `329`, fixed timestep `16,666,667 ns`, and the same scheduled input sequence for 90 authoritative frames while the production `Renderer` presents the P2 scene every frame.

The bounded question is:

> Does the same P2 gameplay workload that retained portable Physics2D + Audio + ResourceRegistry structure on Windows/Linux also execute through the maintained real-GPU renderer path without changing authoritative results or leaking validation work into ordinary frames?

## Reused authority

- P2 / #371 owns combat behavior and the owner-accepted scene.
- XSTRESS1 / #393 owns the Windows/Linux installed-SDK Release structural + CPU evidence.
- #91 owns profiling/structural vocabulary and the rule that machine timing is environment-labelled evidence.
- #92 owns the maintained `owner-windows-primary` Tier B runner and real-GPU support boundary.

XSTRESS2 adds no engine runtime/public API and does not weaken any of those authorities.

## Execution boundary

`P2GpuConformanceTests.OwnerCombatWorkload` is selected by the existing owner `Gpu(Smoke|Conformance)Tests` gate. Outside explicit `TRACE2D_RUN_GPU_SMOKE=1` validation it exits before creating a window, renderer, filesystem artifact or GPU resource, so normal hosted CTest does not accidentally become a real-GPU requirement.

On the owner GPU path the executable:

1. schedules the exact P2 combat input sequence;
2. steps one authoritative fixed frame at a time for 90 frames;
3. presents the production P2 scene after each step;
4. requests exactly one explicit capture on the final frame;
5. validates frozen combat/Physics2D/Audio/ResourceRegistry outcomes;
6. validates renderer frame/draw/sprite work and the single readback/fence capture boundary;
7. writes `trace2d.xstress2.p2-gpu.v1` evidence only after the run completes.

No JSON, filesystem write, readback or explicit fence exists in ordinary gameplay frames. The final capture is validation-only work.

## Evidence

The owner GPU artifact directory retains:

- `p2-xstress2/p2-owner-gpu-evidence.json`;
- `p2-xstress2/p2-owner-gpu-final.bmp`;
- the existing `trace2d.gpu-gate.v2` manifest and GPU test log.

The XSTRESS2 JSON records source revision, workload/seed/fixed-step identity, renderer backend, frozen combat and subsystem counters, renderer structural counters, target/capacity observations, capture metadata and explicit readback/fence counts.

GPU timing remains `not_supported`. The current public SDL3 GPU surface does not provide a trustworthy timestamp/query-pool path, so XSTRESS2 does not relabel CPU wall time as GPU time.

## Comparison policy

XSTRESS2 compares authoritative gameplay structure against the already frozen P2/XSTRESS1 expectations. It does not compare a new screenshot golden or introduce a global pixel tolerance. The capture is retained as reviewable real-workload evidence; semantic GPU comparison policy remains owned by the dedicated #92 fixtures.

This avoids two bad outcomes:

- turning one owner machine's screenshot into global gameplay truth;
- turning one machine's wall-clock duration into a portable performance threshold.

## External-reference decision

The prior XSTRESS1 reference review remains applicable: adapt the wgpu-style separation between backend-independent validation and real-hardware integration, and retain Godot-benchmark-style machine-readable evidence without cross-machine wall-clock pass/fail thresholds. XSTRESS2 narrows that principle to Trace2D's already maintained Tier B owner GPU rather than inventing broader vendor/backend claims.

## Completion boundary

XSTRESS2 is complete when exact-head hosted CI and P2 product proof remain green, the exact-head owner GPU gate passes with the P2 workload selected, and the uploaded XSTRESS2 JSON + BMP prove one explicit capture with frozen gameplay structure.

Afterward #330 needs only a final combined product-proof audit/closeout if no runtime gap is demonstrated.
