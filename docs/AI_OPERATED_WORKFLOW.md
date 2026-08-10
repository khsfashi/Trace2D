# AI-operated product workflow

Issue umbrella: #96.

Trace2D is not merely a 2D engine that happens to expose APIs convenient for coding agents. Its product direction is to make the **AI the primary operator of the supported game-development iteration loop** while preserving explicit human authority over intent, taste and final approval.

Canonical product statement:

> **Humans define intent and judge the result. AI owns the iteration in between.**

A shorter product line is:

> **Tell AI what to build. Review the result.**

## 1. Product boundary

Trace2D should make this workflow practical:

```text
Human intent
    |
    v
AI plan / author / generate
    |
    v
Build / import / normalize
    |
    v
Run / inspect / interact
    |
    v
Deterministic verification
    |
    v
Presentation evidence
    |
    v
Multimodal review where necessary
    |
    v
AI diagnose / repair / re-verify
    |
    v
Human review / feedback / approval
    |
    +---- requested revision ----> AI
```

The human should not be required to open a conventional editor and manually tune every intermediate value merely because an AI changed a particle, animation, UI layout or gameplay behavior.

The target is not "remove humans from game development." The target is to move the human to the parts where human judgment has the highest value: **intent, creative direction, subjective quality, feedback and approval**.

## 2. Three-layer judgment model

Every future Trace2D workflow should separate three kinds of judgment instead of mixing them.

### 2.1 Deterministic / structured verification

If the engine owns the truth, verify it directly.

Examples:

- an animation event fired at frame 18,
- an entity reached the expected transform,
- an enemy's health became zero,
- a UI element exists and is focusable,
- a particle emitter stayed under an authored capacity,
- a resource reference resolved successfully,
- a draw-call/resource budget was satisfied,
- a save/load round trip preserved required state.

Do not ask a vision model to infer engine-owned facts from screenshots when Trace2D can expose the fact directly.

### 2.2 Multimodal / perceptual review

Use multimodal AI only when the question is genuinely perceptual or subjective and cannot honestly be reduced to an engine-owned assertion.

Examples:

- does the attack read as powerful enough,
- does the fire effect visually resemble the intended style,
- does an animation look awkward despite correct frame/event state,
- is a UI composition visually crowded,
- is foreground/background separation sufficient for readability.

Multimodal findings are **advisory review evidence**. They are not authoritative gameplay state and do not silently override deterministic results.

### 2.3 Human final judgment

The user owns decisions such as:

- whether the result matches creative intent,
- whether subjective quality is acceptable,
- whether a visual effect should be stronger/weaker/different,
- whether a game feels fun,
- whether known tradeoffs are acceptable,
- whether the work is approved.

Trace2D must not pretend these decisions are deterministic.

## 3. Supported-feature Definition of Done

A feature is not complete merely because an AI can write code for it or a human can operate it manually.

For a feature to satisfy Trace2D's AI-operated product direction, the supported workflow should answer **yes** to the relevant parts of this question:

> Can an agent author or modify it, execute it, inspect its authoritative state, verify machine-verifiable behavior, diagnose structured failures, repair it, re-verify it, produce review evidence and hand the result to a human without requiring hidden editor-only state?

Not every subsystem needs every verb, but unsupported gaps must be explicit.

## 4. Intent and completion specification

Issue #97 owns the future machine-readable intent / Definition-of-Done contract.

The problem it solves is project-level ambiguity. A coding agent must not need previous chat history to determine:

- what the user asked for,
- which deliverables are expected,
- what is already implemented,
- which deterministic checks must pass,
- which results require perceptual review,
- which results require human approval,
- what remains incomplete.

This should remain a small text-first project/tooling contract rather than becoming a generic project-management framework.

## 5. Verification and repair loop

Issue #98 owns the unified verification/result direction.

The intended project-level composition is equivalent in spirit to:

```text
trace2d verify <project-or-spec> --json
```

That command/API should compose existing subsystem truths rather than create a duplicate truth model.

A failed result should return enough stable context for an agent to:

```text
verify
 -> locate failure
 -> inspect source/semantic target
 -> modify
 -> rebuild/re-run
 -> re-verify
```

The engine runtime does not silently edit game content. The agent owns modifications through explicit source/authored-data operations.

## 6. WorkResult / review package

Every meaningful AI task should be representable as a reviewable result record. The final public name is not frozen, but the semantic categories should support:

```text
WorkResult
- task/spec identity
- changed source/assets/content
- deterministic verification
- structured failures
- performance/resource evidence
- presentation artifacts
- multimodal findings (advisory)
- known limitations
- revision lineage
- human feedback
- approval state
```

This record is the bridge between the autonomous iteration loop and the human review experience.

## 7. Workspace instead of a mandatory editor

Issue #99 and `WORKSPACE.md` define the human review surface.

Trace2D does not need to reproduce a Unity-style authoring editor to satisfy its product goal. It does need a way for humans to see what the AI produced and give feedback efficiently.

The preferred human interaction is:

```text
Read -> Review -> Request -> Approve
```

rather than making manual property manipulation the primary path.

The Workspace must consume the same engine/Agent/result state as CLI/MCP. It is a client of shared structured truth, never a GUI-only authoritative database.

## 8. Benchmark the product claim

Issue #100 and `AUTONOMOUS_BENCHMARK.md` define the benchmark program.

Trace2D should eventually test the same agent on the same eligible 2D game tasks across:

1. Godot + generic coding tools,
2. Godot + a pinned reviewed Godot MCP/agent bridge,
3. Trace2D + its public Agent surface.

Primary evidence includes success rate, revision count, token usage, tool calls, visual-feedback calls, human intervention and deterministic verification coverage.

The purpose is not to make an unfair "young engine vs mature engine feature count" comparison. Capability eligibility and autonomous-operability results are reported separately.

## 9. Model/vendor independence

Trace2D's engine contracts remain independent of a specific model or provider.

- deterministic verification belongs to Trace2D,
- multimodal review consumes explicit artifacts through external adapters,
- agent orchestration may vary by user/tool,
- benchmark harnesses pin model/tool versions for reproducibility,
- no engine gameplay API depends on proprietary chat state.

## 10. Non-goals

This direction does not require:

- a full graphical authoring editor,
- visual scripting,
- a generic reflection/property system,
- an engine-integrated LLM runtime,
- an autonomous final creative approval system,
- fake deterministic scores for subjective quality,
- screenshot inference for facts already owned by the engine.

## 11. Product success criterion

Trace2D succeeds when a user can increasingly express intent at a high level, let an agent own supported implementation/iteration/QA, and spend human attention on the final game and meaningful feedback rather than repetitive editor operation.

Long-term aspiration:

```text
@Trace2D, make me an RPG.
```

The statement is intentionally aspirational until committed benchmark evidence proves which scope can actually be completed autonomously. Trace2D should publish measured capability rather than imply unverified autonomy.