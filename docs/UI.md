# Basic UI

Trace2D's first UI slice is intentionally small. Its job is to establish deterministic, engine-owned UI state before semantic automation is added in Issue #43.

The authoritative flow is:

```text
text-authored TOML
  -> UiDocument
  -> deterministic bounds/state
  -> headless inspection/actions
  -> deterministic RGBA8 raster
  -> optional renderer texture upload
```

The GPU is presentation only. Focus, enabled state, activation counts, element identity, authored text, and bounds live in `engine/ui` and are available without creating a window or renderer.

## Module boundary

`engine/ui` is SDL-free and does not depend on `engine/render`.

Current responsibilities:

- strict versioned TOML loading
- stable non-empty element IDs
- deterministic authored-order iteration
- integer pixel bounds
- panel, label, button, and text-input primitives
- focus state for button/text-input elements
- button activation count
- deterministic CPU RGBA8 rasterization
- minimal built-in 5x7 ASCII-oriented label font

The separate `trace2d_ui_preview` tool is an adapter. In windowed mode it uploads the CPU raster through the existing renderer texture API and draws that texture as one sprite. That adapter does not become UI state ownership.

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

[[elements]]
id = "title"
kind = "label"
bounds = [8, 8, 100, 16]
text = "Trace2D UI"

[[elements]]
id = "start"
kind = "button"
bounds = [8, 32, 96, 24]
text = "Start Game"

[[elements]]
id = "player_name"
kind = "text_input"
bounds = [8, 64, 120, 24]
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

- `text`: defaults to empty
- `enabled`: defaults to `true`

Unknown fields are rejected. Bounds must have positive width/height and remain inside the canvas.

V1 canvas dimensions are bounded to `1..4096` on each axis. The bound is an authored-input safety limit, not a recommended production resolution: it prevents malformed or LLM-authored data from turning one preview/raster request into an unbounded allocation. A 4096x4096 RGBA8 raster is already 64 MiB before renderer resources, so practical UI should remain much smaller.

## Determinism

UI iteration preserves authored element order. No hash-container iteration order participates in observable output.

Bounds use unsigned integer canvas pixels rather than floating-point layout. The same loaded document therefore produces the same element rectangles independent of renderer resolution, GPU vendor, locale, or wall-clock timing.

The CPU rasterizer walks elements in authored order and writes a fixed RGBA8 result. Repeated rasterization of an unchanged document and state produces byte-identical pixels.

## State and actions

The narrow state surface is designed to become the base of semantic UI in #43.

Current headless operations include:

- find an element by stable ID
- inspect authored kind, text, enabled state, bounds, and activation count
- focus a button or text input
- clear focus
- activate a button

Failures return stable `UiActionResult` values such as:

```text
not_found
not_focusable
not_activatable
disabled
```

Focus and activation perform no heap allocation. Element lookup is currently a deterministic O(N) scan because the first UI slice is intentionally small; an index should only be added after real authored UI sizes justify it.

## Rasterization and performance

`RasterizeUi` writes into a caller-owned `UiRasterImage`.

When width and height are unchanged, the existing `std::vector<std::uint8_t>` storage is reused. The raster path does not allocate temporary element or glyph collections.

Current complexity is proportional to the canvas pixels cleared plus the pixels covered by authored primitives and glyphs:

```text
O(canvas_pixels + filled_primitive_pixels + glyph_pixels)
```

The 4096-per-axis V1 limit bounds the raw CPU raster to 64 MiB. It exists to make worst-case authored allocation finite; it is not a performance target or permission to rasterize 64 MiB every frame.

The preview adapter performs one texture upload when the authored UI is loaded, then reuses the resulting renderer texture for subsequent preview frames. It does not rebuild UI data or discover assets every frame.

This is appropriate for the first deterministic UI foundation, not a claim that whole-canvas CPU rasterization is the final production UI renderer. Later optimization should be driven by renderer/UI workloads after the semantic surface is stable.

## Built-in text scope

The first font is deliberately dependency-free and deterministic. It is a fixed 5x7 bitmap glyph set covering ASCII letters, digits, space, and a small punctuation subset. Lowercase ASCII is rendered using the corresponding uppercase glyph.

This is enough for the first machine-verifiable labels and controls, but it is not presented as a complete production typography system.

Not implemented in this slice:

- Unicode shaping
- CJK text
- kerning
- font fallback
- variable font sizing
- authored font assets
- rich text
- localization layout

Those capabilities must not be added merely as speculative breadth. When practical game content requires them, font assets and shaping should be introduced behind the same engine-owned UI/text contract without making the renderer authoritative.

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

The exact executable output directory can vary by generator/configuration. The important contract is that headless and windowed modes load and rasterize the same authored file; windowed mode only adds the existing renderer presentation step.

## Deliberate non-goals

Issue #42 does not introduce:

- a broad widget framework
- nested automatic layout
- anchors/flex/grid systems
- style sheets or themes
- an editor
- coordinate-only automation
- a UI-specific GPU renderer
- a font cache system without a real authored-font requirement

Issue #43 should build semantic identity, role/name/state querying, and semantic actions on top of this module rather than replacing it.
