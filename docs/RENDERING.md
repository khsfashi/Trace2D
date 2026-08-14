# Rendering

Trace2D rendering is presentation and visual-QA state. It must not become authoritative gameplay state.

`PROJECT_STATUS.md` is the source of truth for the active release task. This document records the implemented renderer contract.

## Current implementation

The renderer is isolated in `engine/render` behind Trace2D-owned public types. SDL3 GPU types remain private to the implementation, while `engine/platform` owns SDL initialization and the window lifetime.

Implemented renderer capabilities:

- windowed-only SDL3 GPU renderer construction; headless runtime never initializes a GPU renderer,
- Trace2D-owned orthographic camera/view math,
- Trace2D-owned `SpriteRenderData` and generation-safe typed `TextureHandle` shared with the R0 `ResourceRegistry`,
- caller-ordered textured multi-sprite submission,
- inclusive allocation-free AABB visibility filtering,
- full-span texture validation independent of camera visibility,
- measured contiguous same-texture GPU instancing,
- persistent/capacity-reused instance GPU and upload transfer buffers,
- cumulative submitted/drawn/culled metrics,
- persistent offscreen color target used for presentation and capture,
- explicit simulation-frame GPU readback,
- reusable capture download transfer buffer,
- canonical packed top-down RGBA8 CPU pixels,
- deterministic dependency-free 32-bit BMP artifacts.

## Ownership

```text
Platform
  owns SDL initialization + SDL_Window
        |
        | SDL-free WindowId
        v
Renderer
  owns SDL_GPUDevice
  owns sprite pipeline / unit-quad vertex buffer / sampler
  owns persistent sprite instance GPU + upload buffers
  owns SDL texture residency keyed by canonical ResourceRegistry slot + generation
  owns persistent size-matched offscreen color target
  owns reusable capture download transfer buffer
  claims/releases window swapchain
  acquires/submits command buffers
  consumes derived render data
  does not own canonical resource identity or simulation state
```

Construction order must keep `Platform` alive for the complete `Renderer` lifetime. Simulation, scene, input, agent, and gameplay-testing layers remain renderer-independent.

## Headless invariant

`Renderer` rejects a headless `Platform` before GPU initialization. Deterministic runtime/scene/input/query/assertion workflows do not depend on renderer presentation state.

Backend-independent helpers remain CPU-testable without a window or GPU device:

- orthographic view construction,
- world-to-clip conversion,
- `SpriteInstanceData` transform packing,
- draw-order comparison,
- visibility testing,
- contiguous texture-run measurement,
- capture readback-layout calculation,
- deterministic artifact encoding.

## Orthographic camera contract

`OrthographicCamera::verticalSize` is the full visible world-space height. Horizontal size is derived from target aspect ratio.

```text
halfHeight = verticalSize / 2
halfWidth  = halfHeight * targetWidth / targetHeight
clip.x     = (world.x - center.x) / halfWidth
clip.y     = (world.y - center.y) / halfHeight
```

`TryBuildOrthographicView` performs the divisions once and caches reciprocal extents in `clipScale`. `WorldToClip` then requires subtraction and multiplication only.

The engine convention keeps +Y up. Invalid zero-sized targets, non-finite camera values, or non-positive vertical size are rejected.

## Sprite render-data contract

`SpriteRenderData` contains only the current measured requirements:

- `center` — world-space center,
- `halfExtents` — axis-aligned world-space half size,
- `texture` — canonical R0 `TextureHandle` identifying renderer residency,
- `layer` — primary painter-order key,
- `stableOrder` — deterministic tie-break key within a layer.

It intentionally does not include rotation, UV rectangles, tint, generic materials, or arbitrary shader state yet.

The renderer **does not sort** the submitted span. Caller/extraction code is responsible for producing the desired deterministic painter order. Texture identity never participates in ordering.

`IsSpriteVisible` performs inclusive AABB overlap against the view. A sprite touching the view edge is visible.

## Sprite instance contract

PR #34 replaces the per-sprite 16-byte vertex-uniform push with an instance-rate vertex stream.

`SpriteInstanceData` is a trivially-copyable 16-byte backend-independent structure:

```text
float2 centerClip
float2 halfClip
```

`BuildSpriteInstanceData` converts one visible `SpriteRenderData` to clip-space center/half-extents using the cached orthographic view scale.

The persistent six-vertex unit quad remains unchanged. The graphics pipeline binds:

- slot 0 — `SpriteVertex`, vertex rate,
- slot 1 — `SpriteInstanceData`, instance rate.

SDL's required `instance_step_rate = 0` contract is preserved. The vertex shader consumes the instance transform as one `float4`; no per-sprite uniform update is needed.

## Contiguous same-texture batching

Batching is deliberately restricted to the visible caller sequence.

For visible texture sequence:

```text
A, A, A, B, B, A
```

the renderer emits:

```text
Draw A: first_instance=0, instance_count=3
Draw B: first_instance=3, instance_count=2
Draw A: first_instance=5, instance_count=1
```

A culled sprite contributes no instance and does not split a visible run. No global texture sort or renderer-owned visible-sprite vector is created.

The committed Public Alpha workload has seven visible sprites in two contiguous texture runs, so its ordered draw contract changes from seven unbatched draws to two instanced draws while `submittedSprites` remains seven.

## Instance-buffer lifetime and upload

The renderer owns one persistent instance GPU buffer and one persistent upload transfer buffer.

Capacity policy:

- zero visible sprites require no instance buffer work,
- retained capacity is reused while sufficient,
- capacity grows geometrically when the measured visible count exceeds retained capacity,
- replacement GPU and transfer resources are created before old resources are released,
- ordinary steady-capacity frames do not recreate application-level buffers.

Per visible frame:

1. map the retained upload transfer buffer with SDL cycling,
2. compact only visible `SpriteInstanceData` records into mapped storage in caller order,
3. unmap before GPU upload,
4. upload the visible byte range to the persistent GPU instance buffer with destination cycling,
5. bind the unit-quad and instance buffers once,
6. emit one draw per contiguous visible texture run.

This intentionally trades a few simple O(N) scans for zero per-frame visible-list allocation and stable resource ownership. The separate full-span texture validation scan is retained so invalid texture behavior cannot depend on camera visibility.

## Texture lifetime and upload

`CreateTextureRgba8` and `CreateSpriteTextureRgba8` are explicit GPU-residency upload work outside frame submission. Production callers first publish or resolve a canonical `TextureResource` through `assets::ResourceRegistry`, then pass that generation-safe typed handle together with the upload bytes.

The renderer never allocates texture identity. For each upload it validates the canonical texture handle plus dimensions/byte count, creates one sampled `SDL_GPUTexture`, uploads source bytes through a transfer buffer/copy pass, and records the derived GPU residency at the canonical slot with the exact canonical generation. Residency storage may grow only during this explicit setup/upload work when a higher canonical slot is introduced.

`DestroyTexture` releases derived GPU residency only when both canonical slot and generation match. It does not unload the canonical `ResourceRegistry` entry. A stale-generation destroy is a no-op, so a retired handle cannot release a replacement texture that reused the same canonical slot with a newer generation. Canonical unload remains an explicit caller/project lifecycle operation after renderer residency is released and normal dependency/retain rules permit it.

Every supplied sprite texture handle is validated before command-buffer acquisition. Frame-time lookup is direct slot bounds + generation validation followed by direct SDL texture pointer access; there is no path normalization, string/hash lookup, ownership increment/decrement, or allocation. Actual visible run encoding resolves the run texture again through that same O(1) residency table instead of building a transient resolved-resource array.

## Offscreen target and presentation

Scene rendering targets a renderer-owned offscreen texture rather than the swapchain directly.

The target:

- matches the claimed swapchain format,
- matches the actual acquired presentation dimensions,
- is reused while dimensions are unchanged,
- is replaced only on first usable frame or real size change,
- uses create-before-release replacement,
- uses SDL color-target cycling for safe reuse.

After the render pass, one GPU copy pass copies the completed offscreen image into the acquired swapchain texture. The swapchain is presentation-only and never the capture source.

A minimized/unavailable swapchain texture is a valid no-presentation ordinary frame. Explicit capture fails clearly when no valid presentation target exists.

## Deterministic capture contract

`CaptureRequest` contains:

- `simulationFrame` — authoritative simulation frame selected by the caller,
- `artifactPath` — output path,
- `format` — currently BMP only.

The renderer does not infer simulation identity from wall-clock time and never advances gameplay state.

`CaptureFrame` is intentionally synchronous for Public Alpha:

1. render into the normal offscreen target,
2. copy the target to the swapchain for presentation,
3. download the same completed offscreen target into a reusable download transfer buffer,
4. submit with a GPU fence,
5. wait only for this explicit capture,
6. map after completion,
7. normalize supported RGBA/BGRA target bytes into packed top-down RGBA8,
8. write a deterministic top-down 32-bit BMP,
9. return the normalized `CapturedFrame`.

Normal non-capture frames perform no capture download, fence wait, CPU pixel mapping, image encoding, or file I/O.

Canonical CPU pixel layout:

```text
origin        = top-left
row order     = top to bottom
pixel order   = left to right
channel order = R, G, B, A
bytes/channel = 1
row padding   = none
```

Readback rows use a 256-byte-aligned pitch before normalization to satisfy the stricter D3D12 transfer requirements exposed through SDL.

## Shader packaging baseline

The repository-pinned `SDL3_shadercross` package compiles the small embedded HLSL vertex/fragment shaders during renderer construction:

1. initialize shadercross,
2. compile HLSL to SPIR-V,
3. reflect resources,
4. create backend-compatible SDL GPU shaders,
5. create the persistent graphics pipeline,
6. release temporary shader objects and shut down shadercross state.

Shader compilation is setup work only; it never occurs inside `RenderFrame`.

The current vertex shader reads unit-quad vertex position/UV plus `SpriteInstanceData`. The fragment shader samples the bound sprite texture using the persistent nearest/clamp sampler and standard source-alpha blending.

Offline precompiled shaders remain a later measurement-driven packaging decision.

## Frame path

The normal ordered multi-sprite windowed frame is:

```text
validate every supplied canonical texture slot + generation
  -> acquire GPU command buffer
  -> wait/acquire swapchain texture and actual target size
  -> if presentation target is available:
       -> build OrthographicView
       -> measure visible/culled sprites + contiguous visible texture runs
       -> reuse/grow persistent instance capacity only if required
       -> map cycled upload buffer
       -> compact visible clip-space instance transforms in caller order
       -> unmap
       -> upload visible instance range through GPU copy pass
       -> reuse/resize offscreen target only if required
       -> begin render pass on offscreen target
       -> bind sprite pipeline + unit-quad + instance buffers
       -> scan visible caller sequence
            -> bind texture/sampler once per contiguous run
            -> draw six vertices with run instance count/first instance
       -> end render pass
       -> copy offscreen target to swapchain
  -> submit command buffer
  -> commit actual successful draw/submitted/cull metrics
```

Explicit capture adds download/fence/map/normalize/artifact work after the same offscreen render result.

## Metrics

`RenderMetrics` records cumulative:

- `submittedFrames`,
- `presentedFrames`,
- `renderPasses`,
- `drawCalls`,
- `submittedSprites`,
- `culledSprites`,
- last target width/height.

After contiguous instancing, sprite and draw metrics are intentionally independent:

```text
submittedSprites delta = visible instances encoded
culledSprites delta    = supplied sprites rejected by visibility
drawCalls delta        = contiguous visible texture runs drawn
```

Presentation copies and capture downloads are transfer work and do not increment sprite draw calls.

Metrics are committed only after successful command-buffer submission, so failed speculative encoding does not become authoritative observation state.

## Allocation and performance policy

Persistent/capacity-managed resources:

- GPU device,
- swapchain claim,
- graphics pipeline,
- unit-quad vertex buffer,
- sprite instance GPU buffer,
- sprite instance upload transfer buffer,
- sampler,
- renderer-owned GPU texture residency keyed by canonical handles,
- offscreen color target,
- capture download transfer buffer.

Steady-capacity non-capture frames do not allocate a visible-sprite container, sort sprites, compile shaders, create textures, resize texture-residency storage, recreate instance buffers, recreate offscreen targets, create capture buffers, wait on fences, encode images, or perform file I/O.

CPU submission remains O(N) direct scans. Texture residency lookup remains O(1) slot/generation validation. No LINQ-like abstraction, per-frame path/hash lookup, shared-ownership churn, renderer-owned frame scene, generic frame allocator, or render graph has been introduced.

Capture intentionally allocates its returned canonical RGBA8 byte vector and artifact conversion storage because capture is explicit QA work outside the ordinary frame hot path.

## Validation boundary

GitHub Actions validates the Windows/MSVC configuration, warnings-as-errors build, and full CTest suite on clean hosted runners. The owner Windows GPU gate additionally runs the committed GPU smoke/conformance surface with `TRACE2D_RUN_GPU_SMOKE=1` and treats a skipped GPU test as failure.

The committed `trace2d public-alpha --windowed ... --json` command is the explicit presentation smoke surface. For the seven-sprite sample, successful renderer submission is expected to report:

```text
submitted_sprites = 7
draw_calls        = 2
```

R0 renderer texture lifecycle integration also has a real-GPU smoke proving that canonical slot reuse advances generation, stale-generation rendering is rejected, and stale `DestroyTexture` cannot release the live replacement generation.

## Non-goals for Public Alpha

Do not expand this renderer slice into:

- global texture sorting,
- generic material system,
- render graph,
- bindless/GPU-driven scene architecture,
- lighting/PBR,
- editor renderer,
- broad asset-import pipeline,
- custom allocator framework,
- async/video capture framework.

The first public renderer only needs a deterministic, observable, measurable 2D presentation path that composes cleanly with the agent-first runtime workflow.
