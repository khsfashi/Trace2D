# E6 U0 — deterministic UI hierarchy foundation

Parent: #75

U0 establishes the protocol-independent hierarchy/layout authority that later E6 slices will connect to authored UI, anchors, stacks, pointer routing, clipping, navigation and widgets.

## Authority

`UiLayoutTree` owns setup-time semantic hierarchy resolution.

```text
stable node id + optional parent id + parent-local rect
  -> explicit Finalize()
  -> resolved parent index + depth + absolute rect
```

Parent IDs are authoring/setup identity only. Finalization builds one sorted index and resolves each non-root parent with binary search. Finalized nodes retain direct parent indices; ordinary frame code does not need to rediscover hierarchy or resolve semantic strings.

Root rectangles are expressed in logical UI-canvas coordinates. Child rectangles are expressed in the immediate parent's local coordinate space. Finalization rejects roots outside the logical canvas and children that escape their parent rectangle.

The existing E3 `UiDocument` flat TOML contract is intentionally unchanged by U0. A later E6 slice will adapt authored hierarchy/layout data into this authority rather than adding hierarchy rules directly to rendering or input.

## Deterministic rejection

Finalization rejects:

- invalid logical canvas size,
- empty node IDs,
- excessive node count,
- zero-area rectangles,
- duplicate IDs,
- unknown parents,
- self-parenting,
- hierarchy cycles,
- root rectangles outside the canvas,
- child rectangles outside their parent.

Containment uses subtraction after origin checks rather than unchecked `x + width` / `y + height`, so malformed large unsigned coordinates cannot wrap into accepted geometry.

A failed tree never reports itself as finalized.

## Performance boundary

U0 is setup work.

- semantic parent lookup is prepared once during `Finalize()`,
- lookup, visit-state and chain scratch storage are retained by the layout object,
- resolved nodes are contiguous and expose direct parent indices,
- no filesystem, text layout/rasterization, GPU work, JSON/reporting or frame-time hierarchy discovery is added,
- explicit `Find(id)` remains an inspection/setup convenience; hot runtime paths can use resolved indices/spans directly.

## Deferred to later #75 slices

U0 does not close #75. Remaining work includes:

- authored `parent` integration,
- anchors/pivot and #88 logical-resolution scaling rules,
- horizontal/vertical stack layout plus margin/padding,
- focus traversal/navigation,
- pointer hit testing, hover/press/release, capture and event routing,
- clipping/scroll behavior,
- image/progress and other demonstrated practical widgets,
- semantic Agent inspection of the completed authored layout path.
