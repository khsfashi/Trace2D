# External engineering reference and benchmarking protocol

Last reviewed: 2026-08-18.

Trace2D is developed by repeatedly executing a short continuation request such as `@GitHub Trace2D 다음 진행해줘`. That workflow must not turn into isolated reinvention from model memory.

For every non-trivial Trace2D design or implementation task, the coding agent must recover **Trace2D's own durable decision history** and benchmark the active problem against mature public implementations before freezing the design.

The default engineering rule is:

> **Recover before rediscovering. Benchmark before invention. Reuse before rebuild. Adopt evidence, not fashion.**

External projects, papers and engineering posts are references by default, not automatically dependencies. Trace2D remains responsible for its own authority model, determinism, performance, licensing, tests and public API.

`docs/COMMIT_KNOWLEDGE.md` defines the companion Git-native protocol for carrying durable decisions, rejected alternatives, validation boundaries and gates forward between agents. `docs/REFERENCE_PROJECTS.md` is the starting registry for known useful projects, never an exhaustive whitelist.

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
mandatory external benchmark/reference pass
    ↓
ADOPT / ADAPT / REJECT / DEFER decision
    ↓
smallest Trace2D-owned implementation
    ↓
verification + benchmark evidence
    ↓
final squash commit knowledge atom
```

If an active predecessor is waiting on a real hardware or human gate, research may be performed in a separate documentation/governance PR only when it explicitly does not supersede or falsely complete the active implementation.

A reference that suggests an attractive future subsystem does not promote that subsystem ahead of the fixed lane. Future-feature discovery is handled through the owner-proposal rule in this document.

Commit knowledge is historical evidence, not a second source of truth. Current compiling code, tests, live PR state, owner decisions, active issue acceptance criteria and committed subsystem contracts outrank old trailers. Use `Supersedes:` when a substantive final commit intentionally replaces an older durable decision.

## 2. When a benchmark/reference pass is required

A current external benchmark/reference pass is **mandatory before design is frozen for every non-trivial task** involving any meaningful engineering choice, including:

- a new engine/runtime/rendering/input/scene/resource/UI/physics/audio/persistence subsystem,
- a meaningful extension or refactor of an existing subsystem,
- a bug fix whose repair requires choosing ownership, API, data-flow, caching, synchronization or failure semantics,
- determinism, replay, rollback, concurrency, scheduling or randomized testing,
- Agent/MCP/tooling/diagnostic/repair/workspace workflow,
- autonomous benchmark design, task design, scoring, trial isolation or metrics,
- performance measurement, budgets, profiling or optimization claims,
- GPU behavior, conformance or cross-platform/compiler behavior,
- file/asset/project formats or interoperability,
- a new external dependency, vendored source, service, SDK or separately licensed integration,
- an area explicitly listed in `docs/PRODUCTION_GAPS.md`,
- a production claim for which mature engines or systems already have substantial field experience.

Pure typo fixes, formatting-only edits, comment corrections and mechanical repairs with no design choice may reuse the current owning contract without a fresh search. That exception must stay narrow; it must not be used to avoid benchmarking a real design decision.

## 3. Mandatory GitHub benchmarking baseline

For each non-trivial task, the agent must actively look for **mature, well-implemented GitHub precedents relevant to the exact problem**.

The normal minimum is:

1. **at least one direct analogue** — a mature engine/tool/project implementing the same or closely related capability,
2. **at least one lower-layer or production precedent** when applicable — a project that has already solved the hard ownership, determinism, performance, verification or lifecycle problem underneath the feature,
3. the **current primary documentation/specification** when behavior, version, compatibility, license or deployment facts materially affect the decision.

Two to five strong sources are normally enough. Use more only when the area is contested, safety/correctness critical, licensing-sensitive or the benchmark claim needs broader methodology review.

### 3.1 What counts as a strong GitHub precedent

Prefer repositories or implementations that show several of:

- sustained maintenance,
- meaningful real-world adoption,
- established ecosystem or domain-standard status,
- strong automated test/conformance coverage,
- clear production use or maintainer engineering evidence,
- stable and documented APIs/contracts,
- relevant performance or failure-mode evidence,
- direct relevance to the active Trace2D uncertainty.

GitHub stars, forks and name recognition are useful discovery signals, **not proof that an architecture is correct**. A smaller domain-standard implementation with excellent evidence may outrank a famous but weakly relevant repository.

### 3.2 Reuse-before-build gate

Before introducing a new abstraction, subsystem, format or utility, answer these questions explicitly:

```text
Can Trace2D use an existing maintained implementation directly?
Can Trace2D adapt a proven contract/algorithm/data format without importing the whole stack?
What would direct reuse cost in license, dependency weight, runtime cost, build complexity and portability?
If Trace2D still implements its own version, what concrete requirement makes that the better choice?
```

Do not write custom infrastructure merely because custom code is possible.

Conversely, do not add a dependency merely because it exists. Direct code/library reuse requires current license, security, compatibility, build, runtime and maintenance review.

## 4. Search both the direct layer and the lower layer

Do not search only for products that look like Trace2D.

### 4.1 Direct analogues

Examples:

- mature 2D engines implementing the exact subsystem,
- AI-native or agent-operated development tools,
- Godot/Unity/Roblox/other engine Agent or MCP bridges,
- game-development agents and benchmarks,
- content-production/showroom/asset-pipeline tools.

Question:

> How have successful products exposed this capability to developers or agents?

### 4.2 Verification and determinism precedents

Examples:

- deterministic simulation testing,
- replay/TAS systems,
- property-based and stateful testing,
- controlled concurrency testing,
- mutation testing.

Question:

> How can the behavior be made reproducible, observable and falsifiable?

### 4.3 Benchmark and measurement precedents

Examples:

- coding-agent harness benchmarks,
- independent evaluator architectures,
- benchmark isolation/resource measurement,
- reproducibility/fairness rules,
- visual regression methodology.

Question:

> How can a claimed improvement be measured without accidentally measuring the harness, machine or grader incorrectly?

### 4.4 Production engineering precedents

Examples:

- mature engine/runtime APIs,
- operating-system/compiler/vendor documentation,
- established open-source implementations,
- maintainer engineering writeups describing failures and tradeoffs.

Question:

> What edge cases, ownership rules and performance traps have already been learned the hard way?

The lower-layer search is mandatory when it is more informative than another superficially similar AI product. FoundationDB-style deterministic simulation, Box2D replay or BenchExec-style measurement can be more useful to Trace2D than an AI-game demo when the active problem is reproducibility or benchmarking.

## 5. What to compare

Do not stop at feature lists. Compare the dimensions that matter for the active task.

Typical dimensions include:

- API and authority boundaries,
- ownership and lifecycle,
- setup cost versus steady-state hot-path cost,
- allocations, cache behavior and repeated work,
- CPU/GPU work placement,
- deterministic versus nondeterministic state,
- error/failure/transaction semantics,
- serialization and compatibility rules,
- test/conformance strategy,
- cross-platform/toolchain behavior,
- user/agent discoverability,
- operational complexity and dependency weight.

For performance-sensitive work, inspect actual code paths or primary performance evidence when practical rather than inferring performance from a README.

## 6. Source-quality order

Prefer evidence in this order:

1. official specification/documentation and the project's maintained source repository,
2. peer-reviewed or primary research paper / official technical report,
3. maintainer-authored engineering post describing actual implementation or incidents,
4. reputable independent technical analysis,
5. community examples used only for discovery or additional perspective.

For load-bearing architecture, determinism, benchmark or licensing decisions, do not rely on a secondary summary when the primary source is available.

For rapidly changing Agent/MCP/benchmark projects, check the current repository/paper/site at task time instead of trusting a stale description in this document.

## 7. Bounded research and recovery procedure

A normal non-trivial task performs the following before implementation design is considered final.

### Step A — classify the question

Write down the concrete uncertainties the benchmark/reference pass should answer, for example:

```text
How should a replay artifact represent authoritative execution?
How should GPU conformance evidence be separated from portable semantic truth?
How should a benchmark keep the model constant while varying the engine/harness?
How should an imported Sprite preserve trim/pivot identity without per-frame work?
Can an existing asset-pipeline contract be reused instead of creating another database?
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

### Step D — perform a fresh GitHub/current-source search

For every non-trivial task, refresh the active question against current GitHub/public primary sources rather than assuming the registry is still best-in-class.

Prefer a small number of load-bearing sources over dozens of shallow mentions. Record a version/date/commit when implementation details or reproducibility depend on it.

### Step E — extract decisions, not summaries

For each important reference, classify the result as:

```text
ADOPT  — use the proven concept essentially as-is at the contract level
ADAPT  — reuse the proven idea but change it to fit Trace2D authority/performance/product rules
REJECT — the approach conflicts with Trace2D goals, evidence or constraints
DEFER  — useful but not justified by the active task
```

Record the reason. When an existing dependency or implementation could be reused directly, also record why direct reuse was accepted or rejected.

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

Do not duplicate a mature reusable implementation when an adapter or dependency is demonstrably smaller, safer and cheaper under Trace2D's constraints.

### Step H — validate the borrowed lesson

After implementation, verify that the adopted lesson is actually evidenced by Trace2D tests/workloads/measurements rather than merely copied into prose.

When the external implementation offers a useful conformance fixture, workload shape, known failure case or invariant, adapt that evidence where licensing and scope permit.

### Step I — preserve durable knowledge

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
- important reference decisions,
- `Supersedes:` lineage when replacing older decisions.

Do not paste the research transcript or private model reasoning into Git history.

## 8. Required benchmark/reference evidence in PRs

Every non-trivial PR should contain a compact benchmark/reference review. A useful form is:

```text
External benchmark/reference review
- Source: <project/repo/paper + version/date/commit when material>
  Maturity/relevance: <why this is a useful precedent>
  Lesson: <what solved problem is reusable>
  Decision: ADOPT | ADAPT | REJECT | DEFER
  Reuse decision: <direct dependency / adapter / independent implementation + why>
  Trace2D evidence: <contract/test/workload/metric/file>
```

For choices with several credible implementations, use a small comparison matrix instead of a long narrative.

The PR must make it possible to answer:

```text
What mature implementation did we compare against?
What did it already solve?
What did Trace2D reuse or adapt?
What did we deliberately not copy, and why?
How did we verify the resulting Trace2D behavior?
```

Do not turn every PR into a literature survey. Preserve only load-bearing evidence and decisions.

If a source becomes a durable recurring reference for later work, add/update it in `docs/REFERENCE_PROJECTS.md` or an appropriate focused companion document.

## 9. Adjacent feature harvest and owner proposal rule

External benchmarking has a second purpose: discovering **already-proven capabilities that could materially improve Trace2D** even when they are outside the active task.

While reviewing mature references, inspect adjacent capabilities exposed by those projects. A future-feature candidate is worth surfacing when most of these are true:

- the feature is implemented and proven in a maintained project rather than only proposed,
- it solves a real or foreseeable Trace2D user/Agent/production pain point,
- it fits Trace2D's AI-operated product direction and authority model,
- it does not simply duplicate an existing Trace2D capability,
- the likely product value is meaningful relative to implementation/maintenance complexity,
- there is enough evidence to explain why it is useful rather than merely fashionable.

When such a candidate is found, the agent should mention it to the owner **after completing/reporting the active task**, in the user's language, using a concise form equivalent to:

```text
<reference project>'s <proven feature> solves <problem> by <key idea>.
Trace2D could adapt that into <specific Trace2D feature> to gain <expected benefit>.
이 기능을 토대로 Trace2D에 <specific feature>를 만들어보는 건 어때요?
```

Include the strongest source and a short note on expected scope/tradeoff when useful.

### Proposal safety rules

- Suggest only high-confidence, evidence-backed candidates; do not spam speculative ideas.
- Prefer one strong proposal; surface at most three when several are unusually compelling.
- A proposal does **not** change the roadmap, create an issue, add a dependency or begin implementation by itself.
- Wait for explicit owner approval before promoting the idea into project scope.
- Never let optional future-feature discovery delay, replace or falsely complete the active task.
- Do not repeatedly re-propose an idea the owner already rejected unless materially new evidence changes the tradeoff.
- If no external feature is genuinely worth proposing, say nothing rather than manufacturing a suggestion.

## 10. Version, recency and reproducibility rules

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

## 11. Dependency and license boundary

Learning an idea from open source does not automatically authorize copying code, assets or a dependency.

Before vendoring, linking, redistributing, downloading at build time or requiring an external service/tool:

- verify the current license from the authoritative source,
- determine source and binary redistribution obligations,
- pin the version/commit where practical,
- review transitive/runtime implications,
- review security and maintenance implications,
- measure dependency/build/runtime cost where material,
- prefer an adapter or optional local checkout when redistribution is unnecessary,
- update `docs/THIRD_PARTY.md` or the owning license contract when the dependency becomes real.

Conceptual lessons may be reimplemented independently through Trace2D-owned code and tests when appropriate.

## 12. Anti-cargo-cult rules

Do not:

- copy an architecture because a prestigious project uses it,
- equate GitHub popularity with engineering correctness,
- add a dependency only to save a small amount of straightforward code,
- independently rebuild a mature solution without documenting why reuse/adaptation is unsuitable,
- add per-frame allocations/string lookup/filesystem access to imitate a tooling-oriented project,
- make screenshots the authority for state Trace2D can expose structurally,
- claim cross-platform/GPU bit identity merely because one reference achieves determinism in a narrower domain,
- adopt a benchmark's aggregate score while discarding its raw evidence,
- use an LLM grader where an independent deterministic verifier is possible,
- hide failed trials or choose only a favorable seed,
- let benchmark-specific detection or shortcuts leak into the engine,
- treat a paper's reported result as proof that the same technique works in Trace2D,
- treat old commit knowledge as immutable architecture when current contracts supersede it,
- turn reference reviews or commit trailers into boilerplate that repeats the diff instead of preserving otherwise-lost rationale.

## 13. Benchmark-specific research rule

Before implementing or materially changing #100/#102-#104 or any later comparative benchmark, refresh at least these reference classes:

- matched model/harness comparisons,
- isolated independent evaluation,
- deterministic run/replay precedents,
- benchmark self-validation and known-bad/gold fixtures,
- statistical/repetition/fairness guidance,
- game-development/interactive evaluation.

`docs/AUTONOMOUS_BENCHMARK.md` owns the resulting Trace2D contract.

## 14. Determinism/replay-specific research rule

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

## 15. Performance-specific research rule

When a task introduces a performance claim or optimization:

- define the workload before optimizing,
- compare mature implementations/techniques that solve the same workload,
- inspect allocation/caching/lifecycle choices when relevant,
- separate deterministic structural counts from machine timings,
- record hardware/environment for timings,
- control parallelism and resource interference where feasible,
- preserve before/after evidence,
- prefer stable resource reuse/caching when it removes measured repeated work,
- do not promote a benchmark-only fast path into gameplay code.

BenchExec-style measurement discipline and MLPerf-style fairness/replicability rules are useful methodological references even when Trace2D does not depend on those projects.

## 16. Commit-knowledge-specific research rule

`docs/COMMIT_KNOWLEDGE.md` owns Trace2D's durable Git-history protocol.

The current methodological precedents include:

- Git native `interpret-trailers` / `%(trailers:...)`,
- Lore's structured decision-context trailers,
- long-running Git/Linux `Signed-off-by` / `Tested-by` / `Reviewed-by` practices,
- Gerrit `Change-Id` as structured review lifecycle metadata,
- GitHub Copilot coding agent's `Agent-Logs-Url` provenance trailer,
- `git-ai` as a deferred line-level/Git-Notes provenance direction.

Refresh these sources if Trace2D later adds automated commit validation, a history query tool, signed Agent provenance, Git Notes, external session-log links or another persistence mechanism. Do not add such infrastructure merely because it exists elsewhere; the native trailer protocol is the current minimal solution.

## 17. Reference refresh and retirement

`docs/REFERENCE_PROJECTS.md` and focused companion reference documents are reviewed whenever an owning non-trivial task activates and before published comparative claims.

A reference may be retired or downgraded when:

- the project is abandoned or materially changes direction,
- a stronger primary source supersedes it,
- its license/integration assumptions change,
- Trace2D experiments show that the borrowed idea does not help,
- the reference no longer applies to the current architecture.

Preserve significant rejected/deferred lessons when forgetting them would invite repeated rediscovery or architectural churn.

## 18. Success criterion

This protocol succeeds when a fresh coding agent can receive only:

```text
@GitHub Trace2D 다음 진행해줘
```

and, without previous chat history:

1. recover the actual active task and gates,
2. read current Trace2D contracts and query relevant durable commit knowledge,
3. identify the important current design uncertainties,
4. benchmark the problem against mature current GitHub/public precedents,
5. determine whether an existing implementation can be reused directly or adapted,
6. distinguish proven ideas from incompatible or fashionable complexity,
7. absorb useful lessons into Trace2D-owned contracts/tests/metrics,
8. implement and validate one scoped vertical slice,
9. preserve the benchmark/reference decision in the PR and final squash knowledge,
10. optionally surface one evidence-backed adjacent feature proposal when a mature reference reveals a genuinely valuable opportunity,
11. leave enough repository-native evidence that the next agent can understand, query and refresh the decision.

The objective is not to imitate every famous project. It is to make Trace2D accumulate both its **own verified engineering history** and the best compatible external ideas available, while avoiding unnecessary reinvention and preserving a coherent, deterministic, performant and AI-operated engine design.
