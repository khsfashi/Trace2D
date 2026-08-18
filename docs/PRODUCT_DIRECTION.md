# Trace2D Product Direction

Status: **owner-approved product rule**

Trace2D is not competing to expose the largest editor API, the most MCP tools, or the broadest clone of a mature game engine.

The product claim to prove is narrower:

> **A coding Agent should be able to author, run, observe, verify and revise a 2D game through a compact semantic surface with low discovery, context and revision cost.**

Short loop:

```text
Author -> Run -> Observe -> Verify -> Revise
```

The existing judgment rule remains unchanged:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

## 1. Product moat

Trace2D should optimize five properties together:

1. **compact semantic authoring** — typed, transactional operations over canonical engine state instead of making an Agent hand-edit every low-level representation;
2. **deterministic execution** — explicit fixed-step control and stable semantic identity;
3. **cheap structured observation** — inspect engine-owned facts directly instead of inferring them from screenshots;
4. **replayable verification** — exact inputs, frames, assertions and bounded failure evidence;
5. **human-visible product closure** — a real playable result, targeted owner feedback and a verified revision loop.

A feature is not automatically valuable because it increases engine breadth. A tool is not automatically valuable because it exposes another internal operation.

## 2. Agent Complexity is a first-class product metric

B1 demonstrated that valid final workspaces can still be product failures when the Agent spends excessive context rediscovering representation, syntax or validation rules.

For Agent-facing design, measure at least:

- time/tool calls to first correct target discovery,
- files/resources read and rereads,
- input/output tokens,
- raw-text edits versus semantic transactions,
- authored revisions,
- build/run/validation calls,
- visual-feedback calls,
- human interventions,
- bounded evidence bytes,
- verified outcome and failure class.

Do not solve context pressure primarily by:

- increasing prompt size,
- increasing token budgets,
- adding benchmark-shaped special cases,
- exposing a large number of narrow MCP tools,
- returning full project/resource state by default.

Prefer fewer discoverable concepts, bounded outputs, typed mutations and cached/resolved setup state.

## 3. Product lane and research lane

Trace2D keeps benchmark evidence, but benchmark machinery must not become the product.

### Product lane

The product lane owns:

- runtime and public Agent contracts,
- external game authoring,
- deterministic verification,
- WorkResult/Workspace review surfaces,
- real human-playable proofs,
- general usability fixes justified by retained evidence.

### Research lane

Repeated matched experiments, retrieval comparisons and benchmark methodology belong in the benchmark/TraceResearch lane where possible.

Research may justify a product change, but a benchmark successor is not itself a product milestone.

## 4. Benchmark closure rule

A frozen scored cohort remains immutable even when it fails.

Post-score acceptance work may diagnose and repair **general** product or execution surfaces, but it must not become rerun-until-success.

After a consumed acceptance cohort fails:

1. preserve the cohort unchanged;
2. perform read-only diagnosis when the retained evidence can materially distinguish failure domains;
3. fix only a demonstrated general product/execution defect;
4. create a new append-only acceptance version only when the fix materially changes the previously blocked layer and the owner explicitly decides that another proof is worth the cost;
5. otherwise close the benchmark with the failed acceptance result preserved as part of the conclusion.

Provider capacity, transport instability or other external execution failure is valid evidence. It does not create an obligation to keep adding acceptance versions until one happens to succeed.

## 5. B2 closure boundary

B2 scored execution is complete and immutable. Acceptance-v5 is also consumed and must not be rerun.

The exact next B2 work is bounded read-only diagnosis of the retained V5 `agent_result_failure` evidence.

A V6 is justified only if that diagnosis identifies a concrete, general execution/result-layer defect that Trace2D owns or can correct without changing the frozen task/verifier/presentation authorities. If no such defect is demonstrated, B2 closes with:

- scored benchmark complete,
- post-score remediations preserved,
- final full-loop acceptance **not proven**,
- the failure domain explicitly recorded rather than hidden.

Failure to prove the full loop is a benchmark result, not a reason to distort the benchmark.

## 6. Playable proof before breadth

After B2 closes, the core lane moves to **#315 — tiny external playable product proof** before #89 Material2D/Shader2D.

This checkpoint intentionally uses the capabilities Trace2D already has. It should not wait for Physics2D, Audio, Save, Mesh or every mature-engine subsystem.

The purpose is to answer:

> Can a human give a small game intent, receive something genuinely playable, review it, request one concrete change, and receive a deterministically re-verified revision without the Agent getting lost in the engine?

Only blockers demonstrated by that proof should interrupt it with new general engine work.

The later #12 flagship remains the broader production proof after additional subsystems mature.

## 7. Breadth policy

For mature-engine capabilities such as materials, physics, audio and persistence:

- own the Trace2D semantic/deterministic contract;
- prefer proven backends where they satisfy the contract instead of reimplementing solved infrastructure for its own sake;
- keep backend details out of gameplay and Agent-facing semantics;
- avoid normal-frame parsing, filesystem discovery, repeated string lookup or retained duplicate authority models;
- require measured evidence before broad abstractions such as generic render graphs or similarly expensive infrastructure.

## 8. Semantic Project Index

#312 remains research-gated.

Do not build a generic "RAG graph" because it sounds AI-native. First prove that repository/project rediscovery remains a material cost after typed authoring and bounded failure evidence improvements.

If promoted, prefer compiler-backed C/C++ semantics plus Trace2D-owned project/resource relationships and bounded deterministic retrieval. Do not ship repository indexing or embedding work in the gameplay runtime.

## 9. Decision test for future work

Before adding a new subsystem, Agent tool or abstraction, ask:

1. Does a current playable/product workflow need it?
2. Is the blocker demonstrated by retained evidence?
3. Does it reduce Agent concepts/context/revisions or merely expose more surface?
4. Can a proven backend satisfy the runtime need behind a Trace2D-owned contract?
5. Can the result be verified without screenshot inference where the engine owns the truth?
6. Will this make the next real external game easier to author and approve?

If the answer is mostly no, postpone the work.
