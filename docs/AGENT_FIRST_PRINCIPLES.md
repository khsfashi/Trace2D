# Agent-first design principles

Trace2D is not a general-purpose engine with an AI adapter bolted on later. The runtime, tools, file formats, verification surfaces and future human review experience are designed so that an AI can operate supported game-development workflows through explicit structured contracts.

Canonical product statement:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Detailed product contract: [`AI_OPERATED_WORKFLOW.md`](AI_OPERATED_WORKFLOW.md).

## 1. Text first

Project configuration, scenes, tests, intent/acceptance metadata and authored content should prefer deterministic text formats that are readable, diffable and safe to edit with normal development tools.

Binary formats may exist for generated caches and packaged runtime data, but they must not be the only source of truth for authored content.

## 2. Protocol-independent agent surface

The engine's automation contract must not depend on MCP or any specific LLM product.

The intended layering is:

```text
Coding Agent
    |
    +-- CLI
    +-- JSON / JSON-RPC
    +-- MCP adapter
            |
            v
       Agent Facade
            |
            v
         Runtime
```

MCP is an adapter. The engine API remains independently testable and scriptable.

## 3. Deterministic simulation control

Automated tests must be able to own simulation time instead of waiting for wall-clock time.

The runtime supports a fixed simulation step and explicit frame advancement. Given the same project data, initial state, seed, inputs and frame sequence, deterministic subsystems should produce the same observable result within their documented numeric boundary.

## 4. Observable state over visual guessing

Screenshots are useful for visual QA, but agents should not need to infer gameplay state from pixels when the engine already owns that state.

Runtime inspection should expose structured information such as:

- stable entity identity,
- semantic name and tags,
- transforms and bounds,
- component state,
- visibility and enabled state,
- collision/query results,
- active scene and simulation frame.

## 5. Semantic selection

Automation should target stable semantic identities rather than fragile screen coordinates.

Examples of intended selectors:

```text
#player
#boss
tag:enemy
role:button name:"Start Game"
```

Coordinates remain available when the test explicitly needs them, but they are not the primary automation contract.

## 6. Headless parity

Gameplay simulation, queries, inputs and assertions should work without a visible window whenever rendering is not required.

Windowed and headless modes must share the same authoritative runtime/game logic. Headless mode must not become a separate simplified game implementation.

## 7. Testability is an engine feature

The engine provides or plans first-class facilities for:

- virtual input/actions,
- frame stepping,
- state queries,
- assertions,
- deterministic seeds,
- screenshot/capture artifacts,
- machine-readable failure reports,
- explicit profiling/analysis,
- project-level verification composition.

Gameplay automation is part of the runtime architecture, not only an external test harness.

## 8. Small composable commands

Prefer a compact set of orthogonal operations over a large collection of narrow LLM tools.

The target vocabulary is close to:

```text
build
run
inspect
query
input/action
step
assert
capture
test
analyze
profile
verify
migrate
```

Higher-level agent workflows compose these primitives.

If a task can be accomplished safely by editing versioned source/authored files, do not duplicate the entire editor/source surface as an MCP action merely to make it "AI accessible." Reserve runtime tools for capabilities that genuinely require runtime/engine authority.

## 9. Explicit ownership and predictable C++

Engine code should favor clear ownership, RAII, searchable APIs and explicit lifetime boundaries over clever metaprogramming.

Hot paths are measured before specialized allocators or lock-free structures are introduced. Optimization decisions require profiles/workloads rather than assumptions.

## 10. Failures must be actionable

Machine-facing commands should return stable exit behavior and structured diagnostics. A failed automated test should identify what was expected, what was observed and enough runtime/source context to reproduce the failure.

The long-term requirement is stronger: supported failures should make **verify -> diagnose -> repair -> re-verify** possible without relying on previous conversation memory.

## 11. Deterministic where possible

If Trace2D owns machine-readable truth, that truth is the primary correctness oracle.

Examples include:

- entity/component state,
- frame/event timing,
- animation semantic state,
- input outcomes,
- particle lifecycle/capacity,
- UI semantic/layout state,
- resource validity,
- structural performance budgets,
- persistence/migration results.

Do not replace a structured assertion with screenshot inference merely because a multimodal model is available.

## 12. Multimodal where necessary

Perceptual/subjective questions may use image/video/audio artifacts and multimodal AI review when they cannot honestly be reduced to engine-owned assertions.

Examples include animation feel, style resemblance, effect intensity, composition and readability.

Multimodal findings are advisory:

- they are recorded as review evidence,
- they may recommend a revision,
- they do not silently mutate game state,
- they do not override deterministic failures/passes,
- they do not replace final user approval.

## 13. Human judgment at the end

Trace2D intentionally preserves human authority over:

- creative intent,
- taste,
- fun,
- subjective quality,
- acceptance of tradeoffs,
- final approval.

The engine should reduce repetitive manual operation, not pretend subjective game design is fully deterministic.

## 14. Result-first human interaction

Trace2D plans a result-review Workspace rather than a mandatory broad authoring editor.

The preferred human loop is:

```text
Read -> Review -> Request -> Approve
```

The Workspace consumes the same Agent/verification/result state as other clients. No GUI-only model becomes authoritative.

See [`WORKSPACE.md`](WORKSPACE.md).

## 15. Benchmark the AI-first claim

Claims such as "AI-first" must eventually be supported by committed evidence.

Trace2D plans matched autonomous trials using the same eligible task and coding agent across recorded baselines such as:

```text
Godot + generic coding tools
Godot + pinned Godot MCP/agent bridge
Trace2D
```

Success rate, revisions, token/tool cost, visual-feedback dependence, human intervention and deterministic verification coverage are more meaningful than a demo that succeeded once.

See [`AUTONOMOUS_BENCHMARK.md`](AUTONOMOUS_BENCHMARK.md).

## 16. Feature Definition of Done is AI-operability aware

A supported feature is not considered fully aligned with Trace2D's product direction merely because:

- a programmer can call the API,
- a human can manipulate it manually,
- one screenshot looks correct.

For relevant workflows, ask:

> Can an agent author/modify it, execute it, inspect authoritative state, verify machine-verifiable behavior, diagnose failures, repair/re-verify, and produce review evidence without hidden editor-only state?

Unsupported verbs are allowed during staged development, but they must remain explicit rather than silently assumed complete.

## 17. Nondeterministic creation must cross a deterministic boundary

External generation models, human art edits and other creative tools may be nondeterministic.

Once content enters Trace2D's canonical project/runtime path, deterministic processing/validation should cover every property that Trace2D can honestly own.

Do not make model output authoritative merely because it was generated by AI.

## 18. Performance and verification work stay out of hot paths

Agent snapshots, JSON/MCP payloads, WorkResult assembly, multimodal artifacts, profiling reports, migration reports, benchmark records and review history are explicit tooling work.

They may allocate or perform bounded scans when requested. They must not silently become mandatory per-frame work.

## 19. No unmeasured autonomy claims

The aspirational interaction:

```text
@Trace2D, make me an RPG.
```

is a long-term product target, not a current feature claim.

README/release claims should distinguish:

- implemented engine capability,
- capability-eligible autonomous benchmark results,
- planned workflow,
- subjective examples/demos.

Trace2D should prefer reproducible evidence over impressive but unrepeatable agent anecdotes.

## 20. Agent complexity is a product constraint

Runtime correctness and Agent usability are separate product dimensions. Internal engine complexity may be necessary for determinism, performance, lifetime safety or backend isolation, but that does **not** justify exposing equivalent complexity to an Agent.

For common supported workflows, Trace2D must minimize the concepts, resources and operations that an Agent must understand to express intent and verify the result.

Before freezing a major subsystem contract, explicitly review both:

```text
engine-facing architecture
        !=
Agent-facing mental model
```

Do not answer an Agent-complexity problem by default with more prompt text, more required reading or a larger collection of narrow tools. First reduce unnecessary exposed concepts and repeated discovery work.

The B1 postmortem in [`BENCHMARK_B1_POSTMORTEM.md`](BENCHMARK_B1_POSTMORTEM.md) is the initial evidence for this rule: verifier-valid work can still be a product failure when the Agent reaches it through an excessively expensive path.

## 21. Major subsystems need a compact Agent-facing projection

A major subsystem must provide a compact, discoverable Agent-facing projection for the common workflows it claims to support. The projection may be text authoring, typed CLI/JSON operations, runtime inspection, or another protocol-independent public contract; it is not required to be a dedicated MCP tool.

The projection should:

- expose semantic intent rather than implementation mechanics,
- keep the common vocabulary small and stable,
- avoid requiring knowledge of internal pipeline stages, parser mechanics, backend types or file-patching hazards unless those concepts are genuinely part of user intent,
- make the production validation path obvious,
- prefer bounded semantic operations over arbitrary text surgery when raw editing repeatedly causes repair/context cost,
- keep lower-level APIs available as escape hatches without making them the mandatory baseline workflow.

New tools are justified only when they remove demonstrated Agent work or provide authority that normal source editing cannot safely provide. Tool count is not itself a product goal.

## 22. Agent convenience must not create a second authority

Agent-facing projections must operate on, resolve into, or validate against the same canonical authoritative state used by the engine.

An Agent convenience layer must not create a parallel `AgentScene`, `AgentParticle`, `AgentResource`, hidden task database or other second semantic truth that can diverge from the production parser/runtime/serializer/validator.

Where a typed semantic mutation exists, the preferred flow is:

```text
Agent intent
 -> compact semantic operation
 -> canonical production state
 -> production validation
 -> atomic/transactional commit where applicable
 -> compact result
```

Convenience layers may cache derived discovery metadata or provide projections, but canonical ownership and validation remain with the production subsystem.

## 23. Agent complexity regression is a product regression candidate

A change that preserves runtime correctness but materially increases Agent context, tool usage, exposed concepts/resources or required iterations must be treated as a potential product regression and justified with evidence.

For representative Agent-operated workflows, record the applicable structural and benchmark evidence, including at least:

- input/output tokens when measured,
- tool calls,
- distinct exposed concepts/resources,
- resources touched,
- revisions/iterations,
- visual-feedback calls,
- human interventions,
- engine-native semantic operations used or bypassed when that signal is relevant.

Token counts are model/environment sensitive and are not automatically a universal hard CI threshold. Hard numeric thresholds belong to preregistered benchmark/task contracts. Outside those suites, structural increases should trigger design review rather than silent acceptance.

For every major subsystem beginning with the external game-production foundation (#69 onward), review three questions before completion:

1. Is the engine/runtime architecture correct, deterministic and performant within its contract?
2. What is the minimum subsystem vocabulary and state an Agent must understand for the common workflow?
3. Is the shortest supported Agent-facing path discoverable, canonical and free of unnecessary implementation detail?

Passing runtime tests answers only the first question. Trace2D's AI-first product contract requires all three to be considered.
