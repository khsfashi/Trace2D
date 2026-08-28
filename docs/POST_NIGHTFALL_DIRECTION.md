# Post-Nightfall product direction

Owner decision: **2026-08-28**

## Why the roadmap changed

Nightfall Survivors on PR #418 / #420 answered a question the older roadmap still planned to prove later: Trace2D can already support a closed owner-playable game loop with menus, selection, gameplay, pause, result/unlocks and persistent progression.

PR #418 was closed without merge after owner playtest accepted the product-completion goal. That retained evidence is valid even though the branch is not canonical engine source.

The project should therefore stop spending core-lane time on another game whose main purpose is to re-prove that a game can exist.

## Critical product reading

Nightfall does **not** prove the stronger Trace2D thesis by itself.

It proves:

- practical game composition is possible;
- public-ish engine breadth is sufficient for a real product-shaped loop;
- real dogfood finds reusable engine defects better than isolated subsystem checklists.

It does **not** prove:

- Trace2D is more Agent-efficient than a mature engine + strong tooling;
- every public SDK path is clean;
- persistence/migration/restart is complete;
- all current-main contracts were exercised by the closed Nightfall branch;
- broader genres/rendering/animation needs are already covered.

The stronger comparative claim remains owned by TraceResearch / #369. Do not convert one successful dogfood game into a universal superiority claim.

## Main strategic risks after Nightfall

### 1. Repeating proofs instead of increasing information

A second bespoke flagship game with substantially the same capability mix has low information value. #12 is therefore narrowed to a delta qualification against the proven Nightfall workload.

### 2. Duplicating TraceSprite inside Trace2D

TraceSprite now actively owns the standalone asset-production loop: import/generation, pixel processing, animation review, owner approval and PNG/sprite-sheet export.

Trace2D already owns deterministic Sprite import via SPP3/SPP4. The missing engine responsibility is a small stable tool-neutral interchange/export boundary, tracked by #422.

#322/#323 are removed from the core lane. Their concepts remain deferred and may be promoted only if real external-tool usage demonstrates an engine-context need.

### 3. Designing a broad Asset IR before a consumer exists

The older #177 attempted to define semantic parts, masks, inferred rig metadata, VLM analysis and auto-rig handoff before native skeleton runtime existed.

That ordering risks freezing a producer-shaped schema instead of a runtime-shaped contract. #177 is now deferred. Build the runtime consumer first, then add only the handoff data it actually needs.

### 4. Mixing runtime animation with asset generation

#176 remains important, but its completion is now native deterministic skeleton runtime through scoped Mesh2D skinning. Auto-rig and generative E2E are external-tool integration questions, not engine-runtime completion criteria.

## Responsibility split

```text
TraceSprite / external authoring tools
  -> produce / process / animate / review / approve
  -> export bounded game-ready asset data

Trace2D
  -> deterministic interchange/import validation
  -> canonical asset/runtime identity
  -> render / animate / simulate
  -> deterministic engine-owned observation and verification
```

This split is not anti-integration. It avoids duplicate product ownership while keeping the interchange contract open to TraceSprite, Aseprite and future tools.

## Revised post-persistence core order

After the currently active persistence program reaches #331:

```text
#422 Asset Interchange V1
 -> #12 Nightfall-based flagship qualification
 -> #60 Mesh2D
 -> #176 Native Deterministic Skeletal Animation
 -> #61 Spine license gate
```

Deferred/non-core:

```text
#318 Asset Studio umbrella
#322 approved-lineage product
#323 provider batch proof
#177 broad Asset Intelligence / Asset IR
```

They require fresh evidence-based promotion rather than automatic activation.

## Dogfood follow-ups

Nightfall-discovered issues such as #417 input completeness and #421 deterministic UI geometry diagnostics are valid product follow-ups. They should be promoted when they block/recur in real work, not automatically stop the core lane merely because they exist.

Distribution/security findings such as #419 remain real release concerns and must be resolved before making affected binaries public; they are distinct from engine-breadth sequencing.

## Decision test for future core work

Before promoting a new subsystem or product surface, ask:

1. Does it unlock a materially new game/runtime capability or remove repeated Agent friction?
2. Is the need demonstrated by a real workload rather than a feature checklist?
3. Does another Trace project or mature external tool already own the upstream production job better?
4. Can Trace2D solve the need with a smaller import/interchange/runtime contract?
5. Is the proposed schema designed from a concrete runtime consumer backward?
6. Does the work add new information, rather than repeat a proof we already accepted?

If the answers are weak, keep it deferred.
