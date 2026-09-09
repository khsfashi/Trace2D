# SAVE1: Versioned Save/Persistence Baseline

SAVE1 establishes the first public Trace2D persistence contract. It is deliberately narrow: stable semantic save documents, deterministic JSON, strict validation, and validated same-directory replacement. It does not serialize live engine memory or complete the migration/product-proof work tracked by #79 and #331.

## Save document contract

`trace2d.save.v1` uses these top-level members:

- `schema`: exactly `trace2d.save.v1`.
- `format_version`: the save format version. SAVE1 supports version `1`.
- `producer_version`: the Trace2D/tool version that produced the document.
- `save_id`: a stable semantic identifier for the save.
- `records`: the saved semantic records.

Each record contains:

- `id`: stable record identity.
- `type`: stable semantic type identity.
- `version`: the record schema version.
- `fields`: semantic fields.

Each field contains `name`, `kind`, and `value`. SAVE1 supports these bounded semantic kinds:

- `boolean`
- `signed_integer`
- `unsigned_integer`
- `float`
- `text`
- `float2`
- `float4`
- `entity_reference`
- `resource_reference`
- `enum_name`

Entity/resource references are semantic identities, not raw pointers, addresses, or runtime handles.

## Deterministic canonical form

Serialization is deterministic for the same semantic document:

- records are ordered by stable record id;
- fields are ordered by field name;
- the JSON member order is fixed by the serializer;
- output uses two-space indentation and one trailing newline;
- duplicate record ids and duplicate field names fail closed;
- unknown members, unsupported schema/format versions, invalid semantic values, and non-finite floats fail with diagnostics rather than being silently accepted.

A valid canonical document must parse and reserialize to the same bytes.

## Validated atomic replacement

`WriteSaveDocumentFile` uses the shared Core text transaction rather than owning a second save-specific write implementation:

1. validate and canonicalize the document in memory;
2. write the complete bytes to a unique sibling temporary file in the target directory;
3. flush and close that temporary file;
4. reopen it, read it, strictly parse it, and canonical-reserialize it;
5. require byte-for-byte equality with the intended canonical text;
6. atomically replace the authoritative target;
7. remove the temporary file on failure where possible.

The existing Agent authoring transaction also delegates to this shared Core path. SAVE1 therefore reuses one replacement mechanism instead of maintaining separate authoring/save implementations.

## Durability boundary

SAVE1 provides an atomic-visibility/process-crash baseline, not a full storage journal.

The contract does **not** claim power-loss atomicity across every filesystem/device combination. It does not currently fsync the file and parent directory on POSIX, provide a write-ahead journal, maintain recovery generations, or guarantee controller/media persistence after an unexpected loss of power. Those stronger guarantees require a separate durability/recovery design and proof.

## External-reference decisions

Trace2D's external-reference protocol was applied before introducing new infrastructure.

| Reference | Decision | Trace2D use |
| --- | --- | --- |
| Godot same-directory safe-save/replacement behavior | ADAPT | Use a sibling temporary file and replace only after the complete candidate is closed and validated. |
| Godot/community power-loss caveat for temp+rename without stronger flushing | ADOPT limitation | State the durability boundary explicitly instead of calling SAVE1 fully power-loss durable. |
| SQLite atomic-commit principle | ADAPT | Do not expose a partially written authoritative save. Validate a complete candidate before replacement. |
| SQLite database/journal dependency | REJECT | SAVE1 remains a lightweight file persistence layer rather than introducing a database subsystem. |
| Tiled separation of file-format version and producer/tool version | ADAPT | Keep `format_version` independent from `producer_version`. |
| Generic reflection/Variant object-graph serializer | REJECT | Persist bounded semantic records and stable identities rather than dumping arbitrary runtime graphs. |

See `EXTERNAL_REFERENCE_PROTOCOL.md`, `GAME_PRODUCTION.md`, and `PRODUCTION_ARCHITECTURE_CONTRACTS.md` for the governing repository rules.

## Performance and hot-path boundary

Persistence is lifecycle/tooling I/O. JSON parsing, canonical serialization, filesystem writes, and validation must not be placed in simulation, render, audio, or other per-frame hot paths. SAVE1 introduces no per-frame persistence work.

## Deferred work

SAVE1 intentionally leaves the following to bounded follow-up slices under #79:

- typed gameplay/world capture and restore adapters;
- stable-id reconstruction and world/template restore semantics;
- explicit migration chains across supported schema versions;
- authored-schema migration tooling with reviewable diagnostics;
- backup/recovery policy and stronger durability guarantees;
- the real external save/exit/restart/migration product proof tracked by #331.
