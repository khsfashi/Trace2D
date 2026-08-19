# Asset Candidate Set — minimal comparison substrate

Tracking issue: #321  
Parent product direction: #318  
Production intent: `docs/ASSET_PRODUCTION_SPEC.md` / #320  
Review authority: `docs/WORK_RESULT.md`, `docs/WORKSPACE.md`

## Purpose

`AssetCandidateSet` is deliberately small. It groups a finite set of alternatives for one already-declared production item and points at artifacts already owned by one exact `WorkResult` revision.

It does **not** own candidate approval, rejection, provider state, generation policy, aesthetic scoring or approved-library lineage.

```text
AssetProductionSpec
  owns set/item + candidates_per_item
        |
        v
AssetCandidateSet
  owns only candidate id/ordinal + artifact references
        |
        v
current WorkResult revision / WorkspaceSnapshot
  owns artifacts, verification, feedback, review and approval
```

## Version 1 format

```toml
format_version = 1

[candidate_set]
production_set = "forest-monsters-v1"
item = "mossling"
work_id = "asset-studio-forest-monsters"
revision = "candidate-review-r1"

[[candidates]]
id = "mossling-a"
ordinal = 1
artifacts = ["mossling-a-native", "mossling-a-nearest"]
```

Only these fields are accepted. Unknown fields fail closed. In particular, fields such as `provider`, `model`, `status`, `approved`, `rejected` or `aesthetic_score` are not part of this contract.

## Validation

Setup/tooling validation requires:

- `production_set` equals the referenced `AssetProductionSpec.id`,
- `work_id` equals the referenced production spec's `work_spec`,
- `item` exists in the production spec,
- candidate IDs are non-empty and unique,
- positive ordinals are unique and do not exceed `candidates_per_item`,
- candidate count does not exceed `candidates_per_item`,
- `revision` equals the current Workspace/WorkResult revision,
- every artifact ID belongs to that exact current revision.

Stale revisions and unknown artifacts fail rather than being silently rebound to newer output.

## Comparison view

`BuildAssetCandidateComparison(...)` composes candidate grouping with the existing `WorkspaceSnapshot`. The result copies bounded `WorkArtifact` metadata for presentation; it does not duplicate decoded images, GPU resources or an approval state machine.

Human feedback/approval continues to use ordinary Workspace actions against the exact current revision. A candidate comparison is presentation grouping, not completion truth.

## External reference decisions

- **InvokeAI Gallery/Boards — ADAPT:** outputs remain organized and recallable with useful metadata. **REJECT:** importing its application/database model as Trace2D project truth.
- **ComfyUI execution history/output — ADAPT:** retain stable execution/output references. **REJECT:** binding Trace2D candidate identity to a ComfyUI workflow graph or queue.
- **Trace2D WorkResult/Workspace — ADOPT:** reuse revision, artifacts, feedback, review, approval and stale-action validation directly.

No dependency was added.

## Performance boundary

Candidate parsing, validation and comparison composition are explicit developer-tooling work.

They add no:

- frame-loop parsing or filesystem scan,
- renderer/resource ownership,
- provider execution,
- image decode or GPU upload,
- ordinary gameplay allocation/string lookup.

## Sequencing

#321 closes the immediate Asset Studio foundation checkpoint. After it is green, the operational lane resumes at **#89 Material2D / Shader2D**.

#322 approved-library lineage and #323 live-provider batch proof are deferred until the broader production foundation through #79 is complete. This avoids freezing the later production model around the current Sprite-only capability set.
