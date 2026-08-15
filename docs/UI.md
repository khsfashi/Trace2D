# UI and Semantic Automation

Trace2D UI is intentionally small, deterministic, engine-owned, and directly operable by coding agents without screen-coordinate targeting.

The authoritative authored flow is:

```text
text-authored TOML
  -> setup-only UiLayoutTree compilation
  -> UiDocument resolved runtime state
  -> deterministic engine-owned interaction/text state
  -> AgentFacade semantic inspect/query/action/assert
  -> deterministic RGBA8 raster
  -> optional renderer texture upload
```

The GPU remains presentation only. Element identity, semantic hierarchy, role, name, resolved geometry, visibility, enabled state, focus, text, and activation state live in `engine/ui` and are available without creating a window or renderer. `engine/agent` observes and mutates that same `UiDocument`; it does not maintain a second UI model.

Detailed E6 layout contracts:

- `docs/UI_E6_U0.md` — deterministic hierarchy foundation,
- `docs/UI_E6_U1.md` — deterministic fixed anchor/pivot placement,
- `docs/UI_E6_U2.md` — authored TOML -> runtime hierarchy/layout bridge.

## Module boundary

`engine/ui` is SDL-free and does not depend on `engine/render`, `engine/agent`, JSON, MCP, or an LLM protocol.

Current responsibilities include:

- strict versioned TOML loading,
- stable non-empty element IDs,
- deterministic authored-order iteration,
- setup-time stable parent resolution and hierarchy validation,
- parent-local absolute placement,
- exact integer fixed anchor/pivot placement,
- retained direct parent index, hierarchy depth, local bounds, and absolute logical-canvas bounds,
- visible/enabled/focused state,
- panel, label, button, and text-input primitives,
- button activation count,
- focused text-input mutation/composition,
- deterministic CPU RGBA8 rasterization,
- semantic Agent inspection/query/action/assertion.

`engine/agent` depends on `engine/ui` and exposes protocol-independent semantic snapshots, selectors, actions, and assertions. Protocol adapters consume this existing surface rather than defining UI ownership.

The separate `trace2d_ui_preview` tool remains a presentation adapter. In windowed mode it uploads the CPU raster through the existing renderer texture API and draws that texture as one sprite.

## Authored format

The current format version remains `1`.

### Legacy/root absolute placement

Existing documents remain source-compatible:

```toml
format_version = 1

[canvas]
width = 160
height = 96

[[elements]]
id = "root"
kind = "panel"
bounds = [0, 0, 160, 96]
name = "Main Menu"

[[elements]]
id = "start"
kind = "button"
bounds = [8, 32, 96, 24]
name = "Start Game"
text = "Start Game"
```

With no `parent` and no `placement`, `bounds = [x, y, width, height]` is a root rectangle in logical-canvas coordinates.

### Parent-local absolute placement

A child names its stable parent and authors `bounds` relative to that immediate parent:

```toml
[[elements]]
id = "label"
kind = "label"
parent = "panel"
bounds = [4, 5, 40, 12]
text = "Ready"
```

Children may be declared before parents. The load compiler resolves IDs independently of author order while preserving the original element iteration order in `UiDocument`.

### Fixed anchored placement

U1 placement is exposed explicitly through the authored path:

```toml
[[elements]]
id = "confirm"
kind = "button"
parent = "panel"
placement = "anchored_fixed"
anchor = [1024, 1024]
pivot = [1024, 1024]
offset = [-8, -6]
size = [64, 24]
name = "Confirm"
text = "OK"
```

The normalized anchor/pivot domain is exactly `0..1024` per axis:

- `0` = leading edge,
- `512` = center,
- `1024` = trailing edge.

`offset` is signed logical pixels. `size` is a positive logical-pixel pair. `anchored_fixed` must not also provide `bounds`; absolute placement requires `bounds` and does not accept anchor/pivot/offset/size fields.

All anchor/pivot arithmetic and containment validation remain owned by `UiLayoutTree`; the TOML loader does not define a second implementation.

### Common fields

Each `[[elements]]` table always requires:

- `id`: stable, non-empty, unique identity,
- `kind`: `panel`, `label`, `button`, or `text_input`.

Optional common fields:

- `parent`: stable non-empty parent ID,
- `placement`: omitted/`absolute`, or `anchored_fixed`,
- `name`: stable semantic name used by agents; defaults to initial authored `text`,
- `text`: current label/value text; defaults to empty,
- `visible`: defaults to `true`,
- `enabled`: defaults to `true`.

The `name` fallback is resolved when the authored document loads. Later text-input mutation changes `text` but does not silently change semantic identity.

Unknown fields are rejected. V1 canvas dimensions are bounded to `1..4096` on each axis.

## Setup compilation and runtime state

`LoadUiToml()` stages syntactically valid elements, validates duplicate IDs, then feeds the existing U0/U1 `UiLayoutTree`. Only after `Finalize()` succeeds are elements published into `UiDocument`.

Each runtime `UiElement` retains:

- semantic `parentId`,
- direct `parentIndex` into the same authored-order `UiDocument::Elements()` storage, or `InvalidUiElementIndex` for roots,
- hierarchy `depth`,
- resolved parent-local `localBounds`,
- absolute logical-canvas `bounds`.

The temporary layout tree is discarded after load. Runtime actions, rasterization, later input routing, and Agent inspection consume the already-resolved `UiDocument`; there is no second hierarchy to synchronize per frame.

## Semantic roles

Authored kinds map to stable Agent roles:

```text
panel      -> panel
label      -> label
button     -> button
text_input -> textbox
```

This vocabulary is intentionally smaller than a browser accessibility tree or DOM. Semantic identity remains independent of screen coordinates.

## Semantic selectors

`agent::UiSelector` can contain any non-empty combination of:

- `id`,
- `role`,
- `name`.

Multiple fields are ANDed. `QueryUi` returns every match in authored order. `QueryOneUi` requires exactly one match and returns stable `no_match` or `ambiguous_match` diagnostics otherwise.

## Semantic inspection

`AgentFacade::InspectUi()` and query/action responses expose the same engine-owned runtime truth. Each element snapshot includes:

- stable `id`, role, and semantic `name`,
- optional `parentId`,
- hierarchy `depth`,
- resolved parent-local `localBounds`,
- absolute logical-canvas `bounds`,
- `visible`, `enabled`, `focused`,
- current `text`,
- `activationCount`,
- text composition/layout evidence when applicable.

Snapshots allocate only when an explicit inspection/query request asks for copied observation data. Ordinary UI state and raster paths do not build Agent snapshots.

## Semantic actions

The protocol-independent facade provides:

```text
FocusUi(selector)
ActivateUi(selector)
InputUiText(selector, text)
```

Rules remain deterministic:

- invisible controls reject focus/activation/text input,
- disabled controls reject focus/activation/text input,
- only button/textbox controls are focusable,
- only buttons are activatable,
- only textboxes accept text input,
- text input requires the textbox to be focused first,
- successful text input changes `text`, not semantic `name`.

For buttons, `activationCount` is the engine-owned deterministic edge counter. Gameplay may retain the last consumed count and process the positive delta during deterministic update without introducing a callback graph or heap event queue.

## Determinism and complexity

Observable UI iteration remains authored order. Hash-container iteration order does not participate in runtime or Agent output.

Authored load/setup for `N` valid elements performs bounded staging plus `O(N log N)` duplicate/parent lookup preparation. Publication currently preserves the original duplicate-protected `UiDocument::AddElement` contract, whose repeated linear ID guard makes publication `O(N^2)` in the worst case. This is explicit load/setup work, not a frame loop; a bulk publication/index path should be added only if measured practical authored UI sizes make that setup cost material.

After publication:

- parent traversal can use direct indices,
- hierarchy discovery is not repeated,
- anchor/pivot math is not repeated,
- no TOML parsing, filesystem access, sorting, JSON/reporting, or Agent snapshot generation happens implicitly,
- logical UI geometry remains independent of OS/window presentation size and follows the #88 viewport mapping contract.

Current semantic ID lookup/query/action target resolution in `UiDocument` remains a deterministic O(N) scan. An additional runtime index should be introduced only when measured practical UI sizes show that scan cost is meaningful.

## Rasterization and visibility

`RasterizeUi` consumes each element's resolved absolute `bounds` and writes into a caller-owned `UiRasterImage`.

When width and height are unchanged, existing output storage is reused. Invisible elements are skipped entirely. Current work is proportional to canvas clear plus visible primitive/glyph pixels:

```text
O(canvas_pixels + visible_filled_primitive_pixels + visible_glyph_pixels)
```

The preview adapter performs one texture upload when authored UI is loaded, then reuses the renderer texture for subsequent preview frames. This CPU rasterizer remains presentation plumbing rather than a second UI state model.

## Preview commands

Headless validation:

```powershell
build\windows-debug\tools\ui_preview\Debug\trace2d_ui_preview.exe `
    --headless `
    --ui samples/ui/basic_ui.trace2d.toml `
    --json
```

Windowed preview:

```powershell
build\windows-debug\tools\ui_preview\Debug\trace2d_ui_preview.exe `
    --windowed `
    --ui samples/ui/basic_ui.trace2d.toml `
    --frames 600
```

Headless semantic interaction is covered directly through `AgentFacade` tests over the same `UiDocument` state and requires no renderer initialization.

## Deliberate non-goals for U2

U2 does not introduce:

- stretch anchors without demonstrated need,
- horizontal/vertical stack, flex, or grid layout,
- margin/padding policy,
- pointer hit routing/capture,
- focus traversal/navigation,
- clipping/scroll,
- broad widget/style/theme frameworks,
- an editor,
- coordinate-only automation,
- a UI-specific retained GPU renderer,
- a DOM/browser abstraction,
- a generic callback/event framework without a demonstrated payload requirement.

Those are later bounded #75 slices and must build on this single resolved runtime state rather than bypass it.
