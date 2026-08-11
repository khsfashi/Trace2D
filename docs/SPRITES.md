# Sprite Pipeline Contract

Status: **S0 architecture frozen in #119; implementation starts at S1 after S0 merges**

Operational umbrella: GitHub Issue #59.  
S0 implementation contract: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).

This document owns the complete fixed Sprite stage order and capability target. `SPRITE_ARCHITECTURE.md` owns the exact S0 authority/coordinate/presentation seams. Later Sprite children may refine their own stage-local details but must not silently violate S0.

## 1. Product goal

Trace2D targets an Agent-verifiable Sprite pipeline rather than a minimal quad renderer:

```text
sprite request / source image / external sheet
        -> optional provider generation
        -> raw source pixels
        -> deterministic import / cleanup / normalization
        -> machine-readable Sprite QA
        -> canonical Trace2D SpriteAsset
        -> deterministic SpriteAnimator2D
        -> headless inspect / query / step / assert
        -> production Sprite renderer
        -> capture / motion QA / visual QA
        -> reproducible performance evidence
```

Generated pixels and external tool formats are inputs. They become runtime-usable only after canonical import/validation.

## 2. Frozen S0 architecture

S0 is complete only as an architecture contract; it does not implement broad renderer functionality.

The frozen authority direction is:

```text
external source/generation
        -> deterministic import
        -> canonical SpriteAsset CPU truth
        -> authoritative SpriteRenderer2D / SpriteAnimator2D semantics
        -> backend-independent render extraction
        -> derived presentation data
        -> renderer/backend resources
```

Hard invariants:

1. canonical Sprite data and authoritative animation/gameplay state do not depend on renderer/GPU initialization;
2. exact source-space pixel metadata is canonical; normalized UVs, GPU handles, upload offsets and batch IDs are derived;
3. future `SpriteRenderer2D` and `SpriteAnimator2D` are typed component semantics compatible with #71, never a Sprite-only entity graph;
4. fixed simulation keeps `previous_fixed` and authoritative `current_fixed`; interactive interpolation is presentation only;
5. reset/load/teleport/snap synchronize transform history to avoid smear;
6. exact-frame capture renders authoritative current state unless a sub-frame alpha is explicitly requested and recorded;
7. future #86 resources, #88 camera/viewport and #89 material/shader systems extend fixed seams without changing Sprite authored truth;
8. semantic painter order is preserved; only compatible contiguous work may batch;
9. import/generation/repair/full inspection/capture/reporting are explicit tooling work, not normal per-frame work;
10. deterministic/structured facts are verified before multimodal review; human approval remains the final authority for taste.

The exact coordinate, pivot, trim, packed-rotation, presentation-history, texture, authority, diagnostics, and future-handoff rules are frozen in `SPRITE_ARCHITECTURE.md` and machine-checked through `contracts/sprite-s0.json`.

## 3. Canonical coordinate and asset requirements

S1 implements a strict versioned text/diff-friendly canonical asset representation under these S0 rules:

```text
source-space origin = top-left
+x                  = right
+y                  = down
pixel rectangles    = integer half-open [x, x+w) x [y, y+h)
pivot               = untrimmed source space
trim                 = storage optimization only
packed rotation      = storage orientation only
```

A canonical region must represent at least:

- stable semantic region ID/name,
- project-relative source texture or atlas-page identity,
- packed integer pixel rectangle,
- original untrimmed source size,
- trim size and trim offset in original source space,
- explicit pivot,
- supported packed rotation,
- default sampling/pixel-art and production texture intent where applicable.

A canonical animation frame references a canonical region and an exact deterministic duration representation. Clip data supports stable ID/name, deterministic ordered frames, loop semantics and ordered semantic events.

Trimming and atlas rotation must never silently change visible logical placement, pivot, animation alignment, or gameplay semantics.

## 4. Determinism boundary

Allowed nondeterminism:

- image-generation providers,
- human curation,
- optional human art edits.

For identical committed inputs/configuration, deterministic contracts are required for:

- canonical import metadata,
- trim/region conversion,
- Trace2D-owned deterministic atlas packing,
- deterministic repair algorithms where claimed,
- QA measurements,
- animation time/frame/event progression,
- Agent semantic inspection/assertion,
- backend-independent Sprite geometry/UV/order extraction,
- ordered contiguous batch-run derivation,
- headless fixtures/replay within the documented domain.

GPU pixels are checked under documented presentation tolerances/contracts; do not claim universal cross-vendor bit identity without proof.

## 5. Fixed implementation order inside #59

Exactly one child issue/PR is active at a time:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Current stage: **S0 / #119**.  
Exact next stage after S0 merges: **S1**.

## 6. Foundation

### S0 — Sprite architecture and contract

Frozen by `SPRITE_ARCHITECTURE.md`:

- authority/ownership matrix,
- exact source-space coordinate conventions,
- pivot/trim/rotated-storage semantics,
- fixed-step authoritative vs interactive presentation state,
- exact-frame capture mode,
- resource/view/material compatibility seams,
- painter-order batching invariant,
- production texture ownership/handoffs,
- #97-#99 deterministic -> multimodal -> human review authority,
- offline tooling/hot-path boundary,
- versioning/diagnostic/importer boundaries,
- explicit non-goals/future issue handoffs.

S0 must not opportunistically implement renderer breadth.

### S1 — Canonical Sprite asset model and deterministic import representation

Acceptance:

- strict explicit schema version,
- stable project-relative typed asset identity,
- immutable/explicitly owned canonical Sprite metadata,
- exact validation for sizes/rectangles/trim/pivots/page/frame/clip references/durations,
- deterministic ordering and canonical serialization where published,
- stable machine-readable diagnostics,
- no SDL/GPU handles in the asset model,
- malformed/boundary/round-trip/import fixtures.

## 7. Production-complete Sprite Renderer

The renderer target includes standalone textures, atlas regions, trim/source-size/pivot preservation, complete 2D transform semantics, semantic flip, tint/opacity, sampling, alpha/blend, deterministic painter order/sorting groups/masking, 9-slice, tiled presentation, pixel-perfect runtime presentation, fixed-step interpolation, order-preserving batching, persistent resource reuse, capture/conformance and workloads.

### SR0 — Renderer contract and asset/render separation

Freeze/implement the derived render-data vocabulary:

- authored pixel data separate from normalized UV/GPU data,
- semantic vs derived transform state,
- built-in material/pipeline compatibility identity,
- sampler/blend/mask/primitive ownership,
- backend-independent extraction and headless tests,
- renderer output remains presentation evidence, never authoritative state.

### SR1 — Transform/geometry and presentation history

Implement/test:

- position, rotation, independent X/Y scale,
- pivot and explicit `flip_x`/`flip_y`,
- trim/source-space reconstruction,
- rotation + non-uniform scale + pivot combinations,
- trim + pivot + flip combinations,
- defined negative-scale behavior,
- `previous_fixed` / `current_fixed` history,
- shortest-arc 2D interpolation,
- discontinuity synchronization,
- authoritative-current exact-frame selection,
- future hierarchy seam: interpolate local then compose.

Geometry/presentation math is backend-independent and GPU-free testable.

### SR2 — Atlas/trim/pivot/rotated packing

Implement/test arbitrary sub-rect derivation, exact trim reconstruction, original source-size placement, pivot preservation, atlas page selection, supported 90-degree packed storage, bounds validation, and equivalent geometry for logically identical rotated/unrotated or trimmed/untrimmed sources.

### SR3 — Color/alpha/blend/sampling

- explicit tint/opacity semantics,
- one documented canonical alpha/conversion boundary,
- conventional normal/additive/multiply/screen modes only when backend conformance proves them,
- nearest/linear sampling through renderer-owned cached sampler states,
- explicit color-space intent and conversion behavior.

Do not create per-sprite sampler objects or guess source alpha/color space at render time.

### SR4 — Painter order/sorting groups/masking

- explicit layer plus deterministic stable order,
- precisely defined sorting groups for multi-Sprite objects,
- bounded conventional Sprite mask/clip semantics,
- deterministic order/group/mask fixtures,
- no global texture/material sort that changes visual order.

### SR5 — 9-slice and tiled/repeated primitives

- explicit left/right/top/bottom 9-slice borders,
- well-defined stretch/repeat behavior,
- tiled Sprite geometry that cannot bleed into adjacent atlas regions,
- same canonical asset/resource/render path rather than a second unrelated quad system.

### SR6 — Pixel-perfect runtime presentation

Freeze/test mapping among source pixels, world scale/pixels-per-unit, resolved camera/view, logical viewport, target resolution and final scaling.

Document guarantees/non-guarantees for integer upscale, non-divisible windows, letterbox/pillarbox, movement, camera movement, rotation and non-integer scale. Pixel-perfect presentation is not an unexplained `round(position)` heuristic and is separate from offline pixel-art repair.

### SR7 — Production batching/resource reuse

Preserve:

- semantic order,
- compatible contiguous-run batching,
- persistent/capacity-reused GPU/upload resources,
- measured geometric growth,
- no ordinary steady-state frame-list heap churn,
- resolved compatibility identity that may include texture/resources, material/pipeline, sampler, blend, mask and primitive mode.

Measure CPU extraction, upload bytes, instances, draws, GPU time where available, retained capacities and memory before adding packed/half/bindless/GPU-driven complexity.

### SR8 — Renderer conformance/workloads

Committed fixtures cover pivot, trim, source reconstruction, flip, transforms, rotated atlas storage, tint/opacity, shipped blend/sampling modes, order/groups, masking, 9-slice, tiled Sprite, pixel-perfect presentation and contiguous batch derivation.

Prefer structural headless assertions; use pixel capture only where pixels are the actual fact. Publish raw workload metrics.

## 8. Deterministic Sprite Animation

Animation is authoritative runtime state independent of renderer initialization.

### SA0 — Timing/frame/event contract

Freeze exact time representation, fixed-step advancement, frame-boundary behavior, loop/reset/seek rules, invalid-duration handling, speed semantics and deterministic same-boundary event ordering. Do not use wall-clock render playback as authoritative time.

### SA1 — `SpriteAnimator2D` state

Renderer-independent typed state includes animation set, active clip, canonical time/ticks, frame index, playing state, loop/completion state and speed. Headless and windowed execution consume the same authoritative state.

### SA2 — Playback/events/transitions

Explicit play/restart/pause/resume/stop/reset, loop/non-loop completion, deterministic speed change, emitted semantic events and clip transitions. No generic animation graph is required unless later evidence justifies one.

### SA3 — Agent/MCP verification

Expose protocol-independent inspect/action/assert semantics for clip, frame, time, playing/loop/speed, selected region and recent/emitted events. MCP remains an adapter; snapshots allocate only on explicit requests.

### SA4 — Conformance/workloads

Prove exact frame/event sequences through fixed steps, reset/replay, loops, completion, speed changes and clip transitions. Measure authoritative animation update separately from render extraction/GPU submission.

## 9. Offline Sprite Processing and QA

Rich processing is allowed because it runs on explicit import/authoring/QA commands.

### SPP0 — Processing/QA report

Stable machine-readable raw measurements may include dimensions, expected/actual frame count, alpha residue/edge bleed, trim bounds, pivot/centroid/jitter, pixel-grid confidence, palette size/violations, identity/motion heuristic signals, empty-frame warnings and atlas utilization. Heuristic scores retain underlying raw measurements and are labeled heuristic.

### SPP1 — Alpha/background/frame extraction

Explicit deterministic modes may cover matte cleanup, alpha normalization, connected components, projection separation and justified optimal cuts. Expected-frame mismatch fails explicitly rather than manufacturing plausible frames. Source files remain preserved.

### SPP2 — Pixel-grid/palette/pivot/identity/motion QA and repair

- grid detection reports confidence/failure and tests idempotence when claimed,
- palette analysis/quantization is explicit offline work,
- pivot/baseline suggestions remain reviewable source-space data,
- identity/motion image metrics are QA heuristics, not gameplay truth.

### SPP3 — Aseprite/generic importers

Support documented subsets of loose frames, regular sheets, explicit region manifests and Aseprite-exported sheet/JSON. Convert all to canonical Sprite assets; runtime never branches on source tool.

### SPP4 — sprite-gen / PerfectPixel-style interoperability

Consume useful explicit manifests through conversion/validation boundaries without runtime dependencies on those applications.

### SPP5 — Provider-neutral generation orchestration

```text
sprite request
 -> GenerationProvider / external command
 -> raw output
 -> deterministic Trace2D processing + QA
 -> canonical asset only after validation
```

Credentials are never project data; no core vendor dependency; live generation is not deterministic CI; stable recorded/synthetic fixtures own processing regression tests.

## 10. End-to-end proof

### SE2E

Prove request/import -> raw/generated pixels -> deterministic cleanup/QA -> canonical Sprite asset -> deterministic animation -> headless exact-frame state/events -> renderer -> capture -> motion/visual review. The committed proof uses stable fixtures and does not require network/provider credentials.

### SPERF

Publish reproducible workloads for visible/animated counts, atlas pages, batch-key transitions, draws, culling, animation CPU time, extraction/packing time, upload bytes, retained capacities, texture/page memory/utilization and capture cost. Timings remain environment-labelled evidence, never invented portable percentages.

## 11. Agent observability target

Semantic inspection should eventually expose authoritative facts similar to:

```text
#player SpriteRenderer2D
  asset    = "hero"
  region   = "attack_03"
  pivot    = [16, 28]
  flip_x   = false
  flip_y   = false
  tint     = [1, 1, 1, 1]
  blend    = "normal"
  sampling = "nearest"
  layer    = 10
  order    = 125
  visible  = true

#player SpriteAnimator2D
  clip     = "attack"
  frame    = 3
  playing  = true
  loop     = false
  time     = ...
```

Explicit renderer-debug requests may additionally derive atlas page/rect, normalized UV, presentation/world bounds, visibility, compatibility/batch run and draw index. These derived facts are not built every frame for Agent convenience.

## 12. Verification/review boundary

Reuse #97-#99:

```text
deterministic Sprite fact
 -> verify
 -> diagnose
 -> Agent/user repair
 -> re-verify
 -> multimodal review only if genuinely perceptual
 -> human creative approval
```

Deterministic failure cannot be overridden by a screenshot or subjective grader. Sprite does not create a second review database.

## 13. Explicit non-goals / handoffs

Not part of #59 unless separately promoted:

- generic material/shader graph or programmable user shaders (#89 owns programmable Material2D/Shader2D),
- PBR/deferred/general lighting/render graph,
- bindless/GPU-driven scene architecture,
- arbitrary textured/deformable polygon geometry (#60 Mesh2D),
- skeletal runtime/Spine integration before #61/#101 license/product decision,
- generic ECS/reflection/property bag,
- custom allocator/job system,
- general camera/resource/world systems (#88/#86/#71 own them; #59 only preserves their seams).

## 14. Handoff rule

Every Sprite child PR must:

1. implement only the first incomplete stage,
2. include relevant tests/fixtures/evidence,
3. update this roadmap and the frozen architecture only when a stage genuinely finalizes/supersedes a contract,
4. update `PROJECT_STATUS.md`,
5. leave enough structured evidence for continuation without chat history,
6. avoid beginning the next child until the current PR merges green.

After the complete #59 program, the fixed core order advances to #103 Benchmark B1 before game-production issue #69.
