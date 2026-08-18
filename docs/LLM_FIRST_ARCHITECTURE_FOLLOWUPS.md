# LLM-First Architecture Follow-ups

Status: **owner-approved follow-up register; evidence-gated; not automatically promoted into the active core lane**

This document preserves architecture directions that matter to Trace2D without authorizing speculative implementation ahead of product evidence.

## Scheduling rule

The active product lane is authoritative. These follow-ups do not interrupt it by themselves.

A follow-up is promoted only when retained product/research evidence shows a real recurring cost or risk and the owner explicitly changes the lane.

## Semantic Project Graph / Project Index

#312 remains `BLOCKED / RESEARCH-GATED` until the frozen TraceResearch R01 pilot and postmortem are complete.

Do not implement a generic "LLM RAG code graph" merely because retrieval appears useful. If R01 shows material repository/project rediscovery cost, compare ordinary navigation, compiler-backed indexing and compiler-backed indexing plus Trace2D semantic relationships under held-out tasks.

If promoted, the preferred model is:

```text
compiler-backed code semantics
+
Trace2D project/resource semantics
```

Useful domains may include code symbols, Scene/template/entity/component identities, resources/assets, InputAction, UI/camera/material identities, tests/verifiers, WorkResult evidence and persistence schema identities.

Agent-facing queries must be bounded; never return the entire graph by default. Indexing is developer/setup tooling and must not run in the gameplay frame loop.

## Security / determinism boundary

Trace2D determinism is an observability, verification and simulation property. It is not authentication, authorization or sandboxing.

> **Deterministic where reproducibility is authority. Unpredictable where unpredictability is security.**

Simulation RNG/replay seeds must remain separate from security-sensitive randomness. A deterministic client is not automatically a trustworthy or authoritative client.

Future Agent indexes, importers, networking and tooling must enforce their own security boundaries rather than inheriting trust from deterministic behavior.

## Developer Agent surface vs shipped runtime

Agent inspection, project mutation, semantic indexing, verifier internals and private diagnostic evidence are privileged developer capabilities.

A packaged game runtime must not automatically expose them. Developer tooling authority and shipped-game authority remain separate capability classes.

## Semantic-index security

Future project indexes/graphs must enforce authorized workspace roots, normalized paths, traversal rejection and bounded query size/depth. They must not intentionally index or expose `.env` credentials, authentication tokens, private keys, OS credential stores, Codex/GitHub/local auth state, raw private evidence or files outside authorized roots.

## Renderer evolution

Do not add a classic deferred renderer or public arbitrary render graph for parity with large 3D engines.

At #89, preserve only a bounded renderer-owned multi-pass seam so later Light2D/PostEffect2D work can compose without replacing Sprite submission. Optional features should not impose extra pass/attachment cost when absent.

A deferred-like path requires separate representative 2D workload evidence covering attachment memory/bandwidth, alpha/painter-order composition, pass count, pipeline switches and measured CPU/GPU cost.

## Agent-complexity rule

Infrastructure complexity should stay internal. Public Agent surfaces should remain small, typed, discoverable and bounded.

Examples:

- Agent asks for a semantic mutation; Trace2D owns parser/serializer mechanics.
- Agent asks for a semantic neighborhood; tooling owns index traversal.
- Agent asks for lighting/material intent; renderer owns internal pass scheduling.
- Agent should not manage filesystem capability mechanics or receive whole internal graphs by default.

## Decision summary

| Direction | Decision now | Promotion gate |
| --- | --- | --- |
| Semantic Project Graph | Preserve direction; no implementation | TraceResearch R01 + postmortem + held-out comparison |
| Security/determinism | Architectural boundary applies now | Concrete surface-specific hardening |
| Dev vs shipped runtime | Keep capability separation | Enforce feature-by-feature |
| Bounded multi-pass | Reserve small seam at #89 | Later PostFX/Light2D workload evidence |
| Deferred-like rendering | Do not implement for parity | Representative workload proves benefit |

These follow-ups must not displace #315's playable product proof or turn Trace2D back into an infrastructure-first project.
