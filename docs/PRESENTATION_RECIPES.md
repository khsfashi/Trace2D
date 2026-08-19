# Presentation Recipes

## Purpose

Trace2D separates **engine expression primitives** from **official reusable presentation techniques**.

The engine core should expose compact, performant capabilities such as Material2D/Shader2D, Tween/Sequence, Particle, UI, camera and resource bindings. Finished visual techniques should normally live above those primitives as reusable recipes rather than becoming one-off C++ subsystems.

This keeps the engine small while giving Agents a stable vocabulary for common game-feel work.

## Product layers

```text
Engine Core Primitives
  Material2D / Shader2D
  Tween / Sequence
  Particle
  UI / Camera
  resource / blend / sampler bindings

Official Presentation Recipes
  shader recipes
  UI-motion recipes
  composite game-feel recipes

Game-specific Presentation
  project-owned art direction and bespoke effects
```

## Initial recipe vocabulary

Representative recipes include:

### Shader/material

- `hit_flash`
- `outline`
- `dissolve`
- `grayscale`
- `palette_swap`
- `uv_scroll`
- `water_wave`
- `pixelate`
- `vignette`

### UI/motion

- `button_punch`
- `panel_slide`
- `tooltip_reveal`
- `damage_number`
- `health_bar_ease`
- `screen_flash`

### Composite

- `hit_impact`
- `screen_transition`
- `boss_intro`

This list is a discovery target, not authorization to implement every item immediately.

## Recipe contract

An official recipe should be discoverable by humans and Agents through stable identity and bounded typed parameters. It should declare at least:

- stable recipe ID,
- required engine capabilities,
- typed parameters and defaults,
- authoritative vs presentation-only behavior,
- deterministic/manual-step expectations where applicable,
- resource dependencies,
- representative performance/evidence notes,
- a small executable/sample proof.

A recipe is not gameplay truth. Presentation callbacks or completion must not silently own gameplay state.

## Promotion rule — recipe before core

When a tutorial, video, external project or game workload suggests a useful visual technique:

1. benchmark/understand the proven technique and license boundary;
2. attempt it first using existing Trace2D primitives as a recipe;
3. retain a real sample and performance/complexity evidence;
4. promote only the **smallest common missing primitive** into engine core if multiple independent recipes repeatedly need it or measured performance/quality cannot be achieved otherwise.

Do not add `one cool effect = one engine subsystem`.

A single project-specific boss distortion, HUD transition or art-direction shader remains game-owned unless a cross-project need is demonstrated.

## External reference / licensing rule

Techniques may be reproduced from public tutorials and mature engines after current reference review. Do not copy source code, shader code, textures or other assets unless their exact license permits reuse and required attribution/provenance is recorded.

## Agent-first value

Recipes are part of Trace2D's Agent authoring vocabulary. A request such as:

```text
"독 공격 피격 시 보라색으로 0.08초 점멸하고 살짝 흔들어줘"
```

should normally resolve to a known combination of recipes/typed parameters rather than cause the Agent to invent new renderer or gameplay code.

The goal is lower context cost, more predictable quality and easier deterministic/visual verification.

## Scope discipline

The recipe library must grow from real product proofs. Do not create a large speculative catalog before #89 Material2D/Shader2D and #90 Tween/Sequence have been exercised in the bounded Presentation Product Proof.
