# Sprite SR0 Render Contract

Status: **implemented by #123 / SR0**  
Umbrella: #59  
Predecessors: S0 architecture + S1 canonical asset/import  
Next Sprite child after SR0: **SR1 — transform/geometry semantics and fixed-step presentation history**

SR0 defines the first backend-independent seam between canonical `SpriteAsset` CPU truth and future renderer/backend resources.

## 1. Authority direction

```text
canonical SpriteAsset CPU truth
        -> setup-time region resolution
        -> immutable pre-resolved Sprite selection
        -> O(1) backend-independent render-contract extraction
        -> later geometry/UV/presentation derivation
        -> renderer/backend resources
```

The direction is one-way. Derived render state never becomes authored/runtime authority.

Canonical S1 data therefore continues to exclude:

- normalized UV coordinates,
- SDL/GPU texture handles,
- descriptors or upload offsets,
- frame-local batch IDs,
- backend-specific texture/pipeline/sampler enums,
- renderer residency state.

## 2. Existing view seam is reused

SR0 does not create another camera abstraction.

`SpriteResolvedView` is an alias of the already backend-independent `OrthographicView`. Future #88 `Camera2D` / `Viewport2D` resolves into the view seam instead of replacing Sprite authored/runtime data.

SR1/SR6 may refine how Sprite presentation consumes the resolved view, but SR0 does not implement Camera2D/Viewport2D.

## 3. Setup resolution vs steady-state extraction

Two workloads are deliberately separated.

### Setup/tooling resolution

`ResolveSpriteRegionByIndices(...)` validates an explicit page/region pair. It performs bounds checks and the semantic page-ID relationship check once before steady-state rendering.

`ResolveSpriteRegionById(...)` is an optional semantic setup helper. It linearly scans regions/pages and detects duplicate semantic IDs. Its complexity is:

```text
O(region_count + page_count)
```

It performs no heap allocation but is **not a per-draw API**.

### Steady-state extraction

`ExtractSpriteRenderContract(...)` consumes only a validated `ResolvedSpriteRegion`.

Its success path is:

```text
O(1)
```

and requires no:

- semantic ID search/hash,
- path normalization,
- filesystem access,
- TOML parsing,
- image decode,
- formatted diagnostic strings,
- renderer/GPU initialization,
- heap allocation.

The caller owns/reuses the output record.

## 4. Pre-resolved canonical selection

`ResolvedSpriteRegion` retains non-owning references into an externally owned immutable `SpriteAsset` plus stable page/region indices.

The caller must keep the owning canonical asset alive while the selection is used. SR0 intentionally does not create the future #86 generation-safe resource/handle system.

A resolved selection contains:

- canonical asset pointer,
- exact selected page pointer/index,
- exact selected region pointer/index,
- derived CPU page resource key,
- finite renderer compatibility values.

No full Sprite asset/page/region is copied into a frame-owned object.

## 5. CPU page resource key

`SpritePageResourceKey` contains only immutable CPU identity/intent already proven by S1:

```text
project-relative texture reference
exact page pixel size
color-space intent
alpha intent
```

The texture reference is a `std::string_view` into canonical asset-owned storage. It is not copied/allocated during extraction.

A later renderer cache may map this key to a backend texture, but that cache/handle is derived state and is not part of SR0 or `SpriteAsset`.

## 6. Finite compatibility seam

SR0 introduces typed compatibility values so future batching does not collapse into `texture == texture`.

Current supported values are intentionally narrow:

```text
material/pipeline = built_in_sprite
sampler           = nearest | linear
blend             = straight_alpha
mask              = none
primitive         = quad
```

These are **compatibility identities**, not claims that all future features are implemented.

Handoffs:

- SR3 extends color/alpha/blend/sampling behavior,
- SR4 extends mask/order compatibility,
- SR5 may add primitive kinds for 9-slice/tiled presentation,
- SR7 uses resolved compatibility to form contiguous batch runs,
- #89 extends material/pipeline resolution without changing canonical S1 assets.

Material compatibility never permits global painter-order sorting.

## 7. Canonical metadata forwarding

SR0 does not calculate final vertices or normalized UVs.

The extraction result references the exact selected S1 page/region so later stages consume canonical:

- source size,
- trim offset/size,
- packed rectangle,
- rational pivot,
- packed rotation,
- page size/color/alpha intent.

Ownership remains:

- SR1 — transform/geometry and fixed-step presentation history,
- SR2 — trim/pivot/atlas/rotated-storage geometry and UV derivation,
- SR3 — color/alpha/blend/sampling,
- SR4 — order/groups/masks.

## 8. Structured failures

Setup resolution returns allocation-free structured status values composed of:

```text
SpriteResolveError
SpriteResolveField
```

Covered failures include:

- null asset,
- page index out of range,
- region index out of range,
- selected page/region semantic mismatch,
- empty canonical page/region identity,
- invalid page texture identity or zero page size,
- unsupported color-space/alpha/sampling enum values in manually constructed invalid CPU data,
- missing/duplicate semantic region/page IDs,
- unresolved steady-state selection.

S1 remains responsible for authored schema diagnostics. SR0 rechecks only the minimum invariants required to protect the renderer seam when CPU objects are manually constructed or corrupted.

## 9. Ownership and performance

SR0 adds no GC, background loader, generic resource graph, reflection/property bag, or frame-owned Sprite list.

The steady-state record is trivially copyable and composed of pointers, indices, `std::string_view`, exact small S1 metadata, and finite enums. It can be written repeatedly into caller-owned storage.

Backend texture/pipeline/sampler objects must later be cached/reused rather than recreated per frame, but SR0 deliberately stops before backend resource creation.

## 10. Verification boundary

SR0 tests are entirely backend-independent and require no SDL window or GPU.

They prove:

- canonical asset/page/region pointer identity is preserved,
- exact S1 metadata is forwarded without normalization/mutation,
- finite compatibility defaults are stable,
- sampling intent maps deterministically,
- invalid setup selections fail structurally,
- semantic-ID resolution is deterministic setup work,
- repeated extraction reuses caller-owned output,
- `SpriteAsset` has no texture-handle or normalized-UV member,
- the resolved view seam reuses `OrthographicView`.

## 11. SR1 handoff

After SR0 merges green, SR1 owns complete transform/geometry semantics and the fixed-step presentation-history seam:

```text
previous_fixed
current_fixed
presentation(previous, current, alpha)
```

SR1 must consume SR0's already-resolved selection/contract rather than reintroducing authored path or semantic-name lookup into frame extraction. SR1 still must not derive final atlas/rotated UV behavior that belongs to SR2.
