# UI and Semantic Automation

Trace2D UI is intentionally small, deterministic, engine-owned, and directly operable by coding agents without screen-coordinate targeting.

The authoritative flow is:

```text
text-authored TOML
  -> UiDocument
  -> deterministic engine-owned state
  -> AgentFacade semantic inspect/query/action/assert
  -> deterministic RGBA8 raster
  -> optional renderer texture upload
```

The GPU remains presentation only. Element identity, role, semantic name, bounds, visibility, enabled state, focus, text, and activation state live in `engine/ui` and are available without creating a window or renderer. `engine/agent` observes and mutates that same `UiDocument`; it does not maintain a second UI model.

## Module boundary

`engine/ui` is SDL-free and does not depend on `engine/render`, `engine/agent`, JSON, MCP, or an LLM protocol.

Current responsibilities:

- strict versioned TOML loading
- stable non-empty element IDs
- stable semantic names
- deterministic authored-order iteration
- integer pixel bounds
- visible/enabled/focused state
- panel, label, button, and text-input primitives
- button activation count
- focused text-input replacement
- deterministic CPU RGBA8 rasterization
- minimal built-in 5x7 ASCII-oriented label font

`engine/agent` depends on `engine/ui` and exposes protocol-independent semantic snapshots, selectors, actions, and assertions. MCP is intentionally deferred to Issue #39 and will adapt this existing surface rather than define it.

The separate `trace2d_ui_preview` tool remains a presentation adapter. In windowed mode it uploads the CPU raster through the existing renderer texture API and draws that texture as one sprite.

## Authored format

The current format version is `1`.

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
id = "title"
kind = "label"
bounds = [8, 8, 100, 16]
name = "Title"
text = "Trace2D UI"

[[elements]]
id = "start"
kind = "button"
bounds = [8, 32, 96, 24]
name = "Start Game"
text = "Start Game"

[[elements]]
id = "player_name"
kind = "text_input"
bounds = [8, 64, 120, 24]
name = "Player Name"
text = "Player"
```

The committed sample is `samples/ui/basic_ui.trace2d.toml`.

### Required fields

Top level:

- `format_version = 1`
- `[canvas].width`
- `[canvas].height`

Each `[[elements]]` table requires:

- `id`: stable, non-empty, unique identity
- `kind`: `panel`, `label`, `button`, or `text_input`
- `bounds`: `[x, y, width, height]`

Optional element fields:

- `name`: stable semantic name used by agents; defaults to the initial authored `text`
- `text`: current label/value text; defaults to empty
- `visible`: defaults to `true`
- `enabled`: defaults to `true`

The `name` fallback is resolved when the authored document loads. Later text-input mutation changes `text` but does not silently change semantic identity.

Unknown fields are rejected. Bounds must have positive width/height and remain inside the canvas.

V1 canvas dimensions are bounded to `1..4096` on each axis. The bound is an authored-input safety limit, not a recommended production resolution. A 4096x4096 RGBA8 raster is already 64 MiB before renderer resources.

## Semantic roles

Authored kinds map to stable Agent roles:

```text
panel      -> panel
label      -> label
button     -> button
text_input -> textbox
```

This vocabulary is intentionally smaller than a browser accessibility tree or DOM. V1 is a flat authored-order semantic tree rooted by the UI document itself; there is no speculative hierarchy, automatic layout tree, CSS model, or browser abstraction.

## Semantic selectors

`agent::UiSelector` can contain any non-empty combination of:

- `id`
- `role`
- `name`

Multiple fields are ANDed. A normal agent workflow can therefore use a stable selector equivalent to:

```text
role=button name="Start Game"
role=textbox name="Player Name"
id=start
```

`QueryUi` returns every match in authored order. `QueryOneUi` requires exactly one match and returns stable `no_match` or `ambiguous_match` diagnostics otherwise.

Coordinates are exposed as structured bounds for observation and assertions, but they are not needed to identify or activate a control when semantic identity exists.

## Semantic inspection

`AgentFacade::InspectUi()` returns the canvas and authored-order element snapshots. Each element exposes:

- stable `id`
- semantic `role`
- semantic `name`
- integer `bounds`
- `visible`
- `enabled`
- `focused`
- current `text`
- `activationCount`

Snapshots allocate only when an explicit inspection/query request asks for copied observation data. Ordinary UI state and raster paths do not build Agent snapshots.

## Semantic actions

The protocol-independent facade provides:

```text
FocusUi(selector)
ActivateUi(selector)
InputUiText(selector, text)
```

Each action first resolves exactly one semantic target, then mutates engine-owned `UiDocument` state.

Rules:

- invisible controls reject focus/activation/text input
- disabled controls reject focus/activation/text input
- only button/textbox controls are focusable
- only buttons are activatable in V1
- only textboxes accept text input
- text input requires the textbox to be focused first
- a successful text-input action changes `text`, not semantic `name`

Button activation and focus remain allocation-free. Text replacement is an explicit user/agent mutation and may resize the element's existing `std::string`; it is not part of a per-frame simulation hot path.

Stable action diagnostics include:

```text
ui_unavailable
invalid_selector
no_match
ambiguous_match
not_visible
disabled
not_focusable
not_activatable
not_text_input
not_focused
action_rejected
```

## Semantic assertions

`AgentFacade::AssertUi(selector, expected)` performs an exact single-target query and may verify any combination of:

- `visible`
- `enabled`
- `focused`
- `text`
- `activationCount`

A mismatch returns `state_mismatch` with expected and observed context. This gives coding agents a structured correctness oracle without screenshot inference.

## Determinism and complexity

UI iteration and all semantic multi-query output preserve authored order. No hash-container iteration order participates in observable output.

Current element lookup/query/action target resolution is a deterministic O(N) scan. This is deliberate: the first UI surface is small, predictable, and allocation-light. An index should be introduced only when measured authored UI sizes demonstrate that O(N) lookup is a meaningful cost.

The semantic query path is explicit tooling work and may allocate result snapshots. Focus and activation themselves add no heap allocation. No semantic snapshot, JSON object, fingerprint, or MCP payload is produced during ordinary rasterization or simulation.

Bounds use unsigned integer canvas pixels. The same loaded document therefore produces the same element rectangles independent of renderer resolution, GPU vendor, locale, or wall-clock timing.

## Rasterization and visibility

`RasterizeUi` writes into a caller-owned `UiRasterImage`.

When width and height are unchanged, the existing `std::vector<std::uint8_t>` storage is reused. The raster path does not allocate temporary element or glyph collections.

Invisible elements are skipped entirely by rasterization. The same `visible` state exposed to Agent inspection therefore controls presentation rather than existing as automation-only metadata.

Current complexity is proportional to the canvas pixels cleared plus visible primitive/glyph work:

```text
O(canvas_pixels + visible_filled_primitive_pixels + visible_glyph_pixels)
```

The preview adapter performs one texture upload when the authored UI is loaded, then reuses the resulting renderer texture for subsequent preview frames. This first deterministic CPU rasterizer is not claimed as the final production UI renderer; later optimization should follow measured workloads.

## Built-in text scope

The built-in font is dependency-free and deterministic. It is a fixed 5x7 bitmap glyph set covering ASCII letters, digits, space, and a small punctuation subset. Lowercase ASCII renders through the corresponding uppercase glyph.

Not implemented:

- Unicode shaping
- CJK text
- kerning
- font fallback
- variable font sizing
- authored font assets
- rich text
- localization layout

Those capabilities should be introduced only when practical authored content requires them, behind the same engine-owned semantic contract.

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

## Deliberate non-goals

The current UI/semantic automation surface does not introduce:

- a broad widget framework
- nested automatic layout
- anchors/flex/grid systems
- style sheets or themes
- an editor
- coordinate-only automation
- a UI-specific GPU renderer
- a DOM clone or browser abstraction
- JSON/MCP types inside engine modules
- a font cache system without a real authored-font requirement

Issue #39 may now expose this completed semantic vocabulary through MCP without redesigning UI ownership or automation semantics.
