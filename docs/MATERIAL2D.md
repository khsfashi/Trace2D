# Material2D / Shader2D

Owning roadmap issue: **#89**. Completed substrate: **#332 / MAT1**, **#334 / MAT2**. Current executable slice: **#336 / MAT3**.

Material2D is the compact programmable 2D rendering primitive for Trace2D. It is not a material graph, render graph or finished-effect catalog. Finished presentation techniques belong first in Official Presentation Recipes and should promote only repeated missing primitives into engine core.

## MAT1 prepared-parameter contract

MAT1 established the parameter-preparation ABI before executable custom GPU pipelines are introduced.

```text
canonical Shader2D resource identity
  + finite parameter declarations
        |
        v
PrepareMaterialParameterLayout2D()       setup/tooling only
  name -> { layout identity, type, slot }
        |
        +--> MaterialParameterBinding2D  retained by callers/tween bindings
        |
        v
PrepareMaterialParameterBlock2D()        setup/material defaults
        |
        v
ApplyMaterialParameterOverrides2D()      steady-capable bounded copy/update
        |
        v
MaterialParameterBlock2D
  { packed float4 slots, layout fingerprint, value fingerprint }
        |
        v
SR7 exact contiguous batch compatibility
```

### Parameter vocabulary

The shared canonical/prepared vocabulary intentionally supports only:

- `float`
- `float2`
- `color` / `float4`

`assets::MaterialParameterType2D` and `assets::MaterialParameterValue2D` are the single canonical definitions. The renderer exposes source-compatible aliases rather than maintaining a second type system.

Integer/bool and additional resource/sampler parameters are deferred until executable Shader2D evidence demonstrates a concrete need and cross-backend representation.

### Layout

- maximum parameters: **16**
- maximum parameter-name bytes: **63**
- every prepared parameter occupies exactly one **16-byte float4 slot**
- declaration order is slot order
- unused lanes are canonicalized to `0.0`
- all active authored lanes must be finite
- names must be ASCII identifier form `[A-Za-z_][A-Za-z0-9_]*`
- prepared names are copied into fixed-capacity owned storage
- duplicate names fail preparation/publication

The bounded padding trades at most a few hundred bytes per prepared block for a backend-stable, std140-safe representation with no per-instance associative container. SDL GPU fragment uniform data requires std140-compatible layout and 16-byte alignment for vec3/vec4 data, so Trace2D uses the simpler invariant everywhere. MAT3 uploads only `ActivePackedBytes()`.

### Deterministic identities

Layout identity is computed from generation-safe resource identity plus declaration count/order/names/types. Value identity additionally includes the active parameter count and exact canonical packed float bits. The hash is repository-owned and deterministic rather than `std::hash`.

A binding resolved against one resource generation/layout therefore fails closed against another layout, including the same slot after generation changes.

### Hot-path rule

`ResolveMaterialParameterBinding2D()` is setup/tooling work. Normal retained draw/tween paths keep `MaterialParameterBinding2D` and must not resolve parameter names again.

`ApplyMaterialParameterOverrides2D()` accepts only resolved bindings, copies one fixed-capacity block, detects duplicate overridden slots with a bounded bit mask, performs no string lookup/reflection/filesystem/GPU work and requires no heap allocation.

## MAT2 canonical resource contract

MAT2 publishes Shader2D and Material2D through the existing #86 `ResourceRegistry`. It deliberately does not create a second resource allocator or put SDL handles in canonical state.

```text
project-relative Shader2D
  -> ResourceRegistry<Shader2DResource>
     { HLSL fragment source, entry point, generation-safe handle }
             ^
             | strong dependency
project-relative Material2D
  -> ResourceRegistry<Material2DResource>
     { shader handle, finite defaults, sampler, blend }
```

MAT2 accepts HLSL fragment source, a bounded ASCII entry point, non-empty canonical source and required CPU retention. A Material2D references one generation-safe Shader2D handle as a strong #86 dependency. Wrong-domain/stale/not-ready shader handles fail before publication, live Material2D prevents shader unload, and canonical defaults share MAT1 validation/canonicalization.

Canonical resources contain no SDL shader, pipeline or sampler handles. Existing #86 inspection reports retained CPU logical bytes/container capacity/retention/dependency evidence; unknown driver allocation is not fabricated as exact GPU memory.

## MAT3 executable GPU contract

MAT3 is an explicit setup-to-draw vertical slice. It does not compile lazily from `RenderFrame`.

```text
ResourceRegistry Material2D -> generation-safe Shader2D
        |
        v
Renderer::PrepareMaterial2D()                   setup only
  -> MAT1 layout/default block
  -> HLSL -> SDL_shadercross SPIR-V
  -> graphics reflection / frozen Sprite ABI validation
  -> immutable renderer-owned pipeline bundle cache
        |
        v
PreparedMaterial2D
  { integer pipeline identity, sampler/blend, MAT1 layout/defaults }
        |
        v
SpritePresentationRenderData
  materialPipeline + MaterialParameterBlock2D*
        |
        v
existing SR7 exact contiguous batching / painter order
        |
        +--> SDL_PushGPUFragmentUniformData(slot 0, ActivePackedBytes())
        +--> existing Sprite texture/sampler binding
        +--> existing single Sprite render pass
```

### Frozen fragment ABI

MAT3 custom fragment shaders must match the existing Sprite presentation vertex output:

- input location 0: `float2 uv`
- input location 1: `float4 sampleBounds`
- input location 2: `float4 tint`
- output location 0: one `float4` color
- exactly one sampled Sprite texture/sampler
- no storage textures/buffers
- zero uniform buffers when the material has no MAT1 parameters, otherwise exactly one fragment uniform buffer

For HLSL/DXIL convention this means the Sprite texture/sampler use `t0/s0, space2`, while MAT1 values use `b0, space3`. Each authored MAT1 parameter must be represented by one declaration-order `float4` field in that buffer; scalar/float2 semantics consume `.x`/`.xy` while preserving the fixed 16-byte slot ABI.

### Cache and failure rules

The renderer cache key contains generation-safe Shader2D identity, MAT1 layout identity, sampler and blend compatibility. A cache hit reuses the same integer pipeline identity and performs no shader compile or graphics-pipeline creation.

Each first preparation creates the custom fragment shader and the same bounded four pipeline variants needed by the existing Sprite mask compatibility surface: normal unmasked, stencil-compatible unmasked, mask-inside and mask-outside. Mask **writing** intentionally remains the frozen base-alpha writer; MAT3 does not add a custom mask-generation pass.

Compilation, reflection or pipeline creation failure returns a deterministic `MaterialGpuPrepareError2D`/diagnostic and never publishes the partially prepared record. Retrying a failed resource therefore cannot become a false cache hit.

### Steady rendering

Ordinary Sprite submission performs no shader compilation, reflection, filesystem access or parameter-name lookup. A custom draw resolves its prepared pipeline by integer vector index, validates the fixed block/layout identity, preserves existing exact contiguous compatibility/painter order, and pushes only the active fixed MAT1 bytes before the compatible draw run.

No material-driven global sorting is introduced. No additional render pass is introduced.

### Evidence counters

`RenderMetrics` now exposes:

- `materialShaderCompilations`
- `materialPipelineCreations`
- `materialPipelineCacheHits`
- `materialPipelineSwitches`
- `fragmentUniformUploads`
- `fragmentUniformUploadBytes`

Setup/cache counters are renderer-lifetime cumulative state. Per-frame switch/upload counters are committed only after successful command submission, matching the existing renderer metrics policy.

## SR7 batching handoff

`SpriteBatchCompatibility2D` includes texture, material/pipeline, sampler, blend, mask and effective MAT1 parameter-block value identity. Two adjacent visible Sprite items merge only when the complete tuple is equal. Painter order remains authoritative; Material identity never permits global sorting.

Built-in Sprite rendering remains the default identity and carries no MAT1 block. Prepared custom Sprite rendering requires a pipeline identity produced by the same renderer plus a block whose layout identity matches that prepared pipeline.

## External-reference decisions

### SDL3 GPU / SDL_shadercross — ADOPT

Use SDL's graphics-shader binding convention, reflection path and fragment-uniform push mechanism. Trace2D already depends on the pinned SDL3/SDL_shadercross toolchain; no second shader compiler/binding abstraction is introduced.

The SDL GPU guidance to prepare/cache static GPU resources, minimize state changes and avoid unnecessary render passes matches Trace2D's retained-resource and contiguous-batching rules.

### Godot ShaderMaterial / CanvasItem instance uniforms — ADAPT

The useful production lesson is the ownership split `shared Shader -> Material defaults -> optional per-instance values`. Trace2D adopts that separation, but not a runtime `StringName`/`Variant` parameter surface. Names resolve at setup into typed slots and bounded packed values flow through steady rendering.

### Parallel registry, material graph, generic property bag — REJECT

#86 already owns generation-safe typed resource identity/lifecycle. A second Shader/Material allocator would create stale-generation and teardown ambiguity. #89 also explicitly does not need a public material graph, render graph or `unordered_map<string, Variant>` per Sprite.

## Next #89 work after MAT3

Do not advance to #90 merely because one custom shader executes. Remaining #89 acceptance should use MAT3 evidence to harden the smallest missing pieces: deterministic diagnostic coverage, representative custom material/batching evidence, real-GPU tier evidence and performance/accounting closure. Effect-specific flash/pulse/dissolve catalog work belongs in Official Presentation Recipes unless repeated evidence proves a missing engine primitive.
