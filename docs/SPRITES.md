# Sprite Pipeline Contract

Status: **S0 complete via #119/#120; S1 canonical asset/import active via #121/#122**

Operational umbrella: GitHub Issue #59.  
Frozen S0 architecture: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).  
Concrete S1 format: [`SPRITE_ASSET_FORMAT.md`](SPRITE_ASSET_FORMAT.md).

This document owns the complete fixed Sprite stage order and capability target. Later Sprite children may refine their own stage-local contracts but must not silently violate S0 or replace canonical authored truth with renderer/tool state.

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

## 2. Frozen authority direction

S0 completed the architecture freeze in #119/#120:

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
5. reset/load/teleport/snap synchronize transform history;
6. exact-frame capture renders authoritative current state unless explicit sub-frame alpha is requested and recorded;
7. future #86 resources, #88 camera/viewport and #89 material/shader systems extend fixed seams without changing Sprite authored truth;
8. semantic painter order is preserved; only compatible contiguous work may batch;
9. import/generation/repair/full inspection/capture/reporting are explicit tooling work, not normal per-frame work;
10. deterministic/structured facts are verified before multimodal review; human approval remains final authority for taste.

## 3. Canonical source-space contract

All Sprite stages use:

```text
source-space origin = top-left
+x                  = right
+y                  = down
pixel rectangles    = integer half-open [x, x+w) x [y, y+h)
pivot               = untrimmed source space
trim                 = storage optimization only
packed rotation      = storage orientation only
```

Trimming and atlas rotation must never silently change logical placement, pivot, animation alignment or gameplay semantics.

## 4. S1 canonical `SpriteAsset`

S1 implements the first concrete authored/imported CPU representation as versioned `.sprite.toml`.

Header:

```toml
schema = "trace2d.sprite"
version = 1
sampling = "nearest"
```

Page example:

```toml
[[pages]]
id = "main"
texture = "textures/player.png"
size = [256, 128]
color_space = "srgb"
alpha_mode = "straight"
```

Region example:

```toml
[[regions]]
id = "idle_0"
page = "main"
source_size = [32, 32]
trim_offset = [2, 1]
trim_size = [28, 30]
packed_rect = [0, 0, 28, 30]
pivot = [16, 28, 1]
packed_rotation = "none"
```

### Exact pivot

Pivot is an exact reduced rational:

```text
[x_numerator, y_numerator, denominator]
denominator > 0
```

Equivalent ratios reduce to one canonical representation. A representable pivot may intentionally lie outside source bounds and is not clamped.

### Packed rotation

Schema v1 supports exactly:

- `none`,
- `cw90` — packed pixels contain the trimmed logical content rotated 90 degrees clockwise.

For `none`, packed extent equals trim extent. For `cw90`, packed width/height equal trim height/width. Storage rotation never changes runtime Sprite rotation, logical pivot or trim/source coordinates.

### Texture intent

Schema v1 supports:

- `color_space = "srgb" | "linear"`,
- `alpha_mode = "straight"`,
- `sampling = "nearest" | "linear"`.

Canonical page dimensions are validated against the existing decoded CPU `TextureAssetData`. GPU/package format, mip/compression policy, SDL handles and renderer residency are not authored Sprite truth.

### Deterministic import/serialization

S1 requires:

- normalized project-relative Sprite and texture references,
- strict unknown/missing-field diagnostics,
- duplicate page/region rejection,
- exact trim/source/page bounds validation,
- strict enum/version handling,
- deterministic page/region ordering,
- stable canonical serialization,
- parse/save/parse identity,
- immutable successful cache reuse,
- no renderer/GPU initialization for parse/validation/tests.

`SPRITE_ASSET_FORMAT.md` is the concrete schema reference.

## 5. Determinism boundary

Allowed nondeterminism:

- image-generation providers,
- human curation,
- optional human art edits.

For identical committed inputs/configuration, deterministic contracts are required for:

- canonical import metadata,
- trim/region conversion,
- Trace2D-owned deterministic atlas packing when later implemented,
- deterministic repair algorithms where claimed,
- QA measurements,
- animation time/frame/event progression,
- Agent semantic inspection/assertion,
- backend-independent Sprite geometry/UV/order extraction,
- ordered contiguous batch-run derivation,
- headless fixtures/replay within the documented domain.

GPU pixels follow documented presentation tolerances/contracts; do not claim universal cross-vendor bit identity without proof.

## 6. Fixed implementation order inside #59

Exactly one child issue/PR is active at a time:

```text
S0 [complete] -> S1 [active]
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Current stage: **S1 / #121 / PR #122**.  
Exact next stage after S1 merges green: **SR0**.

Do not begin SR0 while #121/#122 is open.

## 7. Foundation

### S0 — Sprite architecture and contract — complete

Frozen by `SPRITE_ARCHITECTURE.md`:

- authority/ownership matrix,
- source-space coordinates,
- pivot/trim/rotated-storage semantics,
- fixed-step authoritative vs interactive presentation state,
- exact-frame capture mode,
- resource/view/material compatibility seams,
- painter-order batching invariant,
- production texture ownership/handoffs,
- #97-#99 deterministic -> multimodal -> human authority,
- offline tooling/hot-path boundary,
- versioning/diagnostic/importer boundaries,
- explicit future issue handoffs.

### S1 — Canonical Sprite asset/import — active

Acceptance:

- strict explicit schema version,
- stable normalized project-relative asset identity,
- immutable canonical CPU metadata,
- ordered pages/regions with stable IDs,
- exact source/trim/packed geometry,
- exact rational pivot,
- `none`/`cw90` storage semantics,
- explicit color-space/alpha/sampling intent,
- decoded page-size verification through CPU texture import,
- deterministic canonical TOML serialization,
- stable structured diagnostics,
- immutable cache reuse/invalidation,
- no SDL/GPU handles or renderer dependency,
- malformed/boundary/round-trip/cache fixtures.

After merge, advance exactly to SR0.

## 8. Production-complete Sprite Renderer

The renderer target includes standalone textures, atlas regions, trim/source-size/pivot preservation, complete 2D transform semantics, semantic flip, tint/opacity, sampling, alpha/blend, deterministic painter order/sorting groups/masking, 9-slice, tiled presentation, pixel-perfect runtime presentation, fixed-step interpolation, order-preserving batching, persistent resource reuse, capture/conformance and workloads.

### SR0 — Renderer contract and asset/render separation

Freeze/implement derived render-data vocabulary:

- authored pixel metadata separate from normalized UV/GPU data,
- semantic vs derived transform state,
- built-in material/pipeline compatibility identity,
- sampler/blend/mask/primitive ownership,
- backend-independent extraction and headless tests,
- renderer output remains presentation evidence, never authoritative state.

### SR1 — Transform/geometry and presentation history

Implement/test position, rotation, independent X/Y scale, pivot, flip X/Y, trim/source reconstruction, negative-scale behavior, `previous_fixed`/`current_fixed`, shortest-arc rotation interpolation, discontinuity synchronization, authoritative-current exact-frame selection and future local-before-world hierarchy composition.

### SR2 — Atlas/trim/pivot/rotated packing

Implement exact sub-rect derivation, trim reconstruction, source-size placement, pivot preservation, atlas page selection, 90-degree packed storage, bounds validation and equivalent logical geometry for rotated/unrotated or trimmed/untrimmed sources.

### SR3 — Color/alpha/blend/sampling

Implement tint/opacity, one canonical alpha/conversion boundary, supported normal/additive/multiply/screen blend modes only when backend conformance proves them, cached nearest/linear samplers and explicit color-space conversion behavior.

### SR4 — Painter order/sorting groups/masking

Implement explicit layer/stable order, sorting groups, bounded Sprite mask/clip semantics and deterministic order/group/mask fixtures. Global texture/material sorting that changes semantic order is forbidden.

### SR5 — 9-slice and tiled/repeated primitives

Implement explicit borders, stretch/repeat rules and atlas-safe tiled geometry through the same canonical asset/resource/render path.

### SR6 — Pixel-perfect runtime presentation

Freeze mapping among source pixels, world scale/pixels-per-unit, resolved camera/view, logical viewport, target resolution and final scaling. Document integer/non-integer window, movement, camera, rotation and scale guarantees rather than using an unexplained rounding heuristic.

### SR7 — Production batching/resource reuse

Preserve semantic order and compatible contiguous-run batching. Reuse persistent/capacity-managed GPU/upload resources. No ordinary steady-state frame-list heap churn. Measure extraction, upload bytes, instances, draws, retained capacity and memory before adding more complex GPU-driven designs.

### SR8 — Renderer conformance/workloads

Commit structural/GPU fixtures covering transforms, trim/pivot, rotated atlas storage, tint/opacity, blend/sampling, order/groups, masking, 9-slice, tiled Sprite, pixel-perfect presentation and batch derivation. Publish raw workload metrics.

## 9. Deterministic Sprite Animation

Animation is authoritative runtime state independent of renderer initialization.

### SA0 — Timing/frame/event contract

Freeze exact time representation, fixed-step advancement, frame-boundary behavior, loop/reset/seek, duration validity, speed semantics and deterministic same-boundary event ordering.

### SA1 — `SpriteAnimator2D` authoritative state

Implement typed animation set/clip/time/frame/playing/loop/completion/speed state usable headlessly and windowed through the same authority.

### SA2 — Playback/events/transitions

Implement play/restart/pause/resume/stop/reset, loop/non-loop completion, deterministic speed changes, ordered semantic events and bounded clip transitions. No generic animation graph without evidence.

### SA3 — Agent/MCP verification

Expose protocol-independent inspect/action/assert semantics for clip, frame, time, playing/loop/speed, selected region and events. MCP remains an adapter; snapshots are explicit-request work.

### SA4 — Conformance/workloads

Prove frame/event sequences through fixed steps, reset/replay, loops, completion, speed changes and transitions. Measure animation update separately from render extraction/GPU submission.

## 10. Offline Sprite Processing and QA

Rich processing is allowed because it runs on explicit import/authoring/QA commands.

### SPP0 — Processing/QA report

Publish stable machine-readable raw measurements for dimensions, expected/actual frame count, alpha/edge residue, trim bounds, pivot/centroid/jitter, grid confidence, palette properties, identity/motion signals, empty-frame warnings and atlas utilization. Label heuristic scores as heuristic.

### SPP1 — Alpha/background/frame extraction

Provide deterministic explicit modes for matte cleanup, alpha normalization and justified frame extraction. Expected-frame mismatch fails explicitly rather than manufacturing plausible frames.

### SPP2 — Pixel-grid/palette/pivot/identity/motion QA and repair

Grid/palette/pivot/identity/motion analysis remains offline deterministic or explicitly heuristic work with reviewable raw measurements and idempotence tests where claimed.

### SPP3 — Aseprite/generic importers

Support documented loose-frame, regular-sheet, manifest and Aseprite-export subsets. Every importer converts into canonical S1/Sx Sprite assets; runtime does not branch on source tool.

### SPP4 — sprite-gen / PerfectPixel-style interoperability

Consume useful external manifests through conversion/validation without runtime dependencies on those tools.

### SPP5 — Provider-neutral generation orchestration

```text
sprite request
 -> GenerationProvider / external command
 -> raw output
 -> deterministic Trace2D processing + QA
 -> canonical asset only after validation
```

Credentials are not project data; live provider calls are not deterministic CI; recorded/synthetic fixtures own processing regression tests.

## 11. End-to-end proof

### SE2E

Prove request/import -> raw/generated pixels -> deterministic cleanup/QA -> canonical Sprite asset -> deterministic animation -> headless exact-frame state/events -> renderer -> capture -> motion/visual review using committed stable fixtures.

### SPERF

Publish reproducible workloads for visible/animated counts, atlas pages, compatibility transitions, draws, culling, animation CPU time, extraction/packing time, upload bytes, retained capacities, texture/page memory/utilization and capture cost. Timings remain environment-labelled evidence.

## 12. Agent observability target

Semantic inspection eventually exposes authoritative facts such as asset/region identity, pivot, flip, tint, blend, sampling, layer/order, visibility and authoritative animation clip/frame/time/events.

Explicit renderer-debug requests may additionally derive atlas rect, normalized UV, presentation bounds, visibility, compatibility/batch run and draw index. These derived facts are not built every frame for Agent convenience.

## 13. Verification/review boundary

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

## 14. Explicit non-goals / handoffs

Not part of #59 unless separately promoted:

- generic material/shader graph or programmable user shaders (#89),
- PBR/deferred/general lighting/render graph,
- bindless/GPU-driven scene architecture,
- arbitrary textured/deformable polygon geometry (#60 Mesh2D),
- skeletal runtime/Spine before #61/#101 license/product decision,
- generic ECS/reflection/property bag,
- custom allocator/job system,
- general camera/resource/world systems (#88/#86/#71 own them; #59 only preserves seams).

## 15. Handoff rule

Every Sprite child PR must:

1. implement only the first incomplete stage,
2. include relevant tests/fixtures/evidence,
3. update this roadmap and stage-specific contracts when a stage genuinely finalizes a decision,
4. update `PROJECT_STATUS.md`,
5. leave enough structured evidence for continuation without chat history,
6. avoid beginning the next child until the current PR merges green.

After the complete #59 program, the fixed core order advances to #103 Benchmark B1 before game-production issue #69.
