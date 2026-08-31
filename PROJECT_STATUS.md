# Trace2D Project Status

Last owner roadmap review: **2026-08-28**

This file is explanatory context, not live-state authority. Operational next action comes from:

1. compiling code/tests,
2. live GitHub PR/issue/CI state,
3. explicit owner decisions,
4. `config/trace2d.core-lane.json`.

## Product rule

Trace2D is an AI-first / AI-operated C++20 2D engine.

> **Humans define intent and judge the result. AI owns the iteration in between.**

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Optimization target:

> **Author -> Run -> Observe -> Verify -> Revise through a compact semantic surface.**

## What Nightfall changed

PR #418 / #420 produced a closed owner-playable Nightfall Survivors loop covering menu/product screens, character/stage selection, survival gameplay, pause, result/unlocks and persistent progression. Owner playtest accepted the product-completion goal; #418 was intentionally closed without merge so the project could return to engine work.

Consumed historical Nightfall evidence is pinned to exact PR #418 head:

```text
3974489c75b3b7fa460d07835855657cebbe3f3c
```

Do not treat a movable branch name as the evidence identity.

This evidence means Trace2D no longer needs another bespoke game merely to prove that a real game-shaped product can run.

#12 is now a **flagship qualification** checkpoint: reuse Nightfall evidence and close only missing current-public-engine deltas after persistence, rather than rebuilding gameplay/content.

See `docs/POST_NIGHTFALL_DIRECTION.md`.

## Frozen Sprite continuity

Roadmap simplification does not erase frozen architecture contracts:

- **#144 / SA0** remains the frozen deterministic Sprite animation timing/frame/event authority;
- SA1-SA4 compose that SA0 authority rather than replacing it;
- SR0-SR8 remain the production Sprite renderer/presentation continuity;
- SPP0-SPP5 remain the deterministic Sprite processing/import/generation-orchestration continuity.

Later work must compose these contracts rather than create parallel Sprite truth or silently weaken them.

## Asset-production boundary

Actual TraceSprite repository state now makes the separation explicit:

```text
TraceSprite / external tools
  = import/generate/process/animate/review/approve/export

Trace2D
  = deterministic interchange/import
  + canonical runtime assets
  + render/animation/runtime semantics
  + engine-owned verification
```

Trace2D already has SPP3/SPP4 deterministic Sprite import foundations. The missing promoted boundary is #422 **Asset Interchange V1**, including reciprocal supported metadata export.

Asset Studio #318/#322/#323 and broad Asset Intelligence #177 are deferred/non-core while TraceSprite incubates the production workflow.

## Current implementation lane

As of this review, persistence remains the active program. PR #398 owns SAVE1 / #397 and progresses #79. Always inspect its current live CI/head before acting; do not infer readiness from this file.

The machine lane now makes the owner-approved interruption explicit:

```text
#397 / PR #398 SAVE1
 -> #424 P0 Agent Skills / Progressive Discovery
 -> #399 SAVE2 typed gameplay adapters
 -> remaining #79 persistence/migration slices
 -> #331 real exit/restart/migration product proof
```

#424 must not preempt unfinished SAVE1. Once #397 closes, feature breadth pauses until the progressive-discovery task is completed or the owner explicitly changes that decision.

The purpose of #424 is not to add more API breadth. It should reduce the cost for a fresh Agent to answer, progressively:

```text
Can Trace2D do this?
 -> What canonical Trace2D workflow should I use?
 -> Which exact APIs/tools/examples do I need now?
```

No post-Nightfall roadmap change waives persistence acceptance gates.

## Revised continuation after #331

Machine authority is `config/trace2d.core-lane.json`.

```text
#422 Asset Interchange V1
 -> #12 Nightfall-based flagship qualification
 -> #60 Mesh2D
 -> #176 Native Deterministic Skeletal Animation
 -> #61 Spine license gate
```

### #422 Asset Interchange V1

Keep this deliberately small. Reuse existing SPP3/SPP4/S1/SA authority; add only the versioned tool-neutral handoff/export contract required for approved external assets. No generation provider, candidate browser, semantic VLM analysis or Asset Studio UI.

### #12 Flagship qualification

Do not build a second game. Reuse exact consumed #418/#420 evidence where it already proves product behavior and qualify only remaining current-main/public-SDK deltas such as persistence, portability, GPU/profiler evidence and public external-consumer closure.

### #60 Mesh2D

Expand runtime presentation capability to arbitrary textured indexed geometry without turning SpriteRenderer into a general render graph.

### #176 Native deterministic skeleton

Own deterministic skeleton IR/evaluation/events/mixing/inspection and scoped Mesh2D skinning. Auto-rigging/generative E2E is no longer required for engine completion; external tools may later export compatible data through a narrowly promoted interchange contract.

## Deferred / not automatic continuation

- #318 Asset Studio umbrella
- #322 approved asset lineage
- #323 live-provider batch proof
- #177 broad Asset Intelligence / Asset IR
- #312 Semantic Project Graph (research-gated)
- #93 later lighting/navigation/platform/networking/hot-reload breadth

These require fresh owner/evidence promotion. Finishing an older prerequisite does not activate them automatically.

## Nightfall dogfood follow-ups

- #417 practical keyboard/InputControl completeness
- #421 deterministic UI containment/overlap diagnostics
- #419 Windows Defender/distribution safety
- #424 progressive Agent Skills/discovery, promoted to P0 after SAVE1 because repeated discovery/context cost is itself a product defect

#417/#421 remain normal product follow-ups unless they block/recur. #419 must be respected before affected public distribution. #424 is different: its sequence is explicitly promoted in the machine lane.

## Critical direction rule

A successful Nightfall product does **not** prove Trace2D is categorically better than mature engine + Agent workflows. That comparative thesis remains falsifiable and belongs to TraceResearch / #369.

The next phase should maximize new information:

- finish SAVE1 without throwing away current work;
- reduce Agent discovery/context/revision cost through #424 before adding more feature breadth;
- finish missing lifecycle correctness (remaining save/restart/migration);
- reduce duplicated product ownership through clean tool boundaries;
- expand genuinely new runtime expressiveness (Mesh2D/skeleton);
- continue measuring Agent complexity separately from feature breadth.

Do not add another proof, subsystem or IR merely because it looks like a conventional engine checklist item.
