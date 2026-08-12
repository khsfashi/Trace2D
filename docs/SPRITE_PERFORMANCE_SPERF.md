# Sprite Performance Evidence and Guidance — SPERF

Status: **active via #172**  
Parent: **#59 Complete Sprite program**  
Prerequisite: **SE2E #170 complete**  
Exact successor after SPERF merges green: **#103 Benchmark B1**

Machine-readable contract: [`contracts/sprite-performance-sperf.json`](contracts/sprite-performance-sperf.json).

## 1. Purpose

SPERF closes #59 by making the performance characteristics already exposed by the completed Sprite implementation reproducible and reviewable. It is an evidence/composition stage, not another runtime feature stage.

The authority chain remains:

```text
canonical SpriteAsset / SPP evidence
 -> SA0-SA4 deterministic animation authority
 -> SR0-SR8 production presentation authority
 -> existing structural/runtime metrics
 -> explicit SPERF evidence collection
 -> optional machine-labelled timing
 -> practical guidance / #103 benchmark handoff
```

SPERF does **not** add a second renderer, animator, resource manager, profiler, memory tracker or runtime reporting loop. Generic profiling remains owned by #91.

## 2. Evidence classes

SPERF keeps four evidence classes separate.

### 2.1 Portable deterministic structure

These are exact Trace2D-owned values and may be correctness-gated:

- submitted / visible / culled Sprite counts,
- visible quad count,
- contiguous compatibility-run count,
- committed workload identity,
- animation replay state/emissions/digest,
- current built-in Sprite vertex payload bytes known by Trace2D,
- exact decoded RGBA8 page byte arithmetic when that representation is explicitly present.

### 2.2 Trace2D-owned retained capacity

`RenderMetrics.spriteVertexCapacityBytes` is a high-water reusable Sprite vertex-buffer capacity owned by the renderer. It is useful retained-capacity evidence, but it is not the same metric as bytes uploaded during one frame.

SPERF must report these separately:

```text
per-frame uploaded vertex bytes
retained reusable vertex capacity bytes
```

Neither value is presented as total GPU memory or driver allocation.

### 2.3 Environment-dependent timing

Wall-clock timing is local evidence only. It must include the machine/build metadata required by the owning workload tool, use Release for performance collection, discard warmup work, and use repeated measurements. Hosted/shared CI never fails because a microsecond threshold moved.

### 2.4 External/driver memory

SDL GPU resources are opaque backend objects. Trace2D therefore reports only byte sizes it actually owns or can derive from its own explicit payload/format contracts. Driver heaps, alignment, residency, hidden copies, compression, mip allocation and backend bookkeeping are not inferred from a Sprite page or vertex-buffer payload.

## 3. Frozen renderer structural workload

SR8 remains the renderer conformance authority. Its committed production-Sprite structural workload is reused unchanged:

```text
schema             trace2d.sprite-renderer-workload.v1
submitted_sprites  1024
visible_sprites     768
culled_sprites      256
visible_quads       960
contiguous_runs       7
metric_source       deterministic_structure
```

The workload preserves semantic painter order. Culling does not split compatible visible runs, while explicit compatibility transitions do.

This workload proves scaling structure; it is not a portable frame-time claim.

## 4. Current built-in Sprite vertex payload

The production SR7 path already exposes and tests:

```text
vertices_per_quad = 6
bytes_per_vertex  = 48
bytes_per_quad    = 288
```

Therefore the frozen SR8 workload's exact Trace2D-owned visible vertex payload is:

```text
960 visible quads * 288 bytes/quad = 276,480 bytes
```

That is the payload represented by `spritePresentationUploadedVertexBytes` for the corresponding visible geometry. It is **not** a claim that the graphics driver allocates exactly 276,480 bytes.

Retained capacity uses the existing `spriteVertexCapacitySprites` / `spriteVertexCapacityBytes` metrics. `spriteVertexCapacitySprites` is capacity in six-vertex quad slots, so the current invariant is:

```text
spriteVertexCapacityBytes == spriteVertexCapacitySprites * 288
```

Capacity may exceed a frame's required payload because SR7 intentionally retains reusable high-water capacity to avoid repeated allocation/recreation.

## 5. Compatibility fragmentation guidance

The older supplementary `trace2d_renderer_workload` set remains useful for illustrating draw-run structure:

| workload | authored | visible | culled | contiguous texture runs |
| --- | ---: | ---: | ---: | ---: |
| `dense_single_texture` | 400 | 400 | 0 | 1 |
| `alternating_two_textures` | 400 | 400 | 0 | 400 |
| `interleaved_culling` | 600 | 400 | 200 | 1 |

The important production rule is not “sort by texture”. SR4/SR7 painter order is semantic authority. Performance guidance is instead to author/arrange compatible consecutive presentation work where semantics permit it. A resource identity never authorizes a global reorder.

The supplementary workload tool predates the full SR7 presentation path, so its wall-clock timing must not be presented as a complete production `SpritePresentationRenderData` benchmark. Its deterministic run counts remain valid supplementary structure evidence.

## 6. Animation CPU evidence

SPERF reuses all three SA4 workloads without changing semantics:

| workload | stress |
| --- | --- |
| `steady_loop_rational` | 6,000 advances, retained `2/3` rational speed, loop/event boundaries |
| `dense_event_ping_pong` | 4,000 advances, dense events, `5/4` speed, repeated ping-pong bounces |
| `large_step_multi_wrap` | 512 large reverse-loop advances crossing multiple wraps/events |

Structural mode executes from fresh state and proves identical semantic replay through `trace2d.sprite-animation-workload.v1` output and the semantic transcript digest.

Runtime complexity remains owned by SA2/SA4: successful advance cost is proportional to crossed semantic boundaries/events, current observation is fixed scalar work, and emission storage is caller-owned. SPERF adds no timing/hash/reporting operation to `SpriteAnimator2D::Advance`.

For optional local timing, use the existing tool in Release:

```text
trace2d_sprite_animation_workload \
  --workload <name> \
  --timing \
  --machine-label <label> \
  --warmup <N> \
  --iterations <N>
```

The tool reports repeated-window average/median/p95 evidence. Those values are environment evidence, not portable budgets.

## 7. Canonical asset / atlas memory guidance

Memory evidence must identify the representation being measured.

### Canonical metadata

`SpriteAsset` page/region/animation metadata is CPU canonical state. Do not estimate it by multiplying atlas dimensions; metadata and pixel storage are separate categories.

### Decoded RGBA8 pixels

When a page is explicitly retained as tightly packed decoded RGBA8, exact payload bytes are:

```text
width * height * 4
```

Examples:

```text
2048 * 2048 * 4 = 16,777,216 bytes = 16 MiB
4096 * 4096 * 4 = 67,108,864 bytes = 64 MiB
```

Use checked integer arithmetic in tooling before applying this formula to untrusted dimensions.

### Packaged / GPU representation

Do not permanently model production page residency as the decoded RGBA8 formula. #70 owns package policy and #86 owns unified resource lifetime/residency. Later compressed formats, mip chains, streaming/lifetime decisions or backend alignment may make packaged/GPU representation differ materially from source/decoded bytes while canonical Sprite pixel geometry remains unchanged.

### Atlas utilization

SPP0's explicit atlas facts remain the authority for exact page area, region area, overlap/out-of-bounds and utilization evidence. SPERF does not create a second atlas analyzer.

## 8. Offline processing/generation boundary

SPP0-SPP5 remains explicit offline work. Its costs do not belong in normal animation/render frame budgets.

Useful frozen complexity guidance includes:

- SPP0 per-frame pixel scan: linear in pixels; selected duplicate/overlap checks may be bounded offline pairwise work,
- SPP1 component discovery: linear in source pixels plus extracted output copy,
- SPP2 base evidence: linear in frame pixels; block repair `O(sum(B log B))`; bounded palette baseline `O(visible pixels * palette size)` with palette size capped at 256,
- SPP3 generic/loose conversion: `O(frame count)`; Aseprite JSON conversion also includes manifest bytes/tags,
- SPP4: `O(manifest bytes + frames)` plus explicit animation-row ordering,
- SPP5: provider-neutral orchestration reuses SPP2/SPP3/SPP4 costs after concrete provider output exists.

Live generation-provider latency/cost is intentionally not a reproducible engine-performance metric. A provider can be separately evaluated later, but it cannot become a CI correctness threshold for the canonical Sprite runtime.

## 9. Final evidence gate

Run:

```powershell
pwsh -File scripts/sprite_performance_final_gate.ps1
```

The gate:

1. binds output to the exact Git commit,
2. loads and hashes `trace2d.sprite-performance.v1`,
3. runs SR8 backend-independent conformance verbosely and parses the frozen structural marker,
4. validates the three supplementary renderer workload structures,
5. runs all three SA4 animation workloads in structural mode,
6. validates their schemas/status/replay flag,
7. records the current built-in vertex payload contract and derived 276,480-byte SR8 payload,
8. hashes every component evidence file,
9. writes `trace2d.sprite-performance-final-gate.v1` `manifest.json`.

The default final gate needs no live provider and introduces no new real-GPU truth model. SR8's already-completed trusted real-GPU evidence remains renderer presentation authority.

Optional local timing stays outside the default deterministic gate. If timing is collected, preserve the workload tool's environment metadata beside the deterministic manifest rather than merging the time into portable pass/fail criteria.

## 10. Hot-path contract

SPERF adds **zero mandatory normal-frame work**.

Forbidden additions for this stage:

- per-frame JSON/string construction,
- per-frame filesystem access,
- per-frame wall-clock sampling,
- background transcript hashing,
- new Agent/MCP snapshot maintenance,
- automatic GPU readback/fence waits,
- heap allocation solely for performance reporting,
- a new global cache only to duplicate metrics already exposed by SR7/SA4.

Evidence allocation, hashing, JSON and filesystem operations belong only to explicit tools/tests/scripts.

## 11. External reference decisions — 2026-08-13

### Google Benchmark user guide — ADAPT / REJECT

Official Google Benchmark guidance supports warmup periods, repeated measurements, aggregate statistics and machine/context metadata. SPERF adapts those measurement practices for optional local evidence.

SPERF rejects a new benchmark dependency for this bounded stage and rejects shared-CI wall-clock thresholds as correctness gates because Trace2D already has focused dependency-free workload runners and exact structural evidence.

Reference: https://google.github.io/benchmark/user_guide.html

### SDL3 GPU transfer/resource documentation — ADOPT / ADAPT

SDL3 documents GPU buffers/transfer buffers as opaque resource handles and exposes explicit transfer-buffer upload operations. Trace2D adopts the separation between application-known payload bytes and backend-owned resource implementation details.

SPERF therefore reports engine-owned upload/capacity bytes but rejects inferring exact driver allocation/residency from those values. Existing SR7 resource reuse remains authoritative; no new SDL abstraction is introduced.

References:

- https://wiki.libsdl.org/SDL3/SDL_GPUBuffer
- https://wiki.libsdl.org/SDL3/SDL_GPUTransferBuffer
- https://wiki.libsdl.org/SDL3/SDL_UploadToGPUBuffer
- https://wiki.libsdl.org/SDL3/SDL_CreateGPUTransferBuffer

## 12. Completion / handoff

SPERF is complete only when one exact PR head proves:

1. `trace2d.sprite-performance.v1` and this document agree,
2. SR8 structural values and supplementary workload structures reproduce exactly,
3. all three SA4 workloads replay successfully,
4. upload payload and retained-capacity meanings remain distinct,
5. memory guidance does not overclaim package/driver/GPU residency,
6. optional timing stays environment-labelled and outside shared-CI thresholds,
7. normal repository audits/tests are green,
8. no production Sprite hot-path code was changed for reporting,
9. `config/trace2d.core-lane.json`, `PROJECT_STATUS.md`, `docs/SPRITES.md`, #172 and this contract agree.

After those gates pass, merge the SPERF PR, close #172 and close #59. Stop. The following `@GitHub Trace2D 다음 진행해줘` continuation begins **#103 Benchmark B1**.
