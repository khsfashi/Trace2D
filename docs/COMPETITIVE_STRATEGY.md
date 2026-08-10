# Trace2D competitive strategy

Last reviewed: **2026-08-10**.

This document defines how Trace2D should treat mature 2D engines and Agent tooling as real competitors without turning the project into a feature-for-feature clone or making unmeasured superiority claims.

The central hypothesis is intentionally falsifiable:

> **When AI is the primary operator of a supported 2D game-development loop, an engine designed around deterministic execution, semantic observation and machine-verifiable state can reduce autonomous iteration cost compared with attaching Agent tooling to a conventional mature engine.**

Trace2D must test that hypothesis. It must not assume it is true.

Related contracts:

- [`AUTONOMOUS_BENCHMARK.md`](AUTONOMOUS_BENCHMARK.md)
- [`AI_OPERATED_WORKFLOW.md`](AI_OPERATED_WORKFLOW.md)
- [`EXTERNAL_REFERENCE_PROTOCOL.md`](EXTERNAL_REFERENCE_PROTOCOL.md)
- [`REFERENCE_PROJECTS.md`](REFERENCE_PROJECTS.md)
- GitHub Issues #96, #100 and #102

## 1. Godot remains a primary competitor

Trace2D does **not** remove Godot from the comparison merely because Trace2D began as a personal C++ engine project.

Godot is useful precisely because it is a strong mature counterfactual:

```text
mature general-purpose engine
+ large implemented 2D capability surface
+ increasingly capable Agent/editor/runtime bridges
```

against Trace2D's intended model:

```text
engine architecture designed for AI operation
+ deterministic/semantic authority
+ protocol-independent Agent surface
+ explicit verification/repair/review loop
```

The meaningful question is not whether Trace2D can reproduce every Godot feature. The meaningful question is whether Trace2D's architecture creates measurable advantages for supported AI-operated workflows, and whether those advantages remain as task complexity grows.

## 2. What Trace2D is and is not trying to beat

### 2.1 Compete on

Trace2D should try to improve measurable outcomes such as:

- autonomous task completion,
- independently verified correctness,
- repair/revision count,
- time/tool calls/tokens to verified success,
- structured-verification coverage,
- visual/multimodal escalation count,
- required human intervention,
- failure reproducibility and diagnostic quality,
- performance/resource evidence available to the Agent,
- ability to rerun authoritative behavior without the original stochastic Agent.

### 2.2 Do not compete by checklist cloning

Do not add a subsystem merely because Godot has one.

For each mature-engine capability, ask instead:

1. Is it needed by representative Trace2D games or benchmark tasks?
2. What authoritative state does the engine own?
3. How does an Agent author, inspect, verify and diagnose it?
4. What belongs in structured state versus visual/perceptual evidence?
5. What hot-path ownership, allocation and synchronization rules apply?
6. What public external-game contract proves the feature without sample-only shortcuts?

A smaller feature implemented with a strong Agent/verification contract can be more relevant to Trace2D's thesis than a broad feature copied without one.

## 3. Strongest-baseline rule

Trace2D must not choose a deliberately weak Godot bridge to manufacture a favorable result.

Immediately before each publishable benchmark generation (B0, B1, B2 and later), refresh the current Godot Agent ecosystem using primary sources and produce a short capability matrix.

Selection criteria include:

- current maintenance/activity,
- public availability and reproducible installation,
- license/redistribution suitability for the planned harness,
- editor authoring breadth,
- runtime game-state inspection,
- input injection,
- controlled/frozen/exact stepping when available,
- screenshot/presentation evidence,
- profiler/diagnostic access,
- game-side or independent test support,
- compatibility with the pinned coding Agent,
- whether normal users can access the compared capability without benchmark-only patches.

Popularity/stars are useful discovery signals but are **not** the selection rule.

The chosen lane must represent a strong normal external-user workflow, not a weakened configuration selected because Trace2D can beat it.

## 4. 2026-08-10 Godot Agent baseline snapshot

This is a review snapshot, not a permanent ranking. Refresh it at #102 activation.

### 4.1 `satelliteoflove/godot-mcp`

Primary source:

- https://github.com/satelliteoflove/godot-mcp

Current public surface is especially strong for runtime verification:

- frozen/deterministic playtesting,
- controlled time stepping and step-until,
- structured live runtime/entity state,
- broad input injection,
- editor/game screenshots,
- profiler data,
- runtime GDScript scenario setup,
- an Agent-oriented eval harness.

Its public README currently describes 21 tools / 86 actions and explicitly optimizes for an Agent verifying its own work.

**Current provisional B0 role:** strongest direct candidate for the runtime-verification comparison because it attacks nearly the same problem Trace2D claims an engine-native design can improve.

### 4.2 `hi-godot/godot-ai`

Primary source:

- https://github.com/hi-godot/godot-ai

Current public surface is especially strong for broad authoring/editor integration:

- roughly 43 MCP tools / 120+ operations in current documentation,
- scene/node/script/resource authoring,
- UI/material/animation/particle/camera/audio helpers,
- Godot-side test suites,
- run/stop and screenshot feedback,
- performance monitors,
- running-game inspection and synthetic input.

**Benchmark role:** important candidate when B0/B1 tasks stress authoring breadth more heavily than exact-time runtime control. It prevents Trace2D from defining "strong Godot tooling" only around one bridge implementation.

### 4.3 `Erodenn/godot-mcp-runtime`

Primary source:

- https://github.com/Erodenn/godot-mcp-runtime

Current public surface emphasizes:

- headless editing,
- runtime screenshots,
- input simulation,
- UI/runtime discovery,
- live GDScript,
- zero-footprint/transient runtime integration with stock Godot.

**Benchmark role:** useful control for whether installation footprint/editor-addon architecture, rather than engine semantics, is driving observed friction.

### 4.4 Selection decision at B0 activation

Do **not** freeze the final B0 bridge today.

At #102 activation:

1. refresh all three candidates plus any newly stronger maintained alternatives,
2. pin exact commits/releases,
3. run a small bridge qualification fixture before candidate trials,
4. choose the strongest single public lane that covers the committed B0 task requirements,
5. document why it was selected and why close alternatives were not,
6. disclose unavoidable capability asymmetries.

If a publicly documented combination of complementary Godot tools is the normal strongest workflow at that time, a **pinned reviewed stack** may be used instead of artificially forbidding it, but only when:

- the combination is ordinary/reproducible for external users,
- no component contains task-specific solution logic,
- all versions/configuration are recorded,
- the same Agent budget and independent verifier policy still apply.

The benchmark should compare the strongest credible workflow, not protect Trace2D from competition.

## 5. Losing results are valid results

Trace2D benchmark policy is explicitly non-promotional.

If Godot + Agent tooling wins an eligible task or suite on success, time, tokens, iterations, tool calls, human intervention or another metric, publish that result.

Do not:

- remove a losing task after seeing the result,
- relabel a task capability-ineligible after a losing run,
- switch bridge/model/retry policy after observing which lane is ahead,
- publish only best-of-N successful runs,
- collapse tradeoffs into a custom score that hides raw losses.

A result such as the following is useful engineering evidence:

```text
Godot bridge wins: authoring breadth / elapsed time
Trace2D wins: deterministic verification / replay / visual-escalation cost
```

The project improves by identifying where engine-native Agent architecture matters and where it does not.

## 6. Capability parity and competitive evidence are separate

Trace2D can be a credible competitor before broad feature parity, but claims must remain scoped.

Report two dimensions independently:

```text
Capability availability
- can this engine/tool environment honestly perform the task?

Autonomous operability
- given eligibility, how effectively does the same Agent reach verified success?
```

A missing Trace2D subsystem is a capability gap, not an Agent failure.

Conversely, once a task is declared eligible before trials begin, a Trace2D failure remains a failure and must not be hidden by feature-maturity language.

## 7. B0 candidate task slate

These are design candidates for #102, not yet frozen benchmark tasks. Final tasks require a current baseline qualification and independent-verifier implementation.

The first suite should stay small enough to run repeatedly while testing distinct parts of the AI-operated thesis.

### B0-A — input-driven deterministic movement

User-level goal:

> Make the player move right under a specified input sequence and stop at the expected final state.

Common acceptance concept:

- project starts successfully,
- player identity is unambiguous,
- the requested input path drives normal gameplay code,
- after the specified sequence the player reaches the required position/state,
- no forbidden test-only direct position assignment substitutes for gameplay input.

What it measures:

- input setup,
- runtime control,
- state observation,
- exact/controlled verification,
- visual inference avoided for an engine-owned fact.

### B0-B — semantic UI interaction changes game state

User-level goal:

> Add a small UI control that, when activated through the normal interaction path, changes an observable game state and displays the result.

Common acceptance concept:

- control exists and is reachable through the normal UI/input path,
- activation changes the required authoritative state exactly once,
- displayed state agrees with authoritative state,
- independent verifier can exercise the interaction.

What it measures:

- authored UI,
- semantic/structural inspection,
- input/focus/activation workflow,
- distinction between game truth and presentation.

### B0-C — timed gameplay transition

User-level goal:

> Implement a small state transition that occurs after a defined gameplay condition/time/input sequence and verify the transition.

Examples may include a cooldown becoming ready, an entity toggling state, or a counter changing after a bounded sequence.

Common acceptance concept:

- transition uses production gameplay code,
- precondition and postcondition are independently observable,
- verifier rejects one-frame/one-step-early or late variants when timing is part of the task.

What it measures:

- controlled execution,
- temporal correctness,
- diagnosis of off-by-one behavior,
- replay/checkpoint value.

### B0-D — authored visual element plus presentation evidence

User-level goal:

> Import or configure a provided texture/sprite-like visual element, place it according to the task specification, and produce a reviewable capture.

Common acceptance concept:

- resource is valid and referenced by normal project data,
- structural properties such as identity/position/order are verified programmatically where possible,
- capture proves presentation output,
- visual review is used only for genuinely visual acceptance criteria.

What it measures:

- asset workflow,
- structured verification before screenshot escalation,
- capture/tool overhead.

### B0-E — diagnose and repair a seeded semantic defect

User-level goal:

> A small provided project has an intentional behavioral defect. Reproduce it, identify the cause, repair it, and independently verify the result.

Initial mutation candidates:

- wrong movement scale,
- wrong semantic target/identity,
- one-step-late transition,
- UI state not propagated to gameplay state,
- incorrect input binding.

The task fixture must not reveal the answer through benchmark-specific hints.

What it measures:

- reproduce -> inspect -> diagnose -> repair -> re-verify loop,
- diagnostic signal quality,
- repair iterations,
- token/tool/visual cost to first verified success.

### Optional particle B0

A particle task may enter B0 only if the final selected Godot lane and Trace2D expose conceptually fair acceptance without giving one side a privileged task-specific oracle.

Otherwise defer the richer particle comparison to B1 after Sprite/content tooling and use B0 to prove the harness first.

## 8. B0 task admission rules

Before freezing any B0 task:

- all compared environments must be capability-eligible under a definition written **before** candidate runs,
- one known-good implementation must pass each environment's translated verifier,
- meaningful known-bad variants must fail,
- no environment receives task-specific helper commands unavailable to normal users,
- the task must exercise a real Agent workflow rather than only a trivial file edit,
- objective acceptance must be independent from screenshots when the fact is engine-owned,
- perceptual criteria must remain separate from deterministic criteria,
- task duration/cost must be low enough for repeated trials.

## 9. Competitive metrics and interpretation

Raw metrics remain authoritative. Particularly important comparative views are:

- success rate among capability-eligible tasks,
- time/tool calls/tokens to first independently verified success,
- repair loops per success,
- structured inspection/query count,
- deterministic verification count,
- screenshot/capture count,
- multimodal review count,
- human intervention count/type,
- replay/self-determinism success where applicable,
- infrastructure failures separately from implementation failures.

Do not assume lower tool count is always better. A structured query that prevents several speculative edits may be valuable. Use traces and failure taxonomy to explain why the metric moved.

## 10. README / public-positioning rule

The public project should be ambitious without making unsupported claims.

Acceptable positioning:

- Trace2D is an AI-first / AI-operated C++ 2D engine project.
- It is designed to test whether engine-native deterministic/semantic verification improves Agent-operated game development.
- Godot + strong reviewed Agent tooling is an explicit comparison baseline.
- Public Alpha capability is narrower than mature engines.
- superiority claims require committed repeated benchmark evidence.

Do not claim before evidence exists:

- "faster than Godot",
- "better than Godot",
- "more autonomous than Godot-MCP",
- arbitrary percentage improvements,
- general production feature parity.

After benchmark evidence exists, claims must include the task suite/version, sample count, pinned Agent/model/environment and the scoped metrics that actually improved.

## 11. Roadmap interpretation

Trace2D may continue growing into a practical production engine. Portfolio origins do not impose an artificial stopping point.

At the same time, roadmap growth should preserve the project's distinctive engineering question:

> **How should this subsystem be designed when an AI Agent must be able to author, operate, inspect, diagnose and verify it efficiently?**

That question should influence Sprite, TileMap, Physics2D, Audio, resources, UI, persistence and later systems without requiring every mature-engine feature to be reimplemented.

## 12. Refresh gates

Refresh this strategy and its baseline snapshot at least at:

- #102 / Benchmark B0 activation,
- #103 / B1 activation,
- #104 / autonomous micro-game activation,
- before any public comparative performance/autonomy claim,
- when a materially stronger Godot Agent integration appears,
- when Trace2D changes its Agent/verification authority model.

Current snapshot is evidence for planning only. The benchmark's pinned execution manifest, not this prose, is the final authority for any published comparison.
