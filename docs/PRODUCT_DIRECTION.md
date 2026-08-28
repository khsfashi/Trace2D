# Trace2D Product Direction

Status: **owner-approved product rule**  
Last amended: **2026-08-28**

Trace2D is not competing to expose the largest editor API, the most MCP tools, or the broadest clone of a mature engine.

The product claim to prove remains narrower:

> **A coding Agent should be able to author, run, observe, verify and revise a 2D game through a compact semantic surface with low discovery, context and revision cost.**

Short loop:

```text
Author -> Run -> Observe -> Verify -> Revise
```

Judgment rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

## What Nightfall proved — and did not prove

PR #418 / #420 provided owner-accepted evidence that Trace2D can support a real closed game-shaped product loop. The consumed historical PR #418 head is pinned at `3974489c75b3b7fa460d07835855657cebbe3f3c`.

That changes roadmap economics: another bespoke flagship game with substantially the same capability mix would mostly repeat evidence.

Nightfall does **not** prove the stronger comparative thesis that Trace2D is more Agent-efficient than mature-engine + Agent workflows. That remains a falsifiable research/product question under #369 / TraceResearch.

Accordingly:

- #12 is now a delta qualification checkpoint over the proven Nightfall workload, not a second game build;
- future product proofs should add new information, not merely more screens/content;
- real dogfood gaps are more valuable than checklist breadth when choosing polish work.

## Product moat

Optimize these together:

1. **compact semantic authoring** — typed/transactional operations over canonical state;
2. **progressive workflow discovery** — reveal canonical Trace2D usage only when the task needs it, then narrow to exact APIs/tools/examples;
3. **deterministic execution** — explicit fixed-step control and stable semantic identity;
4. **cheap structured observation** — inspect engine-owned facts directly;
5. **replayable verification** — exact inputs, frames, assertions and bounded failure evidence;
6. **human-visible closure** — playable output, targeted feedback and verified revision;
7. **clean external-tool boundaries** — do not duplicate upstream production tools when stable interchange is sufficient.

A feature is not automatically valuable because it increases breadth. A tool is not automatically valuable because it exposes another internal operation.

## Progressive disclosure is part of the public Agent surface

Trace2D should answer Agent discovery questions in layers rather than forcing broad repository reading up front:

```text
Capability: can Trace2D do this?
 -> Skill: what is the canonical Trace2D workflow?
 -> API/tool discovery: which exact public surface is needed now?
```

Capability metadata remains truth about availability/evidence. Skills should contain workflow knowledge and important authority/ordering rules that are not efficiently recoverable from raw symbol/tool descriptions. Public API/tool indexes remain the source for exact declarations and callable primitives.

Do not duplicate whole API references into skills, and do not preload all skills/tool schemas into every context. Keep descriptors small, bounded and cacheable where practical.

#424 is the P0 implementation of this direction after SAVE1 and before SAVE2.

## Agent Complexity is a product metric

Measure where practical:

- time/tool calls to first correct target discovery;
- files/resources read and rereads;
- input/output tokens;
- raw-text edits vs semantic transactions;
- authored revisions;
- build/run/validation calls;
- visual-feedback calls;
- human interventions;
- bounded evidence bytes;
- verified outcome and failure class.

Do not solve context pressure primarily by increasing prompts/budgets, adding benchmark-shaped shortcuts, proliferating narrow tools or dumping complete project state by default.

## Product lane vs research lane

Trace2D owns practical runtime/public Agent contracts, external-game authoring, deterministic verification, public SDK usability and real product dogfood.

TraceResearch owns controlled comparative claims and should be allowed to produce negative results. A successful Trace2D game is product evidence, not self-issued proof of superiority.

## External asset-production boundary

TraceSprite now actively incubates the standalone asset-production workflow. Trace2D should consume approved results through stable deterministic interchange rather than rebuild the same product inside the engine.

Current split:

```text
TraceSprite / external tools
  = import/generate/process/animate/review/approve/export

Trace2D
  = deterministic interchange/import
  + canonical runtime assets
  + render/animation/runtime semantics
  + engine-owned verification
```

#422 is the promoted engine-side integration task. #318/#322/#323 and broad #177 are deferred/non-core until real evidence demonstrates missing engine-context value.

## Build consumers before broad IRs

Do not design a large semantic Asset IR, graph, provider abstraction or analysis pipeline before a concrete consumer exists.

Preferred order:

```text
runtime/product consumer
 -> exact missing handoff or observability gap
 -> smallest stable contract
 -> implementation + evidence
```

This is why #176 native skeleton runtime remains core while #177 broad Asset Intelligence is deferred. Runtime semantics should define what external tools actually need to export.

## Breadth policy

For mature-engine capabilities:

- own the Trace2D semantic/deterministic contract;
- prefer proven backends when they fit;
- keep backend details out of gameplay/Agent-facing semantics;
- avoid normal-frame parsing, filesystem discovery, repeated string lookup and duplicate authority models;
- require measured evidence before broad abstractions;
- prefer runtime expressiveness that unlocks materially new game classes over repeated product demos.

The immediate owner-approved order is SAVE1 -> #424 progressive discovery -> remaining persistence. After persistence/qualification, the promoted breadth is currently Mesh2D then native deterministic skeleton.

## Decision test for future work

Before adding a subsystem, Agent tool or product surface, ask:

1. Does a current workload need it?
2. Does it unlock materially new runtime capability or reduce repeated Agent/human cost?
3. Is the blocker demonstrated by retained evidence?
4. Is another Trace project or mature external tool already the better upstream owner?
5. Can a smaller interchange/import/runtime contract solve it?
6. Is a proposed schema designed from a concrete consumer backward?
7. Does the work add new information rather than repeat a proof already accepted?
8. Can engine-owned facts be verified structurally without screenshot inference?
9. Can a fresh Agent discover the canonical workflow without broad unrelated context loading?

If the answers are mostly no, postpone the work.

See `POST_NIGHTFALL_DIRECTION.md` for the current roadmap amendment.
