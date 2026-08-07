# ADR-0001: Keep the agent facade protocol independent

- Status: Accepted
- Date: 2026-08-07

## Context

Trace2D is explicitly designed for coding-agent automation. It would be easy to expose engine functionality directly as MCP tools and let that protocol shape the engine API.

That would make the runtime dependent on a specific agent ecosystem, encourage a large one-tool-per-function surface, and make deterministic gameplay automation harder to test independently of the protocol adapter.

The engine also needs to be usable from normal command-line scripts and CI even when no LLM client is present.

## Decision

Trace2D will implement a protocol-independent automation/agent facade over runtime capabilities.

The intended layering is:

```text
CLI -----------+
JSON transport +--> Agent Facade --> Runtime
MCP adapter ---+
```

The core vocabulary should remain small and composable, centered on operations such as:

```text
run
inspect
query
input
step
assert
capture
test
```

MCP will be implemented only as an adapter over already-tested engine capabilities.

No runtime behavior may exist exclusively in the MCP layer.

## Consequences

Positive:

- engine automation remains usable without an LLM client
- CLI and CI can test the same capabilities as agent adapters
- MCP can evolve or be replaced without redesigning runtime internals
- machine-facing operations can be versioned and validated independently
- fewer, more composable tools reduce agent/tool-surface complexity

Costs:

- an additional facade/adapter boundary must be maintained
- protocol-specific convenience functions may require composition rather than direct engine calls
- transport serialization cannot leak into core runtime types

## Alternatives considered

### MCP as the primary engine API

Rejected because it couples engine architecture to one protocol and makes non-MCP automation a secondary concern.

### Large direct tool surface mirroring engine functions

Rejected because it creates a fragile machine-facing API and encourages agents to depend on implementation details rather than stable gameplay operations.
