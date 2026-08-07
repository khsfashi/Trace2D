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

The clear-only pass is intentionally a foundation, not the P5 end state.

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
  does not own simulation state
```

Construction order should keep `Platform` alive for the complete `Renderer` lifetime. Normal stack ownership naturally destroys the renderer before the platform when the renderer is created after the platform.

## Headless invariant

The deterministic gameplay path must remain usable without GPU presentation.

`Renderer` rejects a headless `Platform` before GPU initialization. Existing runtime, scene, input, agent, and gameplay-testing modules do not depend on `Trace2D::Render`.

Do not move gameplay truth into renderer-owned resources just to make visual features easier to implement.

## Frame path

The initial windowed frame is:

```text
acquire GPU command buffer
  -> wait/acquire swapchain texture
  -> begin render pass with clear load op
  -> end render pass
  -> submit command buffer
  -> presentation occurs through the acquired swapchain texture
```

A minimized/unavailable swapchain texture is treated as a valid no-presentation frame rather than a renderer failure.

## Metrics

`RenderMetrics` currently records:

- submitted frame count
- presented frame count
- encoded render-pass count
- draw-call count
- submitted-sprite count
- last render-target width/height

`drawCalls` and `submittedSprites` remain zero until the sprite path is introduced. They exist now so later batching work has a stable baseline contract instead of adding unmeasured complexity first.

Metrics are renderer-owned observation state and are not authoritative gameplay state.

## Allocation / lifetime policy

The clear-frame hot path does not intentionally create persistent GPU resources per frame. The GPU device and swapchain claim have renderer lifetime.

Failure diagnostics may allocate strings. Driver-name storage is initialized once during renderer construction.

Before adding resource pools, bindless tables, custom allocators, or persistent staging systems, establish the sprite workload and record measurements that justify the complexity.

## Remaining P5 work

The next vertical slices are:

1. orthographic camera and minimal sprite render data
2. textured sprite pipeline
3. batching baseline with draw-call / sprite metrics
4. visibility/culling baseline
5. offscreen color target
6. deterministic frame-selected readback/capture artifact

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
