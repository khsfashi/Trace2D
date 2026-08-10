# External engineering reference protocol

Last reviewed: 2026-08-10.

Trace2D is developed by repeatedly executing a short continuation request such as `@GitHub Trace2D 다음 진행해줘`. That workflow must not turn into isolated reinvention from model memory. When a substantive subsystem, benchmark, determinism, performance, integration or production-engine task becomes active, the coding agent should recover both **Trace2D's own durable decision history** and proven public work before freezing the design.

This document defines that research-and-adoption discipline. `docs/COMMIT_KNOWLEDGE.md` defines the companion Git-native protocol for carrying durable decisions, rejected alternatives, validation boundaries and gates forward between agents.

The rule is intentionally similar to good open-source engineering practice: understand the problem, inspect what this repository already learned, inspect mature precedents, reuse ideas and techniques where they fit, then implement them through Trace2D-owned contracts rather than cargo-culting another project's architecture.

> **Recover before rediscovering. Research before invention; adopt evidence, not fashion.**

External projects, papers and engineering posts are references by default, not dependencies. Trace2D remains responsible for its own authority model, determinism, performance, licensing, tests and public API.

## 1. Relationship to the core continuation lane

External-reference work never authorizes skipping the owner-fixed execution order.

The continuation priority remains:

```text
live active PR / review / CI / required human or environment gate
    ↓
first incomplete unblocked owner-fixed task
    ↓
current Trace2D contracts + relevant final-main commit knowledge
    ↓
relevant external-reference pass
    ↓
Trace2D design decision
    ↓
implementation + verification
    ↓
final squash commit knowledge atom
```

If an active predecessor is waiting on a real hardware or human gate, research may be performed in a separate documentation/governance PR only when it explicitly does not supersede or falsely complete the active implementation.

A reference that suggests an attractive future subsystem does not promote that subsystem ahead of the fixed lane.

Commit knowledge is historical evidence, not a second source of truth. Current compiling code, tests, live PR state, owner decisions, active issue acceptance criteria and committed subsystem contracts outrank old trailers. Use `Supersedes:` when a substantive final commit intentionally replaces an older durable decision.

## 2. When an external-reference pass is required

Perform a bounded current-reference pass before design is frozen when the active work materially involves one or more of:

- a new engine/runtime/rendering/input/scene/resource/UI/physics/audio/persistence subsystem,
- determinism, replay, rollback, concurrency, scheduling or randomized testing,
- Agent/MCP/tooling/diagnostic/repair/workspace workflow,
- autonomous benchmark design, task design, scoring, trial isolation or metrics,
- performance measurement, budgets, profiling or optimization claims,
- GPU behavior, conformance or cross-platform/compiler behavior,
- file/asset/project formats or interoperability,
- a new external dependency, vendored source, service, SDK or separately licensed integration,
- an area explicitly listed in `docs/PRODUCTION_GAPS.md`,
- a production claim for which mature engines or systems already have substantial field experience.

A fresh internet search is not mandatory for every typo, narrow regression fix or obvious test repair. Those tasks may reuse current repository references when the relevant contract is already well established and there is no material uncertainty.

The commit-knowledge protocol follows the same signal rule: substantive work should query and preserve durable decision context; trivial mechanical changes should not create metadata noise merely to satisfy a checklist.

## 3. Search from both the direct layer and the lower layer

Do not search only for products that look like Trace2D.

For each substantive task, inspect useful precedents from the layers that matter:

### 3.1 Direct analogues

Examples:

- AI-native or agent-operated game engines,
- Godot/Unity/Roblox/other engine Agent or MCP bridges,
- game-development agents and game-development benchmarks,
- mature 2D engine implementations for the exact subsystem.

Question:

> How have others exposed this capability to developers or agents?

### 3.2 Verification and determinism precedents

Examples:

- deterministic simulation testing,
- replay/TAS systems,
- property-based and stateful testing,
- controlled concurrency testing,
- mutation testing.

Question:

> How can the behavior be made reproducible, observable and falsifiable?

### 3.3 Benchmark and measurement precedents

Examples:

- coding-agent harness benchmarks,
- independent evaluator architectures,
- benchmark isolation/resource measurement,
- reproducibility/fairness rules,
- visual regression methodology.

Question:

> How can a claimed improvement be measured without accidentally measuring the harness, machine or grader incorrectly?

### 3.4 Production engineering precedents

Examples:

- mature engine/runtime APIs,
- operating-system/compiler/vendor documentation,
- established open-source implementations,
- maintainer engineering writeups describing failures and tradeoffs.

Question:

> What edge cases, ownership rules and performance traps have already been learned the hard way?

The lower-layer search is mandatory when it is more informative than another superficially similar AI product. FoundationDB-style deterministic simulation, Box2D replay or BenchExec-style measurement can be more useful to Trace2D than an AI-game demo when the active problem is reproducibility or benchmarking.

## 4. Source-quality order

Prefer evidence in this order:

1. official specification/documentation and the project's maintained source repository,
2. peer-reviewed or primary research paper / official technical report,
3. maintainer-authored engineering post describing actual implementation or incidents,
4. reputable independent technical analysis,
5. community examples used only for discovery or additional perspective.

For load-bearing architecture, determinism, benchmark or licensing decisions, do not rely on a secondary summary when the primary source is available.

For rapidly changing Agent/MCP/benchmark projects, check the current repository/paper/site at task time instead of trusting a stale description in this document.

## 5. Bounded research and recovery procedure

A normal substantive task should perform the following before implementation design is considered final.

### Step A — classify the question

Write down the concrete uncertainties the reference pass should answer, for example:

```text
How should a replay artifact represent authoritative execution?
How should GPU conformance evidence be separated from portable semantic truth?
How should a benchmark keep the model constant while varying the engine/harness?
How should an imported Sprite preserve trim/pivot identity without per-frame work?
```

Do not browse broadly without a design question.

### Step B — recover Trace2D's own decision history

Read the current owning contracts first, then query relevant final-main commit knowledge according to `docs/COMMIT_KNOWLEDGE.md` for the affected subsystem/path/issue.

Look especially for:

```text
Decision
Constraint
Rejected
Directive
Not-tested
Gate
Supersedes
```

The purpose is to avoid re-proposing an already rejected design, silently breaking an intentional constraint, or forgetting a previous validation boundary.

Old trailers do not override current contracts. If history and current repository truth disagree, follow the normal source-of-truth hierarchy and determine whether a later decision superseded the old record.

### Step C — inspect the repository registry

Read the applicable sections of `docs/REFERENCE_PROJECTS.md` and any focused companion reference document to recover already-vetted leads and the stage-to-reference map.

The registry is a starting index, never an exhaustive whitelist.

### Step D — search for current primary sources

Search the web/GitHub/paper corpus for current authoritative sources relevant to the active uncertainty. Prefer a small number of load-bearing sources over dozens of shallow mentions.

As a default, two to five strong sources are sufficient for a normal subsystem decision. Use more when the area is contested, safety/correctness critical, licensing-sensitive or the benchmark claim requires broader methodology review.

### Step E — extract decisions, not summaries

For each important reference, classify the result as:

```text
ADOPT  — the idea fits Trace2D essentially as-is at the conceptual level
ADAPT  — the idea is useful but must change to match Trace2D contracts
REJECT — the approach conflicts with Trace2D goals/performance/authority
DEFER  — useful later but not justified by the active task
```

Record the reason.

### Step F — convert adopted ideas into Trace2D-owned evidence

An external pattern is not considered absorbed merely because the PR mentions it.

A useful idea should become one or more of:

- a clear engine/tooling contract,
- a public API boundary,
- deterministic behavior or explicit determinism boundary,
- an automated test or invariant,
- a replay/fixture/artifact format,
- a structural/performance metric,
- a benchmark fairness rule,
- an actionable diagnostic,
- a documented non-goal or rejected architecture.

### Step G — implement the smallest compatible design

Prefer the simplest design that satisfies Trace2D's actual requirement.

Do not add generic ECS/reflection/job systems/render graphs/editor machinery or another project's abstraction stack merely because a reference uses it.

### Step H — validate the borrowed lesson

After implementation, ask whether the adopted lesson is actually evidenced by Trace2D tests/workloads rather than merely copied into prose.

### Step I — preserve the new durable knowledge

For a substantive PR, prepare the final squash knowledge described by `docs/COMMIT_KNOWLEDGE.md` from the **final implementation and final validation state**.

Preserve high-signal facts such as:

- the owning issue/area,
- final non-obvious decisions,
- constraints,
- rejected alternatives worth preventing from being rediscovered,
- directives future modifiers need,
- what actually ran and passed,
- meaningful remaining `Not-tested:` boundaries,
- human/environment/license gate disposition,
- `Supersedes:` lineage when replacing older decisions.

Do not paste the research transcript or private model reasoning into Git history.

## 6. Required PR evidence

For a substantive task that triggered this protocol, the PR description or durable subsystem document should contain a compact external-reference review equivalent to:

```text
External reference review
- Source: <project/paper/post + version/date/commit when material>
  Lesson: <what problem it solves>
  Decision: ADOPT | ADAPT | REJECT | DEFER
  Trace2D evidence: <contract/test/workload/metric/file>
```

Do not turn every PR into a literature survey. The purpose is to preserve why a non-obvious design was chosen and let a future agent refresh the decision when the source changes.

If a source becomes a durable recurring reference for later work, add/update it in `docs/REFERENCE_PROJECTS.md` or an appropriate focused companion document.

A substantive PR should also preserve enough final information to construct the squash commit knowledge atom. A compact block such as the following is sufficient when the facts exist:

```text
Squash commit knowledge
Issue: #...
Area: ...
Decision: ...
Constraint: ...
Rejected: ... | ...
Directive: ...
Tested: ...
Not-tested: ...
Gate: ... | ...
Reference: ... | ADAPT | ...
Related: PR #...
Supersedes: ... | ...
Agent: ...
Model: ...
```

Only high-signal applicable fields should be included. The final squash message must be refreshed after any material code or validation change.

## 7. Version, recency and reproducibility rules

A plain URL to `latest` is not enough when behavior can materially change.

For benchmark integrations and external tools, pin or record the exact version/commit used in a published run.

For architectural reading, record the review date and version/commit when the lesson depends on implementation details.

For deterministic/replay evidence, preserve enough execution identity to distinguish at least:

```text
source revision
build/binary identity
compiler/toolchain where material
runtime/environment identity where material
scenario/task version
engine seed
ordered external inputs/actions
```

A seed by itself is not assumed to reproduce execution across changed code, toolchains or uncontrolled external state.

## 8. Dependency and license boundary

Learning an idea from open source does not automatically authorize copying code, assets or a dependency.

Before vendoring, linking, redistributing, downloading at build time or requiring an external service/tool:

- verify the current license from the authoritative source,
- determine source and binary redistribution obligations,
- pin the version/commit where practical,
- review transitive/runtime implications,
- prefer an adapter or optional local checkout when redistribution is unnecessary,
- update `docs/THIRD_PARTY.md` or the owning license contract when the dependency becomes real.

Conceptual lessons may be reimplemented independently through Trace2D-owned code and tests when appropriate.

## 9. Anti-cargo-cult rules

Do not:

- copy an architecture because a prestigious project uses it,
- add a dependency only to save a small amount of straightforward code,
- add per-frame allocations/string lookup/filesystem access to imitate a tooling-oriented project,
- make screenshots the authority for state Trace2D can expose structurally,
- claim cross-platform/GPU bit identity merely because one reference achieves determinism in a narrower domain,
- adopt a benchmark's aggregate score while discarding its raw evidence,
- use an LLM grader where an independent deterministic verifier is possible,
- hide failed trials or choose only a favorable seed,
- let benchmark-specific detection or shortcuts leak into the engine,
- treat a paper's reported result as proof that the same technique works in Trace2D,
- treat old commit knowledge as immutable architecture when current contracts supersede it,
- turn commit trailers into boilerplate that repeats the diff instead of preserving otherwise-lost rationale.

## 10. Benchmark-specific research rule

Before implementing or materially changing #100/#102-#104, the agent must refresh at least these reference classes:

- matched model/harness comparisons,
- isolated independent evaluation,
- deterministic run/replay precedents,
- benchmark self-validation and known-bad/gold fixtures,
- statistical/repetition/fairness guidance,
- game-development/interactive evaluation.

`docs/AUTONOMOUS_BENCHMARK.md` owns the resulting Trace2D contract.

## 11. Determinism/replay-specific research rule

When work touches deterministic simulation, replay, rollback, random generation or concurrency, explicitly inspect whether every relevant source of nondeterminism is controlled or recorded.

Useful precedent classes include FoundationDB Simulation, Dropbox Nucleus/Trinity, TigerBeetle VOPR, Box2D determinism/replay, TAS tooling, Coyote/Shuttle and property-based testing.

The target distinction is:

```text
Agent/model may be stochastic
        ↓
recorded ordered actions/tool effects
        ↓
claimed-deterministic Trace2D domain
        ↓
reproducible authoritative outcome
```

Do not confuse model stochasticity with environment nondeterminism.

## 12. Performance-specific research rule

When a task introduces a performance claim or optimization:

- define the workload before optimizing,
- separate deterministic structural counts from machine timings,
- record hardware/environment for timings,
- control parallelism and resource interference where feasible,
- preserve before/after evidence,
- prefer stable resource reuse/caching when it removes measured repeated work,
- do not promote a benchmark-only fast path into gameplay code.

BenchExec-style measurement discipline and MLPerf-style fairness/replicability rules are useful methodological references even when Trace2D does not depend on those projects.

## 13. Commit-knowledge-specific research rule

`docs/COMMIT_KNOWLEDGE.md` owns Trace2D's durable Git-history protocol.

The current methodological precedents include:

- Git native `interpret-trailers` / `%(trailers:...)`,
- Lore's structured decision-context trailers,
- long-running Git/Linux `Signed-off-by` / `Tested-by` / `Reviewed-by` practices,
- Gerrit `Change-Id` as structured review lifecycle metadata,
- GitHub Copilot coding agent's `Agent-Logs-Url` provenance trailer,
- `git-ai` as a deferred line-level/Git-Notes provenance direction.

Refresh these sources if Trace2D later adds automated commit validation, a history query tool, signed Agent provenance, Git Notes, external session-log links or another persistence mechanism. Do not add such infrastructure merely because it exists elsewhere; the native trailer protocol is the current minimal solution.

## 14. Reference refresh and retirement

`docs/REFERENCE_PROJECTS.md` and focused companion reference documents are reviewed opportunistically when an owning task activates and before published comparative claims.

A reference may be retired or downgraded when:

- the project is abandoned or materially changes direction,
- a stronger primary source supersedes it,
- its license/integration assumptions change,
- Trace2D experiments show that the borrowed idea does not help,
- the reference no longer applies to the current architecture.

Preserve significant rejected/deferred lessons when forgetting them would invite repeated rediscovery or architectural churn.

## 15. Success criterion

This protocol succeeds when a fresh coding agent can receive only:

```text
@GitHub Trace2D 다음 진행해줘
```

and, without previous chat history:

1. recover the actual active task and gates,
2. read current Trace2D contracts and query relevant durable commit knowledge,
3. recover important prior decisions, constraints, rejected alternatives and validation boundaries,
4. identify the important current design uncertainties,
5. locate the best current external precedents,
6. distinguish proven ideas from incompatible or fashionable complexity,
7. absorb useful lessons into Trace2D-owned contracts/tests/metrics,
8. implement and validate one scoped vertical slice,
9. prepare a factual final squash knowledge atom for the completed work,
10. leave enough repository-native evidence that the next agent can understand, query and refresh the decision.

The objective is not to imitate every project. It is to make Trace2D accumulate both its **own verified engineering history** and the best compatible external ideas available while preserving a coherent, deterministic, performant and AI-operated engine design.