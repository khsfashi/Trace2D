# Semantic Selector and Query Contract

Trace2D P3 exposes semantic entity queries through the protocol-independent `Trace2D::Agent` facade. Selector parsing and query behavior live in `engine/agent`; JSON, CLI argument handling, MCP, and LLM-specific protocol types remain outside the engine contract.

## Selector grammar

Selectors use exact, case-sensitive matching:

```text
#<semantic-id>
name:<entity-name>
tag:<tag>
type:<component-type>
```

Examples:

```text
#player
#boss
name:Boss
tag:enemy
type:Transform2D
```

A selector value must be non-empty. Unknown prefixes and bare values such as `enemy` are invalid syntax and return `invalid_selector`.

`type:` currently means an authoritative component type exposed by the agent facade. P3 exposes only `Transform2D`, so `type:Transform2D` matches every current scene entity. Later component systems can extend the same selector kind without changing the grammar.

On shells where `#` starts a comment, quote ID selectors, for example `--selector "#player"`.

## Engine API

Multi-result query:

```cpp
trace2d::agent::QueryResult result = facade.Query("tag:enemy");
```

Single-result query:

```cpp
trace2d::agent::QueryOneResult result = facade.QueryOne("#player");
```

Both return Trace2D-owned selector, snapshot, and error types. The API has no JSON or MCP dependency.

## Result semantics

`Query` is the multi-result primitive:

- zero matches are a successful empty result
- one or more matches are returned in deterministic observable scene order
- invalid selector syntax and unavailable scene state are errors

`QueryOne` requires exactly one match:

- zero matches return `no_match`
- more than one match returns `ambiguous_match`
- it never chooses an arbitrary entity

Stable query error strings are:

```text
scene_unavailable
invalid_selector
no_match
ambiguous_match
```

## Deterministic ordering

Query result order follows `Scene::ForEachEntity`, which scans live scene slots in ascending slot-index order. Repeated queries over the same scene state therefore return the same ordered matches.

Selectors do not sort or hash results after collection. Tag lookup uses the scene entity's normalized sorted tag storage, while the observable match order remains scene order.

## Performance policy

Queries are explicit observation/automation operations, not per-frame simulation work.

- no selector parsing or query collection is added to the simulation hot path
- scene iteration itself remains allocation-free
- result snapshots allocate only for entities that actually match
- no JSON is generated inside `engine/agent`

The current P3 query is a linear scan over live entities. That keeps semantics simple and deterministic while entity counts are small. Spatial indexes, semantic indexes, or caches should be added only after measured workloads justify their maintenance cost and memory overhead.

## CLI

```text
trace2d query --scene PATH --selector SELECTOR [--one] [--frames N] [--seed N] [--json]
```

Examples:

```powershell
trace2d query --scene tests/data/inspection.trace2d.toml --selector "#player" --one --json
trace2d query --scene tests/data/inspection.trace2d.toml --selector "tag:enemy" --json
```

Multi-result JSON has a stable top-level shape:

```json
{
  "command": "query",
  "status": "ok",
  "mode": "many",
  "selector": {
    "text": "tag:enemy",
    "kind": "tag",
    "value": "enemy"
  },
  "match_count": 1,
  "matches": []
}
```

`--one` uses the same shape with `"mode":"one"` and exactly one entity in `matches` on success.

CLI exit categories match the existing agent-tool boundary:

| Exit code | Meaning |
| ---: | --- |
| `0` | query succeeded, including a zero-match multi-query |
| `2` | invalid command-line usage |
| `3` | scene file I/O failed |
| `4` | scene TOML syntax/schema validation failed |
| `5` | query contract failure such as invalid selector, no match for `--one`, or ambiguity |

## Spatial-query extension point

Bounds and distance selectors are intentionally not invented in P3. Current inspection bounds remain nullable until renderer or physics state can provide authoritative geometry. A later spatial query API should consume those authoritative bounds rather than infer dimensions from transform scale or screen coordinates.
