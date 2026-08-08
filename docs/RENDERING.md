# Rendering

Trace2D rendering is presentation and visual-QA state. It must not become authoritative gameplay state.

This document records the P5 renderer contract as it is implemented. `PROJECT_STATUS.md` remains the source of truth for the active task and validation state.

## Current implementation

The first P5 slice established renderer ownership:

- `engine/render` owns the Trace2D rendering API.
- SDL3 GPU types are private to the renderer implementation.
- `engine/platform` continues to own the SDL window lifetime.
- the public platform API exposes a numeric `WindowId`, not `SDL_Window*`.
- the renderer resolves that ID inside its SDL-backed implementation and claims the window for an SDL3 GPU device.
- renderer construction is valid only for a windowed `Platform`.
- headless runtime/tests do not create a GPU device or swapchain.

The second P5 slice established CPU-only render data:

- `OrthographicCamera` stores only camera center and full visible vertical world size.
- `TryBuildOrthographicView` derives target aspect ratio, world-space half extents, and cached world-to-clip scale.
- `WorldToClip` maps world positions into SDL GPU normalized device coordinates with +Y remaining up.
- `SpriteRenderData` carries axis-aligned center/half-extents, renderer texture identity, and deterministic draw-order keys.
- `SpriteDrawOrderLess` orders lower layers first, then `stableOrder`.
- `IsSpriteVisible` provides an inclusive axis-aligned camera-overlap decision.
- the camera/view/sprite structs are trivially copyable and the helper functions allocate no memory.

The textured-sprite slice added the first real GPU draw path:

- `SpriteRenderData::texture` is a Trace2D-owned 32-bit handle; `0` is invalid and no SDL pointer enters render data.
- `Renderer::CreateTextureRgba8` uploads immutable RGBA8 source bytes once and returns a stable handle.
- texture handles are not reused in the P5 baseline, avoiding accidental stale-handle aliasing before a generation-based resource table is justified.
- the renderer owns a persistent six-vertex unit-quad buffer, nearest/clamp sampler, graphics pipeline, and created textures.
- the vertex shader receives a 16-byte clip-space center/half-extent uniform per submitted sprite.
- the fragment shader samples the bound RGBA8 texture with normal alpha blending.
- windowed `trace2d run` creates a 2x2 sample texture and submits one textured quad.

PR #27 established the explicit ordered multi-sprite unbatched baseline:

- `Renderer::RenderFrame(camera, std::span<const SpriteRenderData>)` consumes a non-owning contiguous view rather than copying a frame list.
- the single-sprite overload delegates to the span path.
- one `OrthographicView` is built per non-empty presented frame.
- caller-supplied sprite order is preserved; the renderer does not sort or materialize a second list.
- each visible sprite produces exactly one draw in the current unbatched submission path.

PR #28 integrated the CPU visibility decision into actual submission:

- each sprite receives one inclusive AABB camera-overlap test before any per-sprite GPU work.
- culled sprites skip uniform upload, texture/sampler bind, and draw encoding.
- relative visible order is unchanged.
- the persistent pipeline and vertex buffer are bound lazily on the first visible sprite.
- `RenderMetrics::culledSprites` records successful-frame culling decisions separately from submitted sprites.
- texture handles are validated for the complete supplied span before GPU command encoding so invalid-input behavior does not depend on camera position.

PR #29 added allocation-free measurement for contiguous same-texture batching opportunities. The current executable sample demonstrates no draw-call saving, so actual instancing remains deferred until a representative workload justifies it.

The current P5 capture-foundation slice adds a persistent offscreen color target:

- scene rendering no longer writes directly into the swapchain texture.
- the renderer creates one offscreen color target lazily when a non-zero presentation target first becomes available.
- the offscreen target uses the actual swapchain texture format and dimensions so the presentation path can use an exact texture-to-texture copy without format conversion.
- the target is reused while dimensions remain unchanged and replaced only when the presentation target size changes.
- replacement is create-before-release, so a failed resize allocation does not destroy the existing target.
- the render pass clears/stores into the offscreen target and sets SDL GPU target cycling for normal frame reuse.
- after the render pass, one GPU copy pass copies the completed offscreen image into the acquired swapchain texture.
- the swapchain remains presentation-only. It is not treated as a readback source.
- no capture/download/fence work occurs yet; that remains explicit-request-only work for the next slice.

This offscreen target is deliberately a capture foundation rather than the final artifact contract. The canonical capture pixel format, row-pitch normalization, image encoding, and explicit simulation-frame request are resolved by the readback/capture slice, not by ordinary presentation.

## Ownership

```text
Platform
  owns SDL initialization + SDL_Window
        |
        | SDL-free WindowId
        v
Renderer
  owns SDL_GPUDevice
  owns sprite pipeline / vertex buffer / sampler / textures
  owns stable Trace2D texture-handle table
  owns persistent size-matched offscreen color target
  claims/releases window swapchain
  acquires/submits command buffers
  renders into offscreen presentation state
  copies completed offscreen output into the swapchain
  consumes derived render data
  does not own simulation state
```

Construction order should keep `Platform` alive for the complete `Renderer` lifetime. Normal stack ownership naturally destroys the renderer before the platform when the renderer is created after the platform.

Simulation/scene systems remain responsible for gameplay truth. A later extraction step may copy authoritative state into `SpriteRenderData`, but render data and capture pixels are disposable presentation/QA output.

## Headless invariant

The deterministic gameplay path must remain usable without GPU presentation.

`Renderer` rejects a headless `Platform` before GPU initialization. Existing runtime, scene, input, agent, and gameplay-testing modules do not depend on `Trace2D::Render`.

The camera math, draw-order comparison, visibility decision, batching measurement, and texture-handle fields remain CPU-owned data and can be tested without creating a window or GPU device.

Do not move gameplay truth into renderer-owned resources just to make visual features easier to implement.

## Orthographic camera contract

`OrthographicCamera::verticalSize` is the full visible world-space height. Horizontal size is derived from the render-target aspect ratio.

For a valid camera/target pair:

```text
halfHeight = verticalSize / 2
halfWidth  = halfHeight * targetWidth / targetHeight
clip.x     = (world.x - center.x) / halfWidth
clip.y     = (world.y - center.y) / halfHeight
```

`TryBuildOrthographicView` performs the divisions once and caches their reciprocals in `clipScale`. Per-position `WorldToClip` then requires only subtraction and multiplication.

The engine convention keeps +Y up in world space and normalized device coordinates. Backend-specific coordinate conversion is left to SDL GPU rather than leaking backend flips into Trace2D camera math.

Invalid zero-sized targets, non-finite camera values, or non-positive vertical size are rejected and clear the output view.

## Sprite render-data contract

The current `SpriteRenderData` remains intentionally smaller than a production sprite component:

- `center` — world-space sprite center
- `halfExtents` — axis-aligned world-space half size
- `texture` — renderer-owned `TextureHandle`; `InvalidTextureHandle` (`0`) means no live texture
- `layer` — primary painter-order key; lower values draw first
- `stableOrder` — deterministic tie-break key within a layer

It deliberately does **not** include UV rectangles, tint, material state, or rotation yet. Those should enter only when a measured sample requires them.

Callers that require a total deterministic draw order must provide unique `stableOrder` values for sprites that share a layer. Equal `(layer, stableOrder)` pairs are intentionally equivalent to the comparator; texture identity does not alter painter order.

The renderer does not sort the submitted span. Ordering is a caller/extraction responsibility so the frame hot path does not allocate or reorder presentation data implicitly.

`IsSpriteVisible` performs inclusive axis-aligned overlap against the camera view. A sprite touching the view edge is visible. The same helper is used by real submission and batching measurement.

## Texture lifetime and upload

`CreateTextureRgba8` is an explicit resource-creation path, not part of frame submission.

For each created texture:

1. validate non-zero dimensions and exact `width * height * 4` source-byte count,
2. create one `SDL_GPUTexture` with RGBA8 sampled-texture usage,
3. create/map an upload transfer buffer,
4. encode one GPU copy pass,
5. submit the upload,
6. release the transfer buffer,
7. retain only the GPU texture behind a stable Trace2D handle.

`DestroyTexture` releases the GPU texture and leaves a tombstone in the handle table. Handles are intentionally not recycled in the P5 baseline. Renderer destruction releases every remaining live texture.

The handle lookup is direct indexed access. No string lookup or hash lookup is needed in the frame path.

Before command-buffer acquisition, every supplied sprite texture handle is validated against the live texture table. Visible sprites then perform the direct lookup again when their actual texture binding is encoded. This duplicate O(1) lookup is intentionally preferred over building a transient resolved-resource array; remove it only if profiling shows it matters.

## Offscreen target lifetime and presentation

The offscreen color target is renderer-owned presentation state.

Its format is queried once from the claimed window swapchain and used both by the sprite graphics pipeline and the size-dependent offscreen texture. Its dimensions are taken from the actual acquired swapchain texture rather than guessed from logical window size.

Normal frames do not create or release an offscreen texture. `EnsureOffscreenColorTarget` returns immediately when the existing target dimensions match. Resource churn occurs only on first usable presentation or a real target-size change.

When a resize requires replacement, the new target is created before the old one is released. SDL releases GPU textures only when safe, so a prior submitted frame may continue using the old backing resource without an application-side stall.

The render pass uses `SDL_GPU_LOADOP_CLEAR`, `SDL_GPU_STOREOP_STORE`, and target cycling. After the pass ends, a GPU copy pass copies the full target into the same-format, same-size swapchain texture. This preserves the current one-render-pass sprite contract while establishing a readable non-swapchain source for the later capture path.

A successfully acquired swapchain texture is not cancelled on later command-encoding failure. SDL documents cancellation after swapchain acquisition as invalid, so those exceptional paths submit the already-acquired command buffer before propagating the error.

## Shader packaging baseline

SDL GPU requires backend-compatible shader formats. Trace2D uses the repository-pinned `sdl3-shadercross` package for the first portable shader baseline.

At `Renderer` construction:

1. initialize SDL_shadercross,
2. compile the small embedded HLSL vertex/fragment sources to SPIR-V,
3. reflect their resource counts,
4. let SDL_shadercross create backend-compatible `SDL_GPUShader` objects for the selected GPU device,
5. create the persistent graphics pipeline,
6. release the temporary shader objects and immediately shut down SDL_shadercross state.

Shader compilation therefore has a one-time renderer-startup cost and is never performed in `RenderFrame`. This is intentionally simpler than committing backend-specific generated blobs while P5 is still proving the contract. Once packaging targets and measured startup requirements are established, offline shader compilation can replace this construction-time step without changing simulation or render-data APIs.

The HLSL bindings follow SDL GPU's documented resource-set convention: vertex uniforms use space/set 1 and fragment sampled texture/sampler resources use space/set 2.

## Frame path

The current ordered multi-sprite windowed frame is:

```text
validate every supplied texture handle
  -> acquire GPU command buffer
  -> wait/acquire swapchain texture and actual target size
  -> if presentation target is available:
       -> build one OrthographicView from actual target size
       -> reuse offscreen target, or replace it only if size changed
       -> begin render pass on offscreen target with clear load op
       -> for each caller-ordered sprite
            -> inclusive AABB visibility test
            -> if culled: increment local cull count and continue
            -> on first visible sprite only: bind persistent pipeline + unit-quad vertex buffer
            -> compute clip-space center / half extents
            -> push one 16-byte vertex uniform
            -> bind sprite texture + persistent sampler
            -> draw six vertices / one instance
       -> end render pass
       -> begin GPU copy pass
       -> copy completed offscreen target into swapchain texture
       -> end copy pass
  -> submit command buffer
  -> commit frame/draw/submitted/cull metrics after successful submission
```

A minimized/unavailable swapchain texture is treated as a valid no-presentation frame. It creates no offscreen target work, sprite draw, or culling decision because no target-dependent `OrthographicView` exists for that frame.

The original clear-only `RenderFrame()` remains available. The single-sprite overload delegates to the same span-based submission path as multi-sprite input.

## Metrics

`RenderMetrics` records cumulative:

- submitted frame count
- presented frame count
- encoded render-pass count
- draw-call count
- submitted-sprite count
- culled-sprite count
- last render-target width/height

For the current unbatched path on a successfully presented frame:

```text
visible sprites = submittedSprites delta = drawCalls delta
culled sprites  = culledSprites delta
supplied sprites = visible sprites + culled sprites
```

The offscreen-to-swapchain texture copy is presentation transfer work, not a sprite draw call, and does not change `drawCalls` or `submittedSprites` semantics.

Once batching is introduced, `submittedSprites` should continue to describe encoded sprites while `drawCalls` may become smaller.

`trace2d run --windowed --json` exposes `draw_calls`, `submitted_sprites`, and `culled_sprites` so the baseline remains script-readable.

Metrics are renderer-owned observation state and are not authoritative gameplay state.

## Allocation / lifetime policy

Persistent renderer resources have renderer lifetime or a size-dependent presentation lifetime:

- GPU device
- claimed swapchain relationship
- unit-quad vertex buffer
- sprite graphics pipeline
- sampler
- created sprite textures
- offscreen color target, reused until presentation size changes

Texture-table growth may allocate during explicit texture creation. The normal steady-size span-based frame path performs no sprite-list allocation/copy, container growth, shader compilation, texture upload, offscreen texture creation, renderer-side sorting, or batching-container construction.

Camera/view/sprite data is trivially copyable. Camera-view construction performs no dynamic allocation, `WorldToClip` performs no allocation or division, and draw-order/visibility/batching-measurement helpers allocate no memory.

Culling is an O(N) direct pass fused into submission. It does not build a visible-sprite list. Fully culled input skips all sprite draw work and also skips pipeline/vertex-buffer binding.

Failure diagnostics may allocate strings. Driver-name storage is initialized once during renderer construction.

Before adding resource pools, bindless tables, custom allocators, atlases, persistent staging systems, or a spatial index, establish the workload and record measurements that justify the complexity.

## Remaining P5 work

Actual GPU sprite instancing remains deferred until a representative Public Alpha sample demonstrates a material saving with the existing contiguous-texture measurement.

The next capture slices are:

1. define an explicit capture request carrying the requested simulation frame and artifact destination/format contract,
2. add a reusable SDL GPU download transfer buffer sized for the current capture target,
3. on requested frames only, encode `SDL_DownloadFromGPUTexture` from the completed offscreen target,
4. submit the capture command buffer with a fence and consume bytes only after the fence is signaled,
5. normalize row pitch / pixel layout into the canonical artifact contract and write a deterministic image,
6. add deterministic artifact validation without making pixels authoritative gameplay state,
7. close Issue #10 when the explicit-frame capture acceptance criterion is met.

Wall-clock timing must never decide which gameplay frame is captured. Normal non-capture frames must not pay download, fence-wait, mapping, encoding, or file-I/O costs.

## Non-goals for P5

Do not add these to finish the renderer/capture release gate:

- lighting or PBR
- render graph framework
- editor renderer
- full material system
- broad asset-import pipeline
- custom allocator framework
- physics
- GPU-driven scene architecture
- async/video capture framework

The Public Alpha renderer only needs to prove the complete agent-first loop with a small sprite scene and deterministic visual capture.
