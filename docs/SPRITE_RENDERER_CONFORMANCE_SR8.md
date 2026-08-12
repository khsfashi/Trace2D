# Sprite Renderer SR8 Conformance Contract

Status: **active via #142**  
Parent: **#59 Complete Sprite program**  
Prerequisite: **SR0-SR7 complete**

SR8 closes the production Sprite renderer before deterministic Sprite animation begins. It adds validation/evidence only. It does not add a second Sprite authority model and it does not move capture, reporting, hashing, filesystem work, GPU readback, or fence waits into ordinary frames.

## 1. Authority boundary

The existing Sprite authority remains unchanged:

```text
canonical SpriteAsset
+ authoritative Sprite runtime / pose history
        -> resolved derived presentation
        -> renderer/backend resources
        -> optional explicit capture evidence
```

A GPU capture can prove presentation behavior. It cannot replace canonical asset data, authoritative pose/history, painter order, primitive structure, visibility facts, or batch derivation.

SR8 therefore uses two complementary evidence tiers:

1. **backend-independent exact conformance** for deterministic semantic/structural facts,
2. **trusted owner presentation-GPU conformance** only for facts that require the real rendering path.

## 2. Fixture matrix

| Renderer stage | Deterministic evidence | Real-GPU evidence |
| --- | --- | --- |
| SR0 asset/render separation | `SpriteRenderContractTests` | covered through all presentation fixtures |
| SR1 transform/history | `SpriteGeometry2DTests`, `SpritePresentation2DTests`, `SpriteRendererConformanceTests` | `SpriteRendererGpuConformanceTests` |
| SR2 trim/pivot/atlas/`cw90` | `SpriteGeometry2DTests`, `SpriteRendererConformanceTests` | `SpriteRendererGpuConformanceTests` |
| SR3 color/alpha/blend/sampling | `SpriteAppearance2DTests` | `SpriteGpuSmokeTests` |
| SR4 painter order/groups/masks | `SpriteOrderMask2DTests` | `SpriteOrderMaskGpuSmokeTests` |
| SR5 sliced/tiled primitives | `SpritePrimitive2DTests` | `SpritePrimitiveGpuSmokeTests` |
| SR6 pixel-perfect presentation | `SpritePixelPerfect2DTests`, `SpritePixelPerfectPrecisionTests` | `SpritePixelPerfectGpuSmokeTests` |
| SR7 culling/batching/reuse | `SpriteBatch2DTests`, SR8 structural workload | `SpriteBatchGpuSmokeTests` |

The SR8-specific real-GPU fixture fills the remaining SR2 presentation gap: a trimmed 2x1 logical region with an exact rational pivot is stored as a 1x2 clockwise-rotated packed rectangle. The CPU contract first asserts logical geometry and UV permutation, then the actual GPU capture must reconstruct red-left / green-right logical orientation.

## 3. Comparison policy

### 3.1 Deterministic CPU facts

Use exact values whenever the public contract is exact:

- integer source/page/trim/packed rectangles,
- reduced rational pivot semantics,
- logical/packed UV corner mapping,
- stable painter/batch structure,
- submitted/visible/culled counts,
- primitive/visible quad counts,
- compatibility-run counts,
- authoritative-current vs interpolated presentation selection.

Floating geometry produced by documented trigonometric/presentation math uses the narrow epsilon already owned by the stage-local contract tests; SR8 does not invent a cross-platform screenshot hash for those values.

### 3.2 GPU pixels

GPU fixtures own an explicit bounded per-channel tolerance when floating color conversion/rasterization can legitimately vary. SR8 does not globally loosen those tolerances.

The SR8 `cw90` fixture uses a per-channel tolerance of **8** on extreme red/green texels. Existing SR3-SR7 fixtures retain their already-reviewed fixture-local tolerances.

Exact byte equality is used only when the relevant contract genuinely guarantees exact bytes. There is no automatic golden-image update command: changing a reference value or tolerance is a reviewed source change.

## 4. Committed structural workload v1

`SpriteRendererConformanceTests.Sr8CommittedStructuralWorkloadHasExactRawMetrics` is the SR8 fixed structural workload.

It uses a caller-owned `std::array<SpriteBatchItem2D, 1024>`; no test-only heap container is added to the production hot path.

Generation rule:

- 1,024 semantic Sprite submissions,
- every fourth item is culled,
- every item whose index is `1 mod 16` expands to four visible quads,
- isolated compatibility changes occur at indices 257, 513, and 769 using texture, sampler, and blend state respectively,
- all other visible work remains compatible and painter order is never resource-sorted.

Frozen raw expectations:

```text
schema             trace2d.sprite-renderer-workload.v1
submitted_sprites  1024
visible_sprites     768
culled_sprites      256
visible_quads       960
contiguous_runs       7
metric_source       deterministic_structure
```

The test emits one `TRACE2D_SR8_WORKLOAD_V1` marker only during explicit test execution. `scripts/sprite_renderer_final_gate.ps1` parses that measured marker and refuses to produce final evidence if any frozen value changes without an explicit contract update.

This workload is structural, not a timing benchmark. Shared CI wall-clock time is intentionally not an SR8 correctness gate. Generic timing/profiler policy remains owned by #91.

## 5. Existing renderer workloads

SR8 also retains the previously committed renderer workload set exposed by `trace2d_renderer_workload --list`:

- `dense_single_texture`,
- `alternating_two_textures`,
- `interleaved_culling`.

The final gate records that tool's deterministic JSON output and SHA256 checksum as supplementary renderer-scale evidence. It does not reinterpret those older workloads as a substitute for the production Sprite SR7 compatibility contract.

## 6. Trusted owner GPU gate

#140/#141 owns the generic real-GPU runner/trust boundary. SR8 does not create another self-hosted-runner policy.

The generic gate automatically selects the SR8 GPU fixture because its suite name is:

```text
SpriteRendererGpuConformanceTests
```

and the test filter remains:

```text
Gpu(Smoke|Conformance)Tests
```

For SR8 completion, the exact implementation head must contain and pass all of these Sprite GPU suites with no skips:

```text
SpriteGpuSmokeTests
SpriteOrderMaskGpuSmokeTests
SpritePrimitiveGpuSmokeTests
SpritePixelPerfectGpuSmokeTests
SpriteBatchGpuSmokeTests
SpriteRendererGpuConformanceTests
```

Fork pull requests remain excluded from the trusted self-hosted runner exactly as documented in `GPU_GATE.md`.

## 7. Capture/readback boundary

Ordinary Sprite presentation remains readback-free and fence-wait-free under the existing SR3-SR7 contracts.

SR8 capture fixtures may explicitly:

- submit an exact simulation-frame id,
- perform GPU readback/fence synchronization required by capture,
- inspect returned RGBA pixels,
- write a temporary BMP,
- delete the temporary BMP after validation.

Those operations are validation work. Their cost must never be hidden inside normal rendering metrics or generalized into a per-frame screenshot/hash path.

## 8. Final evidence gate

On the trusted owner Windows machine, from the repository root:

```powershell
pwsh -File scripts/sprite_renderer_final_gate.ps1
```

The script:

1. delegates real-GPU execution to `scripts/gpu_gate.ps1` with a Sprite-only test regex,
2. requires every stage GPU suite listed above and rejects skips,
3. runs `SpriteRendererConformanceTests` verbosely,
4. parses the measured structural-workload marker,
5. records deterministic `trace2d_renderer_workload --list` output,
6. hashes CPU/GPU/workload evidence,
7. writes `trace2d.sprite-renderer-final-gate.v1` `manifest.json` bound to the exact Git commit.

The final manifest is evidence only. It does not change renderer state, asset state, backend selection, or any performance budget.

## 9. Performance rules

SR8 must preserve all of the following:

- no JSON/string construction in ordinary Sprite frames,
- no filesystem access in ordinary Sprite frames,
- no semantic-name lookup added to the render hot path,
- no new per-frame heap container solely for reporting,
- no ordinary-frame explicit GPU readback/fence wait,
- no resource-based semantic reorder,
- caller-owned/fixed-size CPU conformance data where practical,
- existing SR7 retained GPU resources and capacity behavior remain authoritative.

All new reporting/allocation in this stage is restricted to test scripts, test executables, or explicit evidence commands.

## 10. Completion rule

SR8 is complete only when:

1. hosted CI is green on the final implementation head,
2. the trusted owner GPU gate is green on the same head with no Sprite GPU fixture skipped,
3. the SR8 final evidence manifest records the exact head and frozen structural workload,
4. #142 records the exact evidence,
5. the SR8 PR merges and #142 closes.

After merge, stop. Do not create SA0 in the same continuation. The following `@GitHub Trace2D 다음 진행해줘` continuation creates exactly one SA0 child.