# Agent Tool Surface Audit — P0 V1

Scope: MCP primitives used by the first progressive Agent Skills. This audit treats tools as public APIs: selection clarity, responsibility, input contract, result economy, failure semantics, retry behavior, progressive disclosure, and deterministic verification.

## Decision

**KEEP the current small primitive set. Do not add one MCP tool per Skill.**

`McpServer::tools/list` currently exposes orthogonal state/query/UI/input/step/assert primitives with bounded JSON Schemas and structured results. V1 workflow knowledge belongs in lazy Skill bodies. The packaged Skill registry/CLI supplies descriptor-first discovery without adding runtime or frame work.

## Held input movement

Relevant primitives:

- `trace2d.input.schedule` — one responsibility: enqueue explicit press/release at a future simulation frame.
- `trace2d.input.inspect` — inspect held/pressed/released state for one control.
- `trace2d.runtime.step` — bounded explicit frame advance.
- `trace2d.query` / `trace2d.assert_float` — inspect/verify resulting canonical game state.

Assessment: names and schemas distinguish scheduling, observation, stepping, and assertion. No movement macro-tool is justified.

## Semantic menu flow

Relevant primitives:

- `trace2d.ui.query` — semantic selection by stable id/role/name.
- `trace2d.ui.activate` — one semantic activation operation.
- `trace2d.ui.assert` — structured retained UI verification including activation count.
- `trace2d.runtime.step`, `trace2d.query`, `trace2d.assert_float` — verify downstream game state.

Assessment: semantic selectors avoid coordinate guessing; mutation and verification are separate and retry intent remains visible. No `menu.transition` recipe tool is justified.

## Combat hit feedback

Current general MCP primitives can verify canonical scene/gameplay state (`trace2d.query`, `trace2d.assert_float`) but do not expose a generic audio mutation/inspection tool. V1 does **not** invent one solely to make this Skill symmetrical: C++ public audio contracts already expose structured command results/events, and #424 requires evidence before tool proliferation.

Remaining bottleneck to measure: whether real Agent combat dogfood repeatedly pays material cost for audio state access. If yes, add the smallest orthogonal audio inspect/command primitive with structured failure semantics; do not add `combat.hit_feedback`.

## Progressive disclosure result

```text
tiny root Agent index
 -> tiny Skill descriptor registry
 -> one selected SKILL.md
 -> only task-relevant surface metadata
 -> exact public API index/header or MCP tools/list
```

This preserves a bounded/cacheable discovery path and keeps all workflow discovery outside gameplay/runtime hot paths.
