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

PR #27 then established the explicit ordered multi-sprite unbatched baseline:

- `Renderer::RenderFrame(camera, std::span<const SpriteRenderData>)` consumes a non-owning contiguous view rather than copying a frame list.
- the single-sprite overload delegates to the span path.
- one `OrthographicView` is built per non-empty presented frame.
- the persistent graphics pipeline and unit-quad vertex buffer are bound once before sprite draws.
- caller-supplied sprite order is preserved; the renderer does not sort or materialize a second list.
- every supplied sprite produces exactly one draw in the unculled baseline, so `drawCalls == submittedSprites == N`.

PR #28 integrates the existing CPU visibility decision into actual submission:

- each sprite receives one inclusive AABB camera-overlap test before any per-sprite GPU work.
- culled sprites skip uniform upload, texture/sampler bind, and draw encoding.
- the relative order of visible sprites is unchanged.
- the persistent pipeline and vertex buffer are bound lazily on the first visible sprite, so a fully culled frame performs no sprite pipeline/buffer bind.
- `RenderMetrics::culledSprites` records successful-frame culling decisions separately from actual submitted sprites.
- texture handles are still validated for the complete supplied span before GPU command encoding so invalid-input behavior does not depend on camera position.

The current renderer is intentionally still unbatched. The measured pre-batching contract is now explicit enough to decide whether the next batching implementation is justified by real sample workloads.

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
  claims/releases window swapchain
  acquires/submits command buffers
  consumes derived render data
  does not own simulation state
```

Construction order should keep `Platform` alive for the complete `Renderer` lifetime. Normal stack ownership naturally destroys the renderer before the platform when the renderer is created after the platform.

Simulation/scene systems remain responsible for gameplay truth. A later extraction step may copy authoritative state into `SpriteRenderData`, but render data is disposable presentation input.

## Headless invariant

The deterministic gameplay path must remain usable without GPU presentation.

`Renderer` rejects a headless `Platform` before GPU initialization. Existing runtime, scene, input, agent, and gameplay-testing modules do not depend on `Trace2D::Render`.

The camera math, draw-order comparison, visibility decision, and texture-handle fields remain CPU-owned data and can be tested without creating a window or GPU device.

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

The renderer also does not sort the submitted span. Ordering is a caller/extraction responsibility so the frame hot path does not allocate or reorder presentation data implicitly.

`IsSpriteVisible` performs inclusive axis-aligned overlap against the camera view. A sprite touching the view edge is visible. PR #28 applies this exact helper in the real submission loop; no separate GPU-only visibility rule exists.

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

Before command-buffer acquisition, every supplied sprite texture handle is validated against the live texture table. This preserves deterministic input validation even when a sprite would later be culled. Visible sprites then perform the direct lookup again when their actual texture binding is encoded. This duplicate O(1) lookup is intentionally preferred over building a transient resolved-resource array; remove it only if profiling shows it matters.

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
  -> wait/acquire swapchain texture
  -> build one OrthographicView from the actual target size
  -> begin render pass with clear load op
  -> for each caller-ordered sprite
       -> inclusive AABB visibility test
       -> if culled: increment local cull count and continue
       -> on first visible sprite only: bind persistent pipeline + unit-quad vertex buffer
       -> compute clip-space center / half extents
       -> push one 16-byte vertex uniform
       -> bind sprite texture + persistent sampler
       -> draw six vertices / one instance
  -> end render pass
  -> submit command buffer
  -> commit frame/draw/submitted/cull metrics after successful submission
```

A minimized/unavailable swapchain texture is treated as a valid no-presentation frame. It produces no sprite draw and no culling decision because no target-dependent `OrthographicView` exists for that frame.

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

The equality between visible-sprite and draw-call counts is the explicit pre-batching baseline. Once batching is introduced, `submittedSprites` should continue to describe encoded sprites while `drawCalls` may become smaller.

`trace2d run --windowed --json` exposes `draw_calls`, `submitted_sprites`, and `culled_sprites` so the baseline remains script-readable.

Metrics are renderer-owned observation state and are not authoritative gameplay state.

## Allocation / lifetime policy

Persistent renderer resources have renderer lifetime:

- GPU device
- claimed swapchain relationship
- unit-quad vertex buffer
- sprite graphics pipeline
- sampler
- created textures

Texture-table growth may allocate during explicit texture creation. The current span-based frame path performs no sprite-list allocation/copy, container growth, persistent-resource creation, shader compilation, renderer-side sorting, or texture upload.

Camera/view/sprite data is trivially copyable. Camera-view construction performs no dynamic allocation, `WorldToClip` performs no allocation or division, and draw-order/visibility helpers allocate no memory.

Culling is an O(N) direct pass fused into submission. It does not build a visible-sprite list. Fully culled input skips all sprite draw work and also skips pipeline/vertex-buffer binding.

Failure diagnostics may allocate strings. Driver-name storage is initialized once during renderer construction.

Before adding resource pools, bindless tables, custom allocators, atlases, persistent staging systems, or a spatial index, establish the sprite workload and record measurements that justify the complexity.

## Remaining P5 work

The next vertical slices are:

1. introduce only the batching implementation justified by the measured unbatched/culling baseline
2. add an offscreen color target suitable for visual QA
3. add deterministic frame-selected readback/capture artifact generation

The capture path must select simulation state by explicit frame number. Wall-clock timing must never decide which gameplay frame is captured.

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

The Public Alpha renderer only needs to prove the complete agent-first loop with a small sprite scene and deterministic visual capture.
