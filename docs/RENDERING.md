# Rendering

Trace2D rendering is presentation and visual-QA state. It must not become authoritative gameplay state.

This document records the P5 renderer contract as it is implemented. `PROJECT_STATUS.md` remains the source of truth for the active task and validation state.

## Current implementation

PR #24 establishes the renderer foundation:

- `engine/render` owns the Trace2D rendering API.
- SDL3 GPU types are private to the renderer implementation.
- `engine/platform` continues to own the SDL window lifetime.
- the public platform API exposes a numeric `WindowId`, not `SDL_Window*`.
- the renderer resolves that ID inside its SDL-backed implementation and claims the window for an SDL3 GPU device.
- renderer construction is valid only for a windowed `Platform`.
- headless runtime/tests do not create a GPU device or swapchain.
- one command buffer is acquired for the current minimal frame path.
- one swapchain texture is acquired with `SDL_WaitAndAcquireGPUSwapchainTexture`.
- the current render pass only clears and stores the swapchain target.
- submitting the command buffer presents the acquired swapchain image.

The next P5 slice adds a CPU-only render-data contract that the textured sprite pipeline can consume without making renderer-owned state authoritative:

- `OrthographicCamera` stores only camera center and full visible vertical world size.
- `TryBuildOrthographicView` derives target aspect ratio, world-space half extents, and cached world-to-clip scale.
- `WorldToClip` maps world positions into SDL GPU normalized device coordinates with +Y remaining up.
- `SpriteRenderData` carries axis-aligned center/half-extents plus deterministic draw-order keys.
- `SpriteDrawOrderLess` orders lower layers first, then `stableOrder`.
- `IsSpriteVisible` provides an inclusive axis-aligned camera-overlap baseline.
- the camera/view/sprite structs are trivially copyable and the helper functions allocate no memory.

The clear-only GPU pass is intentionally still the presentation foundation. The new render-data slice prepares deterministic inputs for the next textured draw path; it does not pretend sprites are already submitted to the GPU.

## Ownership

```text
Platform
  owns SDL initialization + SDL_Window
        |
        | SDL-free WindowId
        v
Renderer
  owns SDL_GPUDevice
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

The camera math, draw-order comparison, and visibility decision are CPU-only and can be tested without creating a window or GPU device.

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

The first `SpriteRenderData` is intentionally smaller than a production sprite component:

- `center` — world-space sprite center
- `halfExtents` — axis-aligned world-space half size
- `layer` — primary painter-order key; lower values draw first
- `stableOrder` — deterministic tie-break key within a layer

The current contract deliberately does **not** include texture handles, UVs, tint, material state, or rotation. Those belong in the textured sprite slice once the actual GPU data path is known.

Callers that require a total deterministic draw order must provide unique `stableOrder` values for sprites that share a layer. Equal `(layer, stableOrder)` pairs are intentionally equivalent to the comparator rather than falling back to pointer address, insertion timing, or another unstable process-local value.

`IsSpriteVisible` currently performs inclusive axis-aligned overlap against the camera view. A sprite touching the view edge is visible. This is a correctness-first baseline; more advanced rotated bounds or spatial indexing require measurement before adding complexity.

## Frame path

The current windowed GPU frame is still:

```text
acquire GPU command buffer
  -> wait/acquire swapchain texture
  -> begin render pass with clear load op
  -> end render pass
  -> submit command buffer
  -> presentation occurs through the acquired swapchain texture
```

A minimized/unavailable swapchain texture is treated as a valid no-presentation frame rather than a renderer failure.

The next GPU slice will insert textured sprite submission between render-pass begin/end using the CPU render-data contract above.

## Metrics

`RenderMetrics` currently records:

- submitted frame count
- presented frame count
- encoded render-pass count
- draw-call count
- submitted-sprite count
- last render-target width/height

`drawCalls` and `submittedSprites` remain zero until the textured sprite path is introduced. They exist now so later batching work has a stable baseline contract instead of adding unmeasured complexity first.

Metrics are renderer-owned observation state and are not authoritative gameplay state.

## Allocation / lifetime policy

The clear-frame hot path does not intentionally create persistent GPU resources per frame. The GPU device and swapchain claim have renderer lifetime.

Camera/view/sprite data is trivially copyable. Camera-view construction performs no dynamic allocation, `WorldToClip` performs no allocation or division, and draw-order/visibility helpers allocate no memory.

Failure diagnostics may allocate strings. Driver-name storage is initialized once during renderer construction.

Before adding resource pools, bindless tables, custom allocators, or persistent staging systems, establish the sprite workload and record measurements that justify the complexity.

## Remaining P5 work

The next vertical slices are:

1. textured sprite pipeline consuming `OrthographicView` / `SpriteRenderData`
2. measured batching baseline using draw-call / submitted-sprite metrics
3. integrate the CPU visibility baseline into actual sprite submission and measure its value
4. offscreen color target
5. deterministic frame-selected readback/capture artifact

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
