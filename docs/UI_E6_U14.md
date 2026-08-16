# E6 U14: Authored Progress / Image convergence

Issue: #252  
Parent: #75

U14 closes the authoring gap intentionally left by U12 Progress and U13 Image. Version-1 UI TOML may now declare practical widgets while runtime layout, interaction, inspection and raster authority remain unchanged.

## Authored surface

Progress:

```toml
[[elements]]
id = "health"
kind = "progress"
bounds = [16, 16, 160, 12]
progress_value = 75
progress_maximum = 100
```

Image:

```toml
[[elements]]
id = "portrait"
kind = "image"
bounds = [16, 40, 64, 64]
texture = "ui/portrait.rgba"
```

`progress_value` is a non-negative 32-bit integer. `progress_maximum` is positive and the existing U12 invariant `value <= maximum` remains authoritative. `texture` is a non-empty project-relative #86 texture reference.

The public runtime `UiElementKind` intentionally does **not** gain Progress or Image entries. Both authored kinds resolve through the existing Panel geometry/hierarchy path and are specialized only after the local `UiDocument` is complete:

- `kind = "progress"` -> Panel -> `UiDocument::ConfigureProgress`,
- `kind = "image"` -> Panel -> `UiDocument::ConfigureImage`.

This keeps U0-U11 hierarchy/layout/pointer/focus/clipping/scroll authority single-sourced and lets U12/U13 Agent/raster behavior remain the only practical-widget runtime authority.

## Resource-aware Image load

Image authoring uses:

```cpp
LoadUiToml(text, resources, sourceName)
```

The resource-free overload continues to load all legacy kinds plus Progress. An authored Image passed to the resource-free overload fails deterministically with a diagnostic; it never attempts filesystem discovery or implicit loading.

`ResourceRegistry::FindReadyTexture(reference)` is the bounded U14 bridge from authored project-relative identity to U13's generation-safe handle. It:

1. runs the same #86 canonicalization used by publication,
2. performs the same `identityToSlot_` lookup,
3. accepts only the current Ready Texture slot/payload,
4. returns the current generation-safe handle,
5. never publishes, loads, scans `InspectAll()`, copies decoded bytes, or creates a UI-specific cache.

Canonical-equivalent references therefore converge on the same ready handle.

## Transaction boundary

Parsing/layout first builds a local document exactly as before. Authored scroll configuration still runs through U11. U14 then applies practical-widget specialization to that local document. Any invalid Progress range, Image reference, missing registry, non-ready texture, or U12/U13 configuration failure leaves `UiLoadResult::document` empty.

Widget-only fields are rejected on incompatible kinds. `scroll_content_size` remains valid only on authored `kind = "panel"`; Progress and Image cannot also claim scroll viewport authority.

## Performance boundary

All new work is explicit authored load/setup work:

- one parse-time bounded field validation per authored widget,
- Progress: existing semantic-ID configuration lookup once,
- Image: #86 canonicalization + expected O(1) identity-map lookup + existing semantic-ID configuration lookup once.

No ordinary frame gains resource-path lookup, filesystem work, parser work, hierarchy discovery, duplicate texture ownership, upload/readback, or per-frame allocation. Once configured, Progress/Image use the unchanged retained U12/U13 state and raster/Agent paths.
