# Material2D / Shader2D

Owning roadmap issue: **#89**. Completed substrate: **#332 / MAT1**. Current implementation slice: **#334 / MAT2**.

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

Integer/bool and additional resource/sampler parameters are deferred until the executable Shader2D ABI demonstrates a concrete need and cross-backend representation.

### Layout

- maximum parameters: **16**
- maximum parameter-name bytes: **63**
- every prepared parameter occupies exactly one **16-byte float4 slot**
- declaration order is slot order
- unused lanes are canonicalized to `0.0`
- all active authored lanes must be finite
- names must be ASCII identifier form `[A-Za-z_][A-Za-z0-9_]*`
- prepared names are copied into fixed-capacity owned storage; no prepared `string_view` points back into resource/authored lifetime
- duplicate names fail preparation/publication

The deliberate bounded padding trades at most a few hundred bytes per prepared block for a backend-stable, std140-safe representation with no per-instance associative container. SDL GPU fragment uniform data requires std140-compatible layout and 16-byte alignment for vec3/vec4 data, so Trace2D uses the simpler invariant everywhere instead of backend-specific packing. The executable #89 slice will upload only `ActivePackedBytes()`.

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
             |
             v
MAT1 preparation (later executable backend consumes this)
```

### Shader2D baseline

MAT2 accepts only the currently proven baseline:

- HLSL source,
- fragment stage,
- bounded ASCII identifier entry point,
- non-empty canonical source,
- required CPU retention while deterministic preparation still consumes canonical source.

Compilation, reflection and backend shader objects remain derived renderer/setup state for the next #89 slice.

### Material2D ownership

A canonical Material2D owns no backend object. It references one generation-safe Shader2D handle and publishes that relationship as a strong #86 dependency.

Therefore:

- wrong-domain shader handles fail before publication,
- stale/not-ready shader generations fail before publication,
- a live Material2D prevents unloading its Shader2D,
- teardown removes the material first and then permits the shader to unload,
- duplicate canonical resource references reuse the existing immutable ready resource.

Canonical parameter defaults use the same 16-parameter vocabulary and validation as MAT1. Inactive lanes are normalized to zero during publication so authored state and prepared state do not preserve meaningless payload bits.

### Memory / inspection

Existing #86 inspection now reports retained CPU logical bytes, container capacity, retention policy/reason and dependency/dependent identity for both Shader2D and Material2D. Unknown driver allocation is still not reported as exact GPU memory; MAT2 creates no GPU residency.

## SR7 batching handoff

`SpriteBatchCompatibility2D` reserves `materialParameters` as part of exact compatibility. Two visible adjacent Sprite items may merge only when texture, material/pipeline, sampler, blend, mask **and effective material parameter-block identity** are equal.

Painter order remains authoritative. Material identity never permits global sorting.

The current production Sprite GPU backend still executes only `BuiltInSpriteMaterialPipelineIdentity`; MAT2 does not pretend custom shader rendering exists.

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

### Parallel material/shader registry, material graph, generic property bag — REJECT

#86 already owns generation-safe typed resource identity/lifecycle. A second Shader/Material allocator would create stale-generation and teardown ambiguity. MAT0/#89 also explicitly does not need a public material graph or `unordered_map<string, Variant>` per Sprite.

## Next #89 slice after MAT2

Do not advance to #90 when MAT2 merges. #89 remains active and must next add a narrow executable preparation/rendering slice:

1. explicit shadercross compile + reflection outside ordinary draw submission,
2. deterministic compile/layout diagnostics,
3. immutable shader module / graphics-pipeline cache keyed by generation-safe Shader2D + resolved render state,
4. real custom fragment rendering through the frozen Sprite vertex/presentation ABI,
5. `SDL_PushGPUFragmentUniformData` using MAT1 packed blocks,
6. material/pipeline switch and uniform-upload metrics,
7. representative custom-fragment GPU tests and the required real-GPU tier handoff.

No arbitrary vertex deformation, public render graph, always-on extra pass or effect-specific C++ subsystem is authorized by MAT2.
