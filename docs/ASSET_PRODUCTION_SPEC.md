# Asset Production Spec

Tracking: #318 Asset Studio, implementation slice #320.

`AssetProductionSpec` is the smallest committed project contract needed to recover set-level Sprite production intent and Art Profile references without relying on chat history.

It is an **extension anchored to #97 WorkSpec identity**, not a second workflow/completion system.

## Authority boundary

The ownership split is intentionally strict:

- `WorkSpec` owns task identity, deliverables, dependencies, verification classes, completion/review state and external human approval requirements.
- `AssetProductionSpec` owns only set-level creative production intent: requested items, dimensions, animation/direction requirements, explicit constraints, bounded candidate/provider-call budgets, the identity of the existing human-review acceptance, and Art Profile references.
- `WorkResult` owns produced/revised evidence.
- Workspace owns review/showroom presentation.
- SPP5 remains the replaceable provider boundary.
- canonical Sprite/resource contracts own imported runtime assets.

`AssetProductionSpec` deliberately has no `state`, `verified`, `approved`, provider/model identifier, candidate lifecycle, aesthetic score or runtime resource handle.

## External reference decision

The mandatory #324 reference pass for #320 compared current mature production precedents before freezing this contract.

### ComfyUI — ADAPT the durable intent principle; reject the graph as Trace2D authority

ComfyUI can save/load generation workflows as JSON and recover full workflows from generated media. Its current workflow API also versions saved workflow JSON. This is strong evidence that creative production intent should be recoverable as explicit serialized state instead of hidden session context.

Trace2D adapts that principle but does **not** import a node graph, provider/model parameters or workflow engine into `AssetProductionSpec`; SPP5 already owns the replaceable generation boundary.

### InvokeAI — ADAPT references/metadata separation; reject a parallel gallery database

InvokeAI exposes reusable production workflows plus Board/Gallery organization and image metadata that recalls generation settings. The useful lesson for Trace2D is to keep reusable creative references explicit and separate from canonical runtime asset truth.

Trace2D therefore records compact Art Profile references in committed project state. Candidate/showroom metadata remains #321 Workspace work rather than becoming a new authoritative gallery database here.

### Godot ResourceUID — ADAPT stable-reference intent; reject a new UID registry for #320

Godot uses project resource UIDs to keep references stable across resource moves/renames. Trace2D already owns its canonical project/resource identity semantics, so #320 does not add another UID database. Art Profile references are canonical project-relative asset identities and are validated lexically at parse time; actual asset existence/resolution remains project tooling work, not parser or runtime work.

### Reuse-before-build result

- **ADOPT dependency:** none.
- **ADAPT:** explicit serialized production intent, compact approved-reference identities, strict separation of creative metadata from runtime asset truth.
- **REJECT:** provider-specific graph/model fields, a second workflow/completion state machine, a Workspace-only gallery database, a new resource UID registry.
- **DEFER:** candidate/revision/showroom state to #321, approved provenance/lineage to #322, live provider/model configuration to #323.

## Format

The format is TOML and versioned independently from `WorkSpec` because it is a companion creative-intent contract, not a mutation of #97 completion semantics.

```toml
format_version = 1

[production_set]
id = "forest-monsters-v1"
work_spec = "forest-monsters-work"
owner_review_acceptance = "owner-approval"
art_profile = "forest-monsters"
candidates_per_item = 3
max_provider_calls = 30

[[items]]
id = "mossling"
asset_class = "sprite"
width = 64
height = 64
required_animations = ["idle", "walk", "attack"]
required_directions = ["right"]
constraints = ["transparent background", "single readable silhouette"]

[[items]]
id = "thornboar"
asset_class = "sprite"
width = 64
height = 64
required_animations = ["idle", "walk", "attack"]
required_directions = ["right"]
constraints = ["transparent background", "single readable silhouette"]

[[art_profiles]]
id = "forest-monsters"
description = "Approved forest-monster reference set."
creative_constraints = [
    "pixel-art clusters remain readable at native resolution",
    "single upper-left lighting direction",
]
approved_references = [
    "assets/monsters/approved/moss_golem.sprite.toml",
    "assets/monsters/approved/thorn_wolf.sprite.toml",
]
```

The requested asset count is the number of `[[items]]` entries. It is not duplicated as another count field that could drift.

## Field semantics

### `production_set`

- `id`: stable production-set identity.
- `work_spec`: existing #97 `WorkSpec.id` that owns task/review/completion truth.
- `owner_review_acceptance`: exact existing `WorkSpec.acceptance.id` that owns final human review. Cross-contract validation requires this acceptance to exist and use `verification = "human"`; the production spec therefore cannot invent a second review-required boolean or review state.
- `art_profile`: one declared `[[art_profiles]].id`.
- `candidates_per_item`: positive bounded intent for how many alternatives may be produced per item.
- `max_provider_calls`: positive set-level provider-call ceiling. It is provider-neutral and does not imply one candidate per call.

`ValidateAssetProductionSpecAgainstWorkSpec()` performs the explicit setup-time identity/verification-class check. Parsing alone remains side-effect free and does not inspect project files.

### `items`

Every item has a stable `id`, `asset_class`, positive target `width`/`height`, and optional arrays for required animations, directions and structural constraints.

This contract does not prescribe provider prompts, seeds, samplers, model IDs or workflow nodes.

### `art_profiles`

An Art Profile has:

- stable `id`,
- human-readable `description`,
- optional compact creative constraints,
- at least one approved canonical project-relative asset reference.

Approved references are creative evidence. They do not establish a deterministic aesthetic pass condition.

The parser intentionally rejects unknown fields such as `aesthetic_score` or provider-specific model configuration rather than silently turning subjective quality/provider state into project authority.

## Reference safety

Approved asset references must be lexical project-relative identities using `/` separators. Absolute paths, drive-letter paths, backslashes, empty path segments, `.` and `..` segments are rejected.

The parser does not touch the filesystem and does not resolve/import assets. Existence and canonical resource resolution belong to explicit project tooling so parsing remains deterministic, side-effect free and outside runtime hot paths. The committed #320 fixture additionally parses its referenced manifests through the existing canonical Sprite parser to keep the representative proof honest.

## Performance boundary

This format is setup/tooling state only:

```text
read committed production spec + WorkSpec
 -> parse once
 -> validate identity + human acceptance linkage once
 -> generate/process/review through Agent tooling
 -> import approved canonical assets
 -> runtime uses normal Sprite/resource state
```

No frame-loop parsing, provider metadata lookup, filesystem scanning, allocation churn or render-path branching is introduced by #320.

## Next

After #320 merges green, the exact Asset Studio continuation is #321: bounded Candidate Set + Workspace showroom. Candidate status, revision lineage, deterministic rejection evidence and comparison presentation must not be backfilled into this intent-only contract.
