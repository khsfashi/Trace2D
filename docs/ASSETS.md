# Texture Asset Contract

Trace2D's first P6 asset slice is intentionally narrow: text-authored project-relative texture references resolve to immutable decoded RGBA8 CPU assets through an explicit cache. GPU textures remain renderer-owned presentation resources.

## Reference identity

A texture reference is a UTF-8 path string authored relative to the project root supplied to `TextureAssetCache`.

Canonical cache identity follows these rules:

- references are project-relative only; absolute paths are rejected,
- both `/` and `\` separators are accepted and normalized to `/`,
- repeated separators and `.` path components are removed,
- any `..` component is rejected instead of being resolved,
- Windows drive-prefixed paths such as `C:\...` are rejected on every platform,
- cache identity remains case-sensitive even on case-insensitive filesystems,
- the canonical project-relative path itself is the asset ID; no machine-specific absolute path enters authored identity.

Examples:

```text
textures/player.png       -> textures/player.png
textures/./player.png     -> textures/player.png
textures\player.png       -> textures/player.png
../shared/player.png      -> rejected
C:\game\player.png       -> rejected
```

This keeps authored references portable and deterministic without introducing a global asset database or generated GUID registry.

## Supported texture import

The initial decoder accepts:

- PNG
- JPEG
- BMP
- TGA

Import decodes the source exactly once per successful cache miss into tightly packed top-down RGBA8 bytes owned by `TextureAssetData`.

The decoder is deliberately CPU-side and SDL-free. `engine/assets` does not create windows, renderer objects, SDL textures, or GPU handles.

## Ownership and reuse

`TextureAssetCache` owns successful imports through `std::shared_ptr<const TextureAssetData>` entries.

A successful repeated reference returns the same immutable decoded asset object:

```text
Load("textures/marker.png")   -> import + cache miss
Load("textures/marker.png")   -> same object + cache hit
Load("textures/./marker.png") -> same object + cache hit
```

Callers may retain the returned shared pointer independently of the cache. Removing an entry from the cache does not invalidate a pointer already retained by a caller.

Failures are not cached. A missing or broken source may therefore be fixed and loaded successfully by a later explicit request without restarting the process.

## Invalidation

There is no background file watcher or implicit timestamp polling in this slice.

- `Invalidate(reference)` removes one canonical successful cache entry.
- `Clear()` removes all successful cache entries.
- reloading after invalidation performs a new file read and decode.

This makes invalidation explicit and testable and avoids hidden filesystem work in steady state.

## Diagnostics

Failed loads return a `TextureAssetDiagnostic` with a stable machine-readable code plus human-readable context:

```text
invalid_reference
unsupported_format
missing_file
read_failure
decode_failure
size_overflow
```

Diagnostics include the authored reference and, once resolution is possible, the resolved filesystem path. Gameplay/runtime state does not depend on those machine-local diagnostic paths.

## Cache metrics

`TextureAssetCache::Metrics()` exposes:

- total requests,
- cache hits,
- cache misses,
- successful imports,
- failed imports,
- current cached asset count.

The committed seven-sprite-style test uses one player texture and six marker references. It deterministically produces:

```text
requests:            7
cache misses:        2
successful imports:  2
cache hits:           5
cached assets:        2
```

This is the minimum evidence that repeated sprite-style references reuse imported state instead of repeatedly discovering or decoding the same file.

## Performance contract

The asset slice is setup/load-time infrastructure, not a frame-loop service.

- no per-frame filesystem scanning,
- no per-frame source decoding,
- no automatic modification-time polling,
- no renderer-owned frame-list allocation or growth,
- no GPU resource recreation is introduced by the CPU cache,
- successful cache lookup is average O(1) by canonical path ID,
- file read and decode occur only on explicit cache miss.

Code that needs GPU presentation should upload an imported `TextureAssetData` to the renderer once for the relevant renderer/resource lifetime and reuse that renderer-owned handle. The asset cache never treats a GPU handle as authoritative asset identity.

## Scope boundaries

This slice intentionally does not add:

- an editor asset database,
- generated GUID/meta files,
- texture atlases,
- global texture sorting,
- hot-reload watchers,
- generic importer plugins,
- renderer ownership inside the asset layer.

Future text/UI work may place the same project-relative reference strings in authored scene or UI data without changing the cache identity contract.
