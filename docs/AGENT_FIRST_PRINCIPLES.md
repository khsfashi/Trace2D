# Agent-first design principles

Trace2D is not a general-purpose engine with an AI adapter bolted on later. The runtime, tools, file formats, and test surfaces are designed so that both humans and coding agents can understand and control the same project state.

## 1. Text first

Project configuration, scenes, tests, and metadata should prefer deterministic text formats that are readable, diffable, and safe to edit with normal development tools.

Binary formats may exist for generated caches and packaged runtime data, but they must not be the only source of truth for authored content.

## 2. Protocol-independent agent surface

The engine's automation contract must not depend on MCP or any specific LLM product.

The intended layering is:

```text
Coding Agent
    |
    +-- CLI
    +-- JSON / JSON-RPC
    +-- MCP adapter (later)
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

The runtime will support a fixed simulation step and explicit frame advancement. Given the same project data, initial state, seed, inputs, and frame sequence, deterministic subsystems should produce the same observable result.

## 4. Observable state over visual guessing

Screenshots are useful for visual QA, but agents should not need to infer gameplay state from pixels when the engine already owns that state.

Runtime inspection should expose structured information such as:

- stable entity identity
- semantic name and tags
- transforms and bounds
- component state
- visibility and enabled state
- collision/query results
- active scene and simulation frame

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

Gameplay simulation, queries, inputs, and assertions should work without a visible window whenever rendering is not required.

Windowed and headless modes must share the same runtime logic. Headless mode must not become a separate simplified game implementation.

## 7. Testability is an engine feature

The engine will provide first-class facilities for:

- virtual input
- frame stepping
- state queries
- assertions
- deterministic seeds
- screenshot capture
- machine-readable failure reports

Gameplay automation is part of the runtime architecture, not only an external test harness.

## 8. Small composable commands

Prefer a compact set of orthogonal operations over a large collection of narrow LLM tools.

The target vocabulary is close to:

```text
build
run
inspect
query
input
step
assert
capture
test
```

Higher-level agent tools can compose these primitives.

## 9. Explicit ownership and predictable C++

Engine code should favor clear ownership, RAII, searchable APIs, and explicit lifetime boundaries over clever metaprogramming.

Hot paths will be measured before specialized allocators or lock-free structures are introduced. Optimization decisions should be supported by profiles and benchmarks rather than assumptions.

## 10. Failures must be actionable

Machine-facing commands should return stable exit codes and structured diagnostics. A failed automated test should identify what was expected, what was observed, and enough runtime context to reproduce the failure.
