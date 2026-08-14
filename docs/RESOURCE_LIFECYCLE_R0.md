# R0 Unified Typed Resource Lifecycle

Issue: #86

R0 establishes the runtime identity, lifetime, dependency, unloading, and memory-evidence contract used by production resources after E2 scene/component composition.

## Authority

R0 does not replace deterministic import/parsing authorities such as `TextureAssetCache`, `SpriteAssetCache`, or the canonical Sprite serializer. Those remain setup/import frontends. `ResourceRegistry` is the runtime lifecycle authority after a concrete canonical resource has been produced.

```text
authored project-relative reference
        + explicit resource domain
                ↓ setup/import
canonical CPU resource
                ↓ PublishTexture / PublishSprite
ResourceRegistry slot
  identity + generation + domain
  strong dependencies / dependents
  caller retain count
  CPU/GPU residency evidence
                ↓
resolved typed handle
                ↓ steady state
O(1) slot/generation/domain check + direct typed payload view
```

Authored text identity is never pointer identity. Runtime code does not repeatedly normalize or hash authored paths after a handle has been resolved.

## Canonical identity

Resource identity is the pair:

```text
ResourceTypeDomain + canonical project-relative reference
```

R0 normalizes slash spelling and redundant `.` / repeated separators during explicit publish/failure/invalidation operations. It rejects:

- empty references,
- absolute references,
- portable drive-root references such as `C:/...`,
- any `..` traversal segment,
- control characters,
- `:` inside path segments.

The resource domain is part of identity. A texture handle cannot resolve as a sprite resource even if slot/index values are otherwise identical.

## Runtime handle

A resolved handle contains:

```text
slot + generation + resource domain
```

A slot generation advances whenever a ready/error slot is invalidated or unloaded. Reusing a free slot therefore cannot make an old handle valid again.

`Resolve()` performs only:

1. domain check,
2. slot bounds check,
3. ready-state + generation check,
4. direct `std::variant` typed payload lookup.

It performs no filesystem query, authored reference normalization, string lookup, hash lookup, allocation, ownership increment/decrement, or report construction.

## Ownership model

The registry owns resource payload storage. Callers keep small non-owning generation-safe handles and may explicitly `Retain` / `Release` a root resource when lifecycle reachability must survive `ReleaseUnused()`.

There is no tracing GC and no required `shared_ptr` atomic ownership traffic in steady-state resource access.

Strong resource-to-resource relationships are recorded separately from caller roots. A resource cannot unload while a live strong dependent exists. Dependencies are resolved before publication completes and later dependency replacement is transactional.

## Dependency graph

`SetStrongDependencies()` validates every target as a ready generation-safe handle before mutating the graph.

Strong cycles are rejected. Diagnostics include the canonical identity chain that would create the cycle. Duplicate strong dependencies are collapsed.

A successful relationship is stored in both directions:

```text
owner.dependencies
resource.dependents
```

This allows bounded inspection and explicit unload checks without rediscovering references from files or strings.

Soft/lazy dependencies are intentionally not part of R0. They must remain semantically distinct if introduced later.

## Explicit unload

R0 exposes three lifecycle operations:

- `Unload(handle)` — fails while explicitly retained or while live strong dependents exist,
- `ReleaseUnused()` — explicit scan that repeatedly unloads unretained leaf resources,
- `ClearProjectResources()` — deterministic project shutdown that tears dependents down before dependencies and ignores caller root retains because project ownership is ending.

The dependency graph is acyclic by construction, so project clear always has a leaf to remove. Slot-order selection makes the teardown order deterministic for the same published graph.

Error entries are explicit cache state. A recorded failed load cannot silently become ready on a later publish: the caller must first invalidate the error entry. This keeps diagnostics inspectable while allowing an explicit retry after source/environment change.

## Texture CPU versus renderer residency

`TextureResource` keeps canonical metadata and canonical RGBA8 CPU payload separate from renderer residency evidence.

Metadata retained across the boundary includes:

- width/height,
- sRGB/linear color-space intent,
- straight/premultiplied alpha convention,
- CPU retention policy and reason,
- whether a renderer resource is resident,
- engine-known renderer bytes when known.

A texture declares one CPU retention policy:

- `Required` — canonical CPU pixels may not be released,
- `Releasable` — CPU pixels may be discarded after explicit lifecycle work,
- `Reacquirable` — CPU pixels may be discarded because source/package state can rebuild them.

`ReleaseTextureCpuPayload()` uses an explicit vector swap so released canonical pixels do not remain as retained vector capacity by accident. Width/height/color/alpha metadata and renderer residency evidence remain valid afterward.

R0 records only engine-known renderer bytes. It does not claim unknown driver allocation, residency overhead, fragmentation, or allocator bookkeeping as exact GPU memory.

### Renderer texture identity boundary

The renderer does not mint a second texture identity. `render::TextureHandle` is the canonical typed `ResourceHandle<TextureResource>` produced by `ResourceRegistry`.

Production texture upload therefore follows this order:

```text
PublishTexture / resolve canonical texture
                ↓
ResourceHandle<TextureResource>
                ↓ CreateTextureRgba8 / CreateSpriteTextureRgba8
renderer-owned SDL GPU residency
                ↓ draw / capture
O(1) canonical slot + exact generation validation
```

Renderer GPU residency is indexed by the canonical texture slot and records the exact canonical generation beside the SDL texture pointer. Vector growth can occur only while explicit upload/setup introduces a higher canonical slot. Normal draw/capture resolution performs no allocation, hash lookup, path lookup, or ownership operation.

`DestroyTexture(handle)` releases derived GPU residency only when both slot and generation match. A stale handle is a no-op and cannot release a replacement texture that reused the same canonical slot with a newer generation. Rendering with that stale generation is rejected before the SDL texture pointer is used.

Canonical resource unload remains an explicit `ResourceRegistry` operation owned by the caller/project. The expected teardown sequence is renderer residency release followed by canonical resource unload when dependency/retain rules permit it.

## Memory evidence

Explicit inspection reports separately:

- known retained CPU bytes,
- retained dynamic container capacities,
- engine-known renderer GPU bytes,
- CPU retention policy,
- whether the canonical CPU payload is resident,
- whether the renderer resource is resident,
- the retention reason,
- dependency/dependent identities,
- load state and generation,
- structured error state.

Snapshot construction may allocate copied strings/vectors because it is explicit inspection work. Normal `Resolve()` does not construct a snapshot.

## Baseline resource classes

R0 proves the common contract with two different resource payloads:

- `TextureResource`,
- `SpriteResource`.

Sprite resources use the same generation-safe slot model and may strongly depend on one or more texture resources. The registry therefore demonstrates both a leaf texture-like resource and a metadata resource with explicit dependencies without creating a second Sprite asset parser/serializer.

Future TileSet, Font, Audio, Material, and similar resource payloads should extend this same lifecycle authority rather than creating subsystem-local handle allocators.

## Performance contract

Setup/import/lifecycle work may allocate, normalize text, hash identity keys, build dependency metadata, scan the registry, resize renderer residency storage during upload, or build inspection snapshots.

Steady-state resource access must not do any of those things. The R0 tests retain one texture handle and perform 10,000 direct resolves while proving that the canonicalization counter is unchanged and registry filesystem-query count remains zero. Renderer draw/capture texture resolution is likewise a direct slot/generation check over retained residency storage.

The baseline deliberately does not add:

- asynchronous loading,
- a generic job/thread system,
- background reclamation,
- per-frame dependency scans,
- per-frame memory-report formatting,
- per-frame path/source-file checks,
- generic reflection/property bags.

Streaming, batched staging, or async work should be introduced only after measured production workloads justify them.

## Acceptance evidence

`ResourceRegistryTests.cpp` covers:

1. canonical project-relative identity and invalid path rejection,
2. duplicate ready-publish immutable-state reuse,
3. typed-domain mismatch rejection,
4. stale generation rejection after unload/slot reuse,
5. strong dependency cycle diagnostics,
6. unload blocking under live dependents and caller retains,
7. deterministic dependent-before-dependency project clear,
8. separate CPU/container/GPU memory evidence and CPU-retention policy,
9. 10,000 retained direct resolves with no canonicalization/filesystem work,
10. texture canonical CPU release while renderer residency and color/alpha metadata remain explicit.

`RendererTextureLifecycleGpuSmokeTests.cpp` additionally proves the renderer integration on a real presentation GPU:

11. unload and republish reuse the canonical texture slot with a newer generation,
12. stale renderer destruction cannot release the replacement generation,
13. stale-generation rendering is rejected while the live replacement still renders successfully.

Hosted exact-head Windows/MSVC `/W4 /WX` build and full CTest remain the integration acceptance authority. The owner Windows GPU gate is the acceptance authority for the renderer stale-generation smoke because skipped GPU tests fail that gate.

## Handoff

While the #86 implementation PR is open, continue/fix R0 only. After it merges green and #86 closes, the exact next core-lane item is #87 — templates/world lifecycle.