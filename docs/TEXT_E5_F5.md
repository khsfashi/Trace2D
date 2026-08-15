# Text E5 F5 — revision-cached semantic UI/localization bridge

Parent: #74  
Slice: #220

## Goal

F0-F4 established real font ownership, bounded glyph caching, deterministic UTF-8 layout/fallback, and production Sprite presentation. F5 closes the remaining runtime integration gap: unchanged resolved strings must not be laid out every frame, E3 committed/IME text must feed the same production Text path, and Agent inspection must see semantic text plus measured layout without treating glyph pixels as authority.

## Final authority boundary

The reusable source contract is intentionally localization-service agnostic:

```text
caller-owned semantic/localization source
  identity + revision + resolved UTF-8 bytes
        |
        v
TextLayoutCache
  -> revision/options/fallback-chain cache probe
  -> TextLayoutRun only on a miss
  -> F1 bounded GlyphAtlas resolution/raster only when needed
        |
        +-> deterministic TextLayoutMetrics
        |
        +-> F4 BuildTextPresentation2D
             -> existing SpritePresentationRenderData
             -> existing SR7 renderer/GPU path
```

`TextSourceView::identity` identifies a stable semantic source. `revision` is the invalidation contract: the caller must change it whenever the resolved UTF-8 display bytes change. A cache hit intentionally does **not** compare or scan `utf8`; that is what makes an unchanged text probe independent of string length.

A localization catalog, locale selector, plural-rule engine, message formatter, or key store is not introduced. Those systems can resolve a string and feed the exact same `TextSourceView` later without changing Text ownership.

## Cache and performance contract

`TextLayoutCache` owns one prepared `TextLayoutRun` plus a fixed-capacity fallback pointer identity vector. Preparation reserves the fallback identity storage and the underlying F2 layout capacities.

A hit requires equality of:

- source identity,
- source revision,
- all layout option fields,
- fallback chain length and exact atlas pointers.

The hit path is O(fallback count) and performs:

- no UTF-8 byte scan,
- no layout rebuild,
- no glyph lookup,
- no FreeType rasterization,
- no vector growth/allocation,
- no filesystem access,
- no ResourceRegistry lookup,
- no GPU work.

A miss reuses the transactional F2/F3 `TextLayoutRun`. A failed relayout does not partially publish a new Text layout key or glyph/line result. Cache reset invalidates only the key and retains prepared capacities.

## E3 UI / IME bridge

`UiTextLayoutCache` is a thin UI adapter above `TextLayoutCache`; Text does not depend on UI.

For `Label`, `Button`, and `TextInput`, the adapter maps the existing semantic element rectangle into the layout box in 26.6 pixels. The element remains authoritative for committed text.

For a focused `TextInput`, the E3 composition buffer is appended to committed text for **display layout only**:

```text
committed value:  "A"
IME composition:  "한"
display layout:   "A한"
Agent semantics:  text="A", composition="한"
```

This matches I3's current append-only editing baseline. F5 does not pretend that caret insertion/selection replacement already exists.

Each UI element receives an engine-owned stable text-source identity and monotonic display-text revision when added. The revision changes when the **displayed UTF-8 bytes** change. This includes ordinary committed-value edits, changed composition bytes, composition cancellation/focus loss, or a commit whose final bytes differ from the active preedit.

Two semantic-only transitions deliberately do not force layout:

- changing only IME selection/cursor metadata while composition bytes are identical,
- committing the exact active composition, e.g. visible `A + [한]` becoming committed `A한`.

In the second case the committed/composition semantic boundary changes but the visible UTF-8 bytes do not. UI layout evidence is invalidated because `includesComposition` changed, while the text revision is preserved. The next `UiTextLayoutCache::Update` therefore takes the existing Text cache hit and republishes the same measured layout with `includesComposition=false` without touching GlyphAtlas or rerasterizing.

Composition concatenation uses one cache-owned string reserved during `PrepareUiTextLayoutCache`. If committed + composition bytes exceed the declared bound, the update fails instead of growing storage implicitly.

## Semantic measured evidence

A successful UI layout publishes `UiTextLayoutEvidence` back to the existing `UiElement` runtime state:

- source revision,
- whether active composition was included,
- glyph count,
- line count,
- content width/height in 26.6 pixels,
- final layout width/height in 26.6 pixels.

The evidence is invalidated whenever displayed text changes and is accepted only when its source revision matches the element's current revision. It is also invalidated for display-preserving semantic transitions whose `includesComposition` meaning changed, then republished from the cached layout.

`AgentFacade::InspectUi` / UI query snapshots expose two optional semantic projections:

- active composition text and selection metadata for the focused textbox,
- current measured production-text layout evidence.

The committed `text` field remains unchanged. Glyph atlas rectangles, texture pixels, GPU handles, and raster output are deliberately not promoted to semantic authority. MCP JSON transport projection is not required by #74 and remains available for #75 to extend if its concrete Agent workflow needs these new optional fields.

## Presentation handoff

`UiTextLayoutCache::Layout()` exposes the successfully cached production `TextLayoutRun`. A host can pass that exact layout, the same ordered fallback atlas chain, and F4 atlas bindings to `BuildTextPresentation2D`. F5 therefore does not add a UI-only font renderer or another batching path.

## Deterministic acceptance evidence

The F5 tests cover:

- Korean/CJK resolved UTF-8 through a revision-keyed generic Text cache,
- exact cache-hit reuse with unchanged GlyphAtlas hit/miss/rasterization counters,
- explicit revision invalidation causing relayout while already-cached glyphs avoid rerasterization,
- bounded fallback-chain rejection without partial publication,
- focused E3 IME composition included in production UI layout while committed semantic text remains separate,
- selection-only composition metadata changes reusing the existing layout,
- exact composition commit preserving display revision and reusing the existing layout with zero GlyphAtlas metric change,
- element-bounds-to-layout-box mapping,
- bounded composition scratch failure without publishing stale evidence,
- Agent inspection of composition and measured layout.

## Explicit deferrals

F5 intentionally does not add:

- a localization catalog/service, locale storage, plural/message formatting policy,
- caret navigation, selection replacement, Backspace/Delete, clipboard, or rich text editing,
- UI hierarchy/anchors/resolution scaling/scroll widgets/event routing (owned by #75),
- HarfBuzz shaping, bidi, system font discovery, or language-specific word breaking without a concrete requirement,
- atlas eviction/growth, automatic partial GPU upload, or an R8-only text renderer,
- MCP-specific serialization of the new optional Agent UI evidence until a concrete #75 tool workflow requires it.

With F5 green, #74's production text foundation is complete and the fixed core lane advances to #75 practical deterministic UI.
