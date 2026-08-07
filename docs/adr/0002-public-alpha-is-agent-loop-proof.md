# ADR-0002: Public Alpha proves the agent loop, not engine breadth

- Status: Accepted
- Date: 2026-08-07

## Context

Trace2D has a potentially unbounded engine scope. A project described as an agent-first 2D engine can easily expand into editor tooling, physics completeness, scripting, networking, audio, advanced rendering, job systems, memory frameworks, multiple platforms, and protocol adapters before the core idea is actually demonstrated.

The project's differentiating hypothesis is narrower: a game engine can be designed so coding agents can build, execute, observe, control, test, and visually verify gameplay with the same reliability expected from modern web-development automation.

A first public release should prove that hypothesis before expanding feature breadth.

## Decision

The first public release target is `v0.1.0-alpha.1`.

It is gated by one complete vertical workflow:

```text
text-authored data/code
  -> build
  -> deterministic headless run
  -> explicit frame step
  -> structured inspection/query
  -> virtual input
  -> gameplay assertion
  -> minimal 2D render
  -> visual capture at a known frame
```

A deliberately tiny sample game will demonstrate the workflow.

The following are explicitly not required for Public Alpha:

- MCP adapter
- graphical editor
- full physics/UI feature set
- networking/audio
- job system/custom allocator framework
- advanced rendering
- multi-platform support

The canonical detailed checklist lives in `docs/PUBLIC_RELEASE.md` and the live GitHub release tracking issue.

## Consequences

Positive:

- the project can reach a coherent, demonstrable milestone without becoming a multi-year engine rewrite
- the public story is clear and falsifiable
- architecture is validated by end-to-end use before broad subsystem investment
- later features extend a proven automation contract

Costs:

- Public Alpha will intentionally lack many features associated with established engines
- some viewers may initially interpret the project as a test/runtime framework rather than a broad engine
- documentation must clearly explain the scope and future direction

## Alternatives considered

### Wait until a Godot-like feature set exists

Rejected because feature breadth would dominate development before the core agent-first hypothesis is proven.

### Require MCP before any public release

Rejected because MCP is only one adapter. The underlying automation contract should first prove itself through direct CLI/test surfaces.

### Publish immediately with architecture documents only

Rejected because the portfolio value depends on a working end-to-end demonstration rather than plans alone.
