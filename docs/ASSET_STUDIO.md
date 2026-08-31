# Trace2D Asset Studio

Tracking umbrella: #318  
Current promoted engine-side boundary: #422

## State — 2026-08-28

**Deferred product direction. TraceSprite currently incubates the standalone asset-production workflow.**

Trace2D should not duplicate a second generation/pixel-processing/animation-review/approval application while TraceSprite is actively proving that workflow.

## Responsibility split

```text
TraceSprite / external authoring tools
  -> import or generate source assets
  -> process / pixelize / normalize
  -> produce/review animation
  -> human approve/reject
  -> export game-ready PNG / sprite sheet / bounded metadata

Trace2D
  -> validate deterministic interchange
  -> import into canonical Sprite/runtime assets
  -> render / animate / simulate
  -> expose engine-owned structured verification
```

Trace2D already owns substantial import authority through SPP3/SPP4. #422 adds the missing stable tool-neutral interchange/export boundary rather than another Asset Studio UI.

## Existing completed work

#320 and #321 remain valid completed foundations/evidence:

- set-level production intent / Art Profile references;
- bounded candidate comparison references composed with WorkResult/Workspace.

They are not deleted, but they no longer imply that Trace2D must finish an in-engine provider/library product.

## Deferred work

### #322 approved lineage

Deferred and non-core. Resume only if real external-tool use proves that canonical Trace2D project/runtime identity needs engine-side lineage beyond #422.

### #323 provider batch proof

Deferred and non-core. TraceSprite is the current environment for proving provider-backed production. Do not reproduce the same provider/auth/candidate workflow in Trace2D without a demonstrated engine-context advantage.

### #177 Asset Intelligence / Asset IR

Deferred and non-core. Do not freeze semantic parts/masks/rig inference/VLM analysis before a concrete runtime consumer exists. Native skeleton runtime should be built first; any later handoff schema is designed from that consumer backward.

## Current integration rule

Preferred boundary:

```text
approved external asset
 + versioned bounded interchange metadata
 -> existing Trace2D deterministic importer
 -> canonical engine asset
```

Reciprocal export should expose only metadata Trace2D actually owns. It must not pretend to reconstruct source-tool candidate history, prompts, model state, layer state or approval UI state.

## Product promotion rule

A deeper Trace2D Asset Studio slice requires retained evidence that an external tool boundary is insufficient **because of Trace2D-specific runtime/project context**.

Valid signals may include:

- repeated canonical-identity/revision handoff failures;
- owner review that genuinely requires live game context;
- measurable Agent/human rediscovery caused by missing engine-owned linkage;
- a new canonical runtime asset class whose handoff cannot be represented by the existing interchange contract.

When that happens, promote the smallest missing engine-side contract rather than copying TraceSprite wholesale.

## Human judgment

`TECHNICAL_PASS != OWNER_APPROVED` remains valid across both projects. Deterministic checks reject objective defects; aesthetic approval remains human.

## Non-goals while deferred

- no Trace2D provider SDK/auth integration;
- no duplicate candidate showroom;
- no automatic aesthetic score;
- no runtime generation;
- no second asset database;
- no broad semantic/VLM analysis pipeline without a runtime consumer.

See `POST_NIGHTFALL_DIRECTION.md` for the broader roadmap rationale.
