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

PR #30 added the persistent offscreen presentation target:

- scene rendering no longer writes directly into the swapchain texture.
- the renderer creates one offscreen color target lazily when a non-zero presentation target first becomes available.
- the offscreen target uses the actual swapchain texture format and dimensions so presentation can use an exact texture-to-texture copy.
- the target is reused while dimensions remain unchanged and replaced only when the presentation target size changes.
- replacement is create-before-release.
- the render pass clears/stores into the offscreen target and uses SDL GPU target cycling for normal frame reuse.
- after the render pass, a GPU copy pass copies the completed offscreen image into the acquired swapchain texture.
- the swapchain remains presentation-only and is never the capture source.

The deterministic capture slice builds directly on that target:

- `CaptureRequest` carries an explicit `simulationFrame`, artifact path, and format.
- capture is opt-in work; ordinary `RenderFrame` calls do not download, wait on a fence, map pixels, encode an image, or perform file I/O.
- the renderer owns one reusable download transfer buffer and grows it only when required capacity increases.
- readback rows are padded to a 256-byte pitch so D3D12 backends do not need an avoidable temporary row-pitch repack.
- requested frames download the completed offscreen target in the same GPU copy pass used for presentation.
- capture submission acquires a fence; CPU mapping occurs only after that fence is signaled.
- supported RGBA8/BGRA8 SDR target bytes are normalized into packed, top-down canonical RGBA8 CPU pixels.
- the first artifact format is a dependency-free, lossless 32-bit top-down BMP.
- `trace2d run --windowed --frames N --capture PATH` binds the artifact to the exact `RuntimeState.frame` produced by explicit stepping.

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
  owns reusable capture download transfer buffer
  claims/releases window swapchain
  acquires/submits command buffers
  renders into offscreen presentation state
  copies completed offscreen output into the swapchain
  optionally downloads explicitly requested frames
  consumes derived render data
  does not own simulation state
```

Construction order should keep `Platform` alive for the complete `Renderer` lifetime. Normal stack ownership naturally destroys the renderer before the platform when the renderer is created after the platform.

Simulation/scene systems remain responsible for gameplay truth. Render data and capture pixels are disposable presentation/QA output.

## Headless invariant

The deterministic gameplay path must remain usable without GPU presentation.

`Renderer` rejects a headless `Platform` before GPU initialization. Existing runtime, scene, input, agent, and gameplay-testing modules do not depend on `Trace2D::Render`.

The camera math, draw-order comparison, visibility decision, batching measurement, capture readback-layout calculation, and artifact encoding tests remain usable without creating a window or GPU device.

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

The offscreen color target is renderer-owned presentation state and the sole capture source.

Its format is queried once from the claimed window swapchain and used both by the sprite graphics pipeline and the size-dependent offscreen texture. Its dimensions are taken from the actual acquired swapchain texture rather than guessed from logical window size.

Normal frames do not create or release an offscreen texture. `EnsureOffscreenColorTarget` returns immediately when the existing target dimensions match. Resource churn occurs only on first usable presentation or a real target-size change.

When a resize requires replacement, the new target is created before the old one is released. SDL releases GPU textures only when safe, so a prior submitted frame may continue using the old backing resource without an application-side stall.

The render pass uses `SDL_GPU_LOADOP_CLEAR`, `SDL_GPU_STOREOP_STORE`, and target cycling. After the pass ends, a GPU copy pass copies the full target into the same-format, same-size swapchain texture. A requested capture additionally downloads that same completed offscreen image before submission.

A successfully acquired swapchain texture is not cancelled on later command-encoding failure. SDL documents cancellation after swapchain acquisition as invalid, so those exceptional paths submit the already-acquired command buffer before propagating the error.

## Deterministic capture contract

`CaptureRequest` is deliberately small:

- `simulationFrame` identifies the authoritative simulation state selected by the caller.
- `artifactPath` identifies the requested output artifact.
- `format` is currently `Bmp` only.

The renderer does not infer frame identity from wall-clock time and does not advance simulation. The caller first selects/steps simulation state, then passes that state's frame number with the derived render data.

`CaptureFrame` is intentionally synchronous for the first Public Alpha contract. It:

1. renders the supplied presentation data into the normal offscreen color target,
2. presents that target to the swapchain exactly as an ordinary frame does,
3. downloads the completed offscreen target into a reusable `SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD` buffer,
4. submits with `SDL_SubmitGPUCommandBufferAndAcquireFence`,
5. waits for the capture fence,
6. maps the download buffer only after completion,
7. strips aligned row padding and normalizes RGBA/BGRA bytes into packed top-down RGBA8,
8. writes the deterministic BMP artifact,
9. returns the normalized `CapturedFrame` for tests/tools that need the bytes.

The transfer layout uses a 256-byte aligned row pitch with offset zero. This satisfies the stricter D3D12 row-pitch requirement documented by SDL while remaining valid on other backends. The buffer is recreated only when the required capacity exceeds the retained capacity.

Canonical CPU capture layout is:

```text
origin        = top-left
row order     = top to bottom
pixel order   = left to right
channel order = R, G, B, A
bytes/channel = 1
row padding   = none
```

The BMP writer is an artifact adapter over those canonical bytes; it emits top-down 32-bit BGRA pixel rows without compression. No image codec library is added for P5.

The explicit CLI smoke surface is:

```text
trace2d run --windowed --frames N --capture PATH
```

The artifact is identified as simulation frame `N` because `run` first performs `runtime.Step(N)`, reads the resulting `RuntimeState.frame`, and passes that exact value into `CaptureRequest`.

Deterministic capture here means deterministic state selection, byte-layout normalization, and artifact encoding. It does not claim bit-identical floating-point rasterization across unrelated GPU vendors/backends.

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

The normal ordered multi-sprite windowed frame is:

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

A requested capture adds only this work to the same rendered frame:

```text
before copy pass: ensure reusable aligned download capacity
inside copy pass: download offscreen target into transfer buffer
submit: acquire fence
wait fence
map download buffer
normalize packed RGBA8
unmap + release fence
write artifact
```

A minimized/unavailable swapchain texture is a valid no-presentation frame for ordinary rendering. Explicit capture instead fails clearly because there is no valid offscreen target size to capture.

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

Presentation copy and capture download are transfer work, not sprite draw calls, and do not change `drawCalls` or `submittedSprites` semantics.

Once batching is introduced, `submittedSprites` should continue to describe encoded sprites while `drawCalls` may become smaller.

`trace2d run --windowed --json` exposes `draw_calls`, `submitted_sprites`, and `culled_sprites`. With `--capture`, JSON additionally exposes capture frame, dimensions, format, and artifact path.

Metrics are renderer-owned observation state and are not authoritative gameplay state.

## Allocation / lifetime policy

Persistent renderer resources have renderer lifetime or a size/capacity-dependent presentation lifetime:

- GPU device
- claimed swapchain relationship
- unit-quad vertex buffer
- sprite graphics pipeline
- sampler
- created sprite textures
- offscreen color target, reused until presentation size changes
- capture download transfer buffer, allocated lazily and capacity-reused across requested captures

Texture-table growth may allocate during explicit texture creation. The normal steady-size non-capture frame path performs no sprite-list allocation/copy, container growth, shader compilation, texture upload, offscreen texture creation, download-buffer creation, fence wait, renderer-side sorting, image encoding, or file I/O.

Capture intentionally allocates the returned canonical RGBA8 byte vector and one row-sized BMP conversion buffer. Those allocations occur only on explicit capture and are outside the ordinary frame hot path.

Camera/view/sprite data is trivially copyable. Camera-view construction performs no dynamic allocation, `WorldToClip` performs no allocation or division, and draw-order/visibility/batching-measurement/readback-layout helpers allocate no memory.

Culling is an O(N) direct pass fused into submission. It does not build a visible-sprite list. Fully culled input skips all sprite draw work and also skips pipeline/vertex-buffer binding.

Failure diagnostics may allocate strings. Driver-name storage is initialized once during renderer construction.

Before adding resource pools, bindless tables, custom allocators, atlases, persistent staging systems, or a spatial index, establish the workload and record measurements that justify the complexity.

## Remaining P5 work

Actual GPU sprite instancing remains deferred until a representative Public Alpha sample demonstrates a material saving with the existing contiguous-texture measurement.

For Issue #10, the renderer/capture implementation is complete once Windows/MSVC CI validates this capture slice and the explicit CLI artifact path. After that, close #10 and move to the tiny Public Alpha vertical sample/release-quality repository checks tracked by #14.

Do not grow the synchronous capture API into a generic async readback framework unless later measurements/use cases justify it.

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
