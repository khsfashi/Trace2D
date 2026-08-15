# E6 U2 — authored UI hierarchy/runtime layout bridge

Parent: #75  
Child: #227

U2 connects the already-merged U0/U1 deterministic `UiLayoutTree` authority to the ordinary authored `LoadUiToml()` path. The layout tree is setup-time compiler state only; the published `UiDocument` remains the single runtime UI model used by semantic actions, text, rasterization, Agent inspection, and later pointer work.

## Source compatibility

The UI format remains `format_version = 1`.

Existing flat elements are unchanged:

```toml
[[elements]]
id = "start"
kind = "button"
bounds = [8, 32, 96, 24]
```

With no `parent` and no `placement`, `bounds` is the root rectangle in logical-canvas coordinates. Its resolved `localBounds` and absolute `bounds` are identical.

## Parent-local absolute placement

An element may name a stable semantic parent:

```toml
[[elements]]
id = "label"
kind = "label"
parent = "panel"
bounds = [4, 5, 40, 12]
```

For a child, the same `bounds` field is interpreted as parent-local absolute geometry. U0 resolves the parent by stable ID, validates the hierarchy, and publishes the final absolute logical-canvas rectangle.

Children may appear before parents in authored order. Runtime element order remains authored order; the resolved `parentIndex` directly indexes that same `UiDocument::Elements()` storage.

## Fixed anchored placement

An element selects the U1 anchor/pivot authority explicitly:

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
```

`anchor` and `pivot` use the exact U1 normalized integer domain `0..1024`. `offset` is a signed 32-bit logical-pixel pair and `size` is a positive unsigned logical-pixel pair.

`anchored_fixed` must not also author `bounds`. Conversely, absolute placement requires `bounds` and does not accept `anchor`, `pivot`, `offset`, or `size`.

All anchor/pivot math, round-half-up behavior, containment rules, and overflow rejection remain owned by `UiLayoutTree`; `LoadUiToml()` does not duplicate that arithmetic.

## Runtime publication

Successful load performs:

```text
strict TOML parse
  -> bounded authored element staging
  -> duplicate-ID validation
  -> UiLayoutTree::AddNode / Finalize
  -> resolved hierarchy + geometry publication
  -> UiDocument
```

Each runtime `UiElement` retains:

- stable `parentId`,
- direct `parentIndex` or `InvalidUiElementIndex` for roots,
- hierarchy `depth`,
- resolved parent-local `localBounds`,
- absolute logical-canvas `bounds`.

The temporary `UiLayoutTree` is discarded after load. There is no second retained UI hierarchy and no per-frame synchronization step.

## Agent inspection

`UiElementSnapshot` exposes the same resolved hierarchy without coordinate inference:

- optional semantic `parentId`,
- `depth`,
- resolved `localBounds`,
- absolute `bounds`.

Queries/actions still target stable ID/role/name. Hierarchy geometry is observation evidence, not a new coordinate-only automation path.

## Failure contract

Parsing remains strict and does not publish a partial document.

U2 rejects:

- empty authored parent IDs,
- unsupported placement names,
- incompatible absolute/anchored field combinations,
- malformed or out-of-range normalized anchor/pivot values,
- signed offset overflow,
- zero/invalid anchored sizes,
- duplicate IDs,
- unknown/self/cyclic parents,
- root geometry escaping the logical canvas,
- child geometry escaping its immediate parent.

Syntactic/type failures retain field-oriented diagnostic paths. Cross-node hierarchy/finalization failures use the bounded `layout` diagnostic path and the stable `UiLayoutResult` name.

## Complexity and allocation boundary

U2 work happens only during explicit authored UI load/setup.

For `N` syntactically valid elements:

- duplicate validation sorts an index vector: `O(N log N)`,
- U0 parent lookup/finalization uses its existing sorted lookup and direct resolved indices,
- publication is `O(N)` and preserves authored order.

Temporary vectors reserve from the authored element count. Normal frame code performs no TOML parsing, parent string lookup, hierarchy discovery, anchor math, sorting, filesystem work, Agent snapshot creation, JSON work, or layout-tree rebuild.

Raster/input paths consume the already-resolved absolute rectangle. Hierarchy traversal, when later required, can use the retained direct index rather than semantic string lookup.

## Deliberate deferrals

U2 does not close #75. Later bounded slices still own:

- stretch anchors if representative UI demonstrates the need,
- horizontal/vertical stack layout, margin, and padding,
- deterministic pointer hit testing/routing/capture,
- focus traversal/navigation,
- clipping and scroll,
- image/progress and practical production widgets.
