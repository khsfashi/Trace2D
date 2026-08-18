# Trace2D Product Direction

Status: **owner-approved product rule**

Trace2D is not competing to expose the largest editor API, the most MCP tools, or the broadest clone of a mature game engine.

The product claim to prove is narrower:

> **A coding Agent should be able to author, run, observe, verify and revise a 2D game through a compact semantic surface with low discovery, context and revision cost.**

Short loop:

```text
Author -> Run -> Observe -> Verify -> Revise
```

The judgment rule remains:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

## Product moat

Trace2D should optimize these together:

1. **compact semantic authoring** — typed/transactional operations over canonical state instead of forcing low-level representation edits;
2. **deterministic execution** — explicit fixed-step control and stable semantic identity;
3. **cheap structured observation** — inspect engine-owned facts directly instead of inferring them from screenshots;
4. **replayable verification** — exact inputs, frames, assertions and bounded failure evidence;
5. **human-visible closure** — a real playable result, targeted owner feedback and a verified revision loop.

A feature is not automatically valuable because it increases engine breadth. A tool is not automatically valuable because it exposes another internal operation.

## Agent Complexity is a product metric

Measure at least:

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

Do not solve context pressure primarily by increasing prompts/budgets, adding benchmark-shaped shortcuts, proliferating narrow MCP tools, or returning complete project/resource state by default.

Prefer fewer discoverable concepts, bounded outputs, typed mutations and cached/resolved setup state.

## Product lane vs research lane

Trace2D keeps benchmark evidence, but benchmark machinery must not become the product.

The product lane owns runtime/public Agent contracts, external game authoring, deterministic verification, WorkResult/Workspace review, real playable proofs and general usability fixes justified by retained evidence.

Repeated matched experiments, retrieval comparisons and benchmark methodology belong in benchmark/TraceResearch work where possible. Research may justify a product change, but a successor benchmark is not itself a product milestone.

## Benchmark closure rule

A frozen cohort remains immutable even when it fails. Post-score acceptance work may diagnose and repair **general** product/execution surfaces, but must not become rerun-until-success.

After a consumed acceptance cohort fails:

1. preserve it unchanged;
2. perform bounded read-only diagnosis when useful;
3. fix only a demonstrated general product/execution defect;
4. create a new append-only acceptance version only when the fix materially changes the blocked layer and the owner explicitly decides another proof is worth the cost;
5. otherwise close the benchmark with failed acceptance preserved as part of the conclusion.

Provider capacity, transport instability or other external execution failure is valid evidence. It does not create an obligation to keep adding acceptance versions until one happens to pass.

## B2 conclusion

B2 scored execution is complete and immutable. Acceptance-v5 is also consumed and must not be rerun.

The read-only V5 diagnostic (`32123441180`) showed the common failure clearly:

- both Agent subprocesses timed out after 885 seconds,
- both produced no valid final Agent result or retained Codex event stream,
- both were correctly classified as non-authoritative infrastructure `agent_result_failure`,
- deterministic verification and presentation review were correctly skipped,
- both workspaces nevertheless contain partial authoring side effects.

Those partial workspaces are evidence, not valid V5 candidates. They must not be promoted, retroactively verified as accepted turns, or used to reinterpret the consumed cohort.

The diagnostic did not demonstrate a Trace2D gameplay-verifier or presentation defect, and it did not demonstrate a general Trace2D-owned fix that would justify an automatic V6. Therefore B2 may close with:

- scored benchmark complete,
- post-score remediations preserved,
- final full-loop acceptance **not proven**,
- the Agent execution/result timeout explicitly recorded.

Failure to prove the full loop is a benchmark result, not a reason to distort the benchmark.

## Playable proof before breadth

After B2 closes, the core lane moves to **#315 — tiny external playable product proof** before #89 Material2D/Shader2D.

This checkpoint intentionally uses capabilities Trace2D already has. It should not wait for Physics2D, Audio, Save, Mesh or every mature-engine subsystem.

The question is:

> Can a human give a small game intent, receive something genuinely playable, review it, request one concrete change, and receive a deterministically re-verified revision without the Agent getting lost in the engine?

Only blockers demonstrated by that proof should interrupt it with new general engine work. #12 remains the later broad flagship proof.

## Breadth policy

For mature-engine capabilities such as materials, physics, audio and persistence:

- own the Trace2D semantic/deterministic contract;
- prefer proven backends where they satisfy the contract instead of reimplementing solved infrastructure for its own sake;
- keep backend details out of gameplay and Agent-facing semantics;
- avoid normal-frame parsing, filesystem discovery, repeated string lookup or duplicate authority models;
- require measured evidence before broad abstractions such as generic render graphs.

## Semantic Project Index

#312 remains research-gated. Do not build a generic "RAG graph" merely because it sounds AI-native.

First prove repository/project rediscovery remains a material cost. If promoted, prefer compiler-backed C/C++ semantics plus Trace2D-owned project/resource relationships and bounded deterministic retrieval. Repository indexing or embedding work must not enter the gameplay runtime.

## Decision test for future work

Before adding a subsystem, Agent tool or abstraction, ask:

1. Does a current playable/product workflow need it?
2. Is the blocker demonstrated by retained evidence?
3. Does it reduce Agent concepts/context/revisions or merely expose more surface?
4. Can a proven backend satisfy the need behind a Trace2D-owned contract?
5. Can the result be verified without screenshot inference where the engine owns the truth?
6. Will this make the next real external game easier to author and approve?

If the answer is mostly no, postpone the work.
