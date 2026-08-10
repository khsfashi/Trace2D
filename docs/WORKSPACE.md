# Trace2D Workspace contract

Tracking issue: #99. Parent product umbrella: #96.

The Trace2D Workspace is the human-facing review surface for an AI-operated game-development workflow.

It is **not** intended to become a Unity-style editor where the primary workflow is selecting objects, manually tuning properties and saving hidden editor state.

The Workspace exists because humans still need to see the game, inspect what changed, understand verification evidence, judge subjective quality and provide feedback.

## 1. Product role

The intended human loop is:

```text
Read -> Review -> Request -> Approve
```

The intended AI loop around it is:

```text
receive intent/feedback
 -> author/modify
 -> build/run
 -> deterministic verification
 -> presentation evidence
 -> multimodal review where useful
 -> diagnose/repair/re-verify
 -> publish WorkResult
 -> wait for human approval or feedback
```

A human may still edit source/TOML directly with normal development tools. Trace2D simply does not make a large manual editor the mandatory source of truth.

## 2. Shared-state rule

The Workspace must not invent a separate GUI-only model.

Preferred architecture:

```text
Authoritative engine/project state
              |
              v
Protocol-independent Agent / verification / result APIs
      |                 |                 |
      v                 v                 v
     CLI               MCP            Workspace
```

Consequences:

- what the human sees and what the agent sees come from the same semantic state,
- GUI presentation never becomes authoritative gameplay truth,
- no hidden scene database is required just for the Workspace,
- Workspace-specific caching/presentation state is allowed but remains derived,
- expensive snapshots/reports are requested explicitly rather than generated every frame.

## 3. Core views

### 3.1 Recent work

The default landing experience should answer:

- what did the AI change,
- what passed verification,
- what failed,
- what needs subjective review,
- what needs human approval.

Example conceptual view:

```text
Recent work

Player attack animation      REVIEW
Fireball impact particle     VERIFIED
Enemy health bar             VERIFIED
Inventory layout             FAILED
```

This is result-first rather than hierarchy-first.

### 3.2 Review queue

Items requiring human judgment should be explicit.

Examples:

- visual quality/taste,
- animation feel,
- effect intensity,
- UI composition/readability,
- intentional acceptance of a documented performance/feature tradeoff.

The queue must not include machine-verifiable failures merely because a screenshot exists. Those remain verification failures.

### 3.3 Workspace/world view

A semantic project/world browser is useful for orientation and inspection.

It may expose:

- worlds/scenes,
- entities by stable semantic identity,
- components and authoritative state,
- assets/resources,
- animation/particle/UI/audio state,
- active verification status.

A traditional editable inspector is optional and secondary. The main modification path remains source/authored-data editing by humans or `Ask AI to modify`/equivalent feedback.

### 3.4 Artifact previews

The Workspace should present the smallest useful artifact for the review question:

- still capture for static visual checks,
- looped animation preview,
- particle loop/short sequence,
- short gameplay capture/replay,
- interactive game view when the user needs to play the result,
- UI preview,
- audio playback/evidence when audio exists.

Artifacts record enough context to identify the relevant simulation frame/time, viewport/camera and revision when those concepts apply.

### 3.5 Verification evidence

A result should clearly separate:

- deterministic pass/fail,
- structural/performance/resource evidence,
- multimodal advisory findings,
- known limitations,
- human approval state.

Do not blend these into one opaque AI confidence score.

### 3.6 Revision history

The user should be able to understand a sequence such as:

```text
v1 generated
 -> deterministic PASS
 -> multimodal: attack anticipation weak
 -> human: make it heavier
 -> v2 generated
 -> deterministic PASS
 -> human APPROVED
```

Revision records reference the associated WorkResult/evidence rather than storing an unbounded duplicate of engine state.

## 4. Feedback contract

Human feedback may be free-form natural language, with optional structured target identity when selected from the Workspace.

Examples:

```text
"Make this attack feel heavier."
"The fireball is too bright."
"Keep this particle exactly as it is."
"Rework only the inventory, not the combat HUD."
```

When a stable semantic target is known, the Workspace should pass that identity alongside the feedback so the agent does not need coordinate/pixel guessing to locate the object.

Feedback itself is user intent, not an automatic engine mutation. An agent interprets it, changes reviewable source/authored state, and re-runs verification.

## 5. WorkResult dependency

Issue #98 owns the shared result model. The Workspace should consume equivalent data rather than scrape logs.

Minimum useful categories:

```text
Task identity
Changed items
Deterministic verification
Diagnostics
Performance/resource evidence
Preview artifacts
Multimodal findings
Known limitations
Revision lineage
User feedback
Approval
```

## 6. Performance rules

The Workspace is tooling and may perform explicit expensive work, but it must not pollute normal game hot paths.

- no mandatory frame-by-frame JSON serialization,
- no continuous GPU readback merely to populate UI panels,
- no repeated filesystem rescans when state is unchanged,
- use explicit refresh/subscription/dirty-state semantics where the implementation needs them,
- preview capture/readback remains explicit presentation/tooling work,
- bounded histories/caches should replace unbounded session growth.

## 7. Implementation technology

The Workspace UI technology is deliberately not frozen here.

A web/native/hybrid client is acceptable if it preserves the boundaries above. The core engine must not become dependent on a UI framework solely because the Workspace exists.

## 8. Non-goals

V1 does not need:

- a scene gizmo for every authored property,
- animation curve editors,
- particle graph editors,
- a visual scripting graph,
- a material graph,
- a DOM/CSS-style engine UI architecture,
- GUI-only project truth.

A small manual control may be added when it demonstrably improves review or debugging, but manual authoring breadth is not the product goal.

## 9. Acceptance

The Workspace contract is proven when a representative AI-produced change can be reviewed end to end:

1. the user sees the new/changed result,
2. deterministic verification is visible separately from subjective review,
3. the relevant animation/particle/UI/gameplay result can be previewed or played,
4. the user can submit targeted natural-language feedback or approve,
5. an agent can consume that feedback, revise the project and return a new verified result,
6. no hidden editor-only state is required in the loop.