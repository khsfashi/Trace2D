# Material2D / Shader2D

Owning roadmap issue: **#89**. Current implementation slice: **#332 / MAT1**.

Material2D is the compact programmable 2D rendering primitive for Trace2D. It is not a material graph, render graph or finished-effect catalog. Finished presentation techniques belong first in Official Presentation Recipes and should promote only repeated missing primitives into engine core.

## MAT1 contract

MAT1 establishes the parameter-preparation ABI before executable custom GPU pipelines are introduced.

```text
canonical/future Shader2D resource identity
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

MAT1 intentionally supports only:

- `float`
- `float2`
- `color` / `float4`

Integer/bool and additional resource/sampler parameters are deferred until the executable Shader2D ABI demonstrates a concrete need and cross-backend representation.

### Layout

- maximum parameters: **16**
- maximum UTF-8/ASCII parameter-name bytes: **63**
- every parameter occupies exactly one **16-byte float4 slot**
- declaration order is slot order
- unused lanes are canonicalized to `0.0`
- all active authored lanes must be finite
- names must be ASCII identifier form `[A-Za-z_][A-Za-z0-9_]*`
- prepared names are copied into fixed-capacity owned storage; no prepared `string_view` points back into resource/authored lifetime
- duplicate names fail preparation

The deliberate bounded padding trades at most a few hundred bytes per prepared block for a backend-stable, std140-safe representation with no per-instance associative container. SDL GPU fragment uniform data requires std140-compatible layout and specifically calls out 16-byte alignment for vec3/vec4 data, so MAT1 uses the simpler invariant everywhere instead of backend-specific packing. The executable #89 slice will upload only `ActivePackedBytes()`.

### Deterministic identities

Layout identity is computed from:

- generation-safe resource handle slot
- resource generation
- resource domain
- declaration count/order
- declaration names
- declaration types

Value identity is computed from:

- layout identity
- active parameter count
- exact IEEE float bit patterns of the canonical packed slots

The hash algorithm is repository-owned and deterministic rather than `std::hash`, whose cross-process representation is not an authored ABI.

A binding resolved against one resource generation/layout therefore fails closed against another layout, including the same slot after generation changes.

### Hot-path rule

`ResolveMaterialParameterBinding2D()` is setup/tooling work. Normal retained draw/tween paths keep `MaterialParameterBinding2D` and must not resolve parameter names again.

`ApplyMaterialParameterOverrides2D()`:

- accepts only resolved bindings,
- copies one fixed-capacity block,
- detects duplicate overridden slots with a bounded bit mask,
- performs no string lookup,
- performs no reflection/filesystem/GPU work,
- requires no heap allocation.

This is the intended per-instance override substrate. A future caller may cache blocks by value identity when the same override set is reused.

## SR7 batching handoff

`SpriteBatchCompatibility2D` now reserves `materialParameters` as part of exact compatibility. Two visible adjacent Sprite items may merge only when texture, material/pipeline, sampler, blend, mask **and effective material parameter-block identity** are equal.

Painter order remains authoritative. Material identity never permits global sorting.

The current production Sprite GPU backend still executes only `BuiltInSpriteMaterialPipelineIdentity`; MAT1 does not pretend custom shader rendering exists.

## External-reference decisions

### SDL3 GPU / SDL_shadercross — ADOPT

Use SDL's existing graphics-shader binding convention, reflection path and fragment-uniform push mechanism for the executable backend. Trace2D already depends on the pinned SDL3/SDL_shadercross toolchain; no second shader compiler/binding abstraction is introduced.

The SDL GPU guidance to prepare/cache static GPU resources, minimize state changes and avoid unnecessary render passes matches Trace2D's retained-resource and contiguous-batching rules.

### Godot ShaderMaterial / CanvasItem instance uniforms — ADAPT

The useful production lesson is the ownership split:

```text
shared Shader
 -> Material defaults
 -> optional per-instance values
```

Trace2D adopts that separation, but not Godot's runtime `StringName`/`Variant` parameter surface. Trace2D resolves names at setup into typed slots and carries bounded packed values through steady rendering.

### Material graph / generic property bag — REJECT

MAT0/#89 explicitly does not need a public material graph or `unordered_map<string, Variant>` per Sprite. Those would increase Agent surface area, allocation behavior and runtime ambiguity without helping the current 2D fragment-processing goal.

## Next #89 slice after MAT1

Do not advance to #90 when MAT1 merges. #89 remains active and must next add:

1. dedicated canonical Shader2D / Material2D publication through the existing #86 resource lifecycle,
2. explicit shader import/preparation and deterministic diagnostics,
3. shadercross reflection/compilation outside ordinary draw submission,
4. immutable shader module / graphics-pipeline cache keyed by generation-safe resource + blend/mask/render-target compatibility,
5. real custom fragment rendering through the frozen Sprite vertex/presentation ABI,
6. `SDL_PushGPUFragmentUniformData` using MAT1 packed blocks,
7. public/headless material inspection and retained cache/upload metrics,
8. representative custom-fragment GPU tests and the required real-GPU tier handoff.

No arbitrary vertex deformation, public render graph, always-on extra pass or effect-specific C++ subsystem is authorized by MAT1.