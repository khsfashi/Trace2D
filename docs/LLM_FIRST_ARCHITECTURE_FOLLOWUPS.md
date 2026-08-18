# LLM-First Architecture Follow-ups

Status: **owner-approved follow-up register; evidence-gated; not automatically promoted into the active core lane**

This document records four architecture directions that should be preserved while Trace2D continues the current B2/core sequence. It is deliberately a design and research register, not authorization to interrupt the active lane or build speculative infrastructure ahead of evidence.

The four directions are:

1. a semantic project/code graph for bounded Agent retrieval,
2. an explicit security/determinism boundary,
3. a hard separation between developer Agent surfaces and shipped runtime authority,
4. a bounded multi-pass renderer foundation before any deferred-renderer decision.

## 1. Scheduling and promotion rules

These follow-ups do not change the current owner-fixed sequence by themselves.

### 1.1 Semantic Project Graph gate

Do not implement or insert a Project Graph into the Trace2D core lane merely because it appears useful.

The first formal review point is **after the frozen TraceResearch R01 six-run pilot and its postmortem are complete**. R01 must remain uncontaminated by a new retrieval intervention so that typed mutation effects can be separated from retrieval effects.

After the R01 postmortem:

- inspect measured input-token cost, rereads, file discovery, tool calls, revisions, validation calls and failure classes,
- determine whether repository/project re-navigation is a material recurring cost,
- freeze a separate comparison if justified,
- compare ordinary repository navigation against compiler-backed indexing and a Trace2D semantic graph under the same task/Agent/verifier constraints,
- promote a graph feature into Trace2D only after repeated measured benefit without verified-outcome regression.

R03 Failure Trace Shrinking remains a separate research question: Project Graph reduces discovery context, while R03 reduces repair/failure context.

### 1.2 Security boundary gate

The security/determinism rules in this document are architectural constraints now. Larger implementation work such as new fuzzing programs, privilege separation or release-surface stripping should be promoted through dedicated measured issues rather than silently added to unrelated features.

Any future Project Graph, Agent endpoint, networking, importer or plugin work must apply these constraints from its first design checkpoint.

### 1.3 Renderer gate

The current renderer remains a small unlit forward-like 2D presentation path. Do not add a classic deferred renderer merely for feature parity with large 3D engines.

At #89 Material2D/Shader2D, preserve a **bounded internal multi-pass seam** so later PostEffect2D/Light2D work does not require replacing the renderer architecture. This does not authorize a public arbitrary render graph.

A deferred-like path is considered only after a representative 2D workload demonstrates that the bandwidth, attachment, alpha/painter-order and complexity tradeoffs are justified.

## 2. Semantic Project Graph / Project Index

### 2.1 Product model

Do not frame the feature as an "LLM RAG code graph". RAG is one consumer. The engine/tooling-owned product primitive is a deterministic semantic index or project model.

Candidate product names:

- `Trace2D Semantic Project Index`,
- `Trace2D Project Graph`.

The intended product statement is:

> LLMs do not need to read a Trace2D project as a bag of files; they query a deterministic semantic project model.

### 2.2 Scope

A plain C++ call graph is not the differentiation. Compiler-backed code intelligence should be combined with Trace2D-owned semantic state.

Candidate node/domain vocabulary includes:

- code symbols, definitions, references, calls and inheritance,
- Scene and scene templates,
- Entity/component types and authored component instances,
- Resource and asset identities,
- InputAction,
- Camera/UI semantics,
- Material/Shader resources when available,
- tests and verifiers,
- WorkResult/Workspace evidence,
- persistence/schema identities when available.

Example relationship:

```text
Component: PlayerHealth
 -> defined-by PlayerHealth.cpp
 -> instantiated-in Scene: combat_room
 -> read-by DamageSystem
 -> observed-by HealthHUD
 -> persisted-by SaveSchema: player.v2
 -> verified-by combat-health verifier
 -> exercised-by benchmark task
```

### 2.3 Authority and implementation boundary

Compiler-backed semantics should be preferred for C/C++ authority. Clang/SCIP-style indexing is a candidate implementation source; Tree-sitter-style syntax parsing may be useful as a fallback or syntax aid but should not become the authoritative C++ semantic model for templates, overloads, macros and compile-command-sensitive meaning.

The graph must not run in the gameplay frame loop.

Ownership direction:

```text
Trace2D engine/repository
  owns semantic IDs, schemas and query contracts

Trace2D tooling/project layer
  builds and incrementally updates code/project indexes

Gameplay runtime
  consumes only normal resolved runtime state
  does not parse repositories, build embeddings or scan source trees
```

Possible generated state may live under a tooling/cache namespace such as:

```text
.trace2d/index/code.scip
.trace2d/index/project.db
.trace2d/index/project.graph.meta
```

Exact formats are not frozen here.

### 2.4 Incremental invalidation

Index work should be incremental and setup/background-tooling only. Candidate invalidation inputs include:

- source content digest,
- compile-command digest,
- Trace2D manifest revision,
- authored resource digest,
- semantic schema/index version.

Unchanged translation units and unchanged semantic resources should reuse cached index state.

### 2.5 Bounded Agent query surface

Never hand the complete graph to an Agent by default.

The public Agent-facing surface should be bounded and semantic, for example conceptually:

```text
project.symbol("PlayerHealth")
project.neighborhood(symbol, depth=2, edgeKinds=[...])
project.impact(symbol)
project.path(from, to)
project.context(task, byteOrTokenBudget)
```

`context()` should use deterministic traversal/ranking plus a hard output budget. An uncontrolled LLM-generated repository summary is not the authority layer.

Performance direction:

- symbol lookup: indexed/hash or logarithmic lookup,
- neighborhood/path queries: bounded by visited nodes/edges,
- no requirement to load the entire graph into memory for one query,
- no normal-frame graph construction or query cost.

### 2.6 Project Graph security rules

The index is a privileged developer tool and therefore must have an explicit security filter.

It must not intentionally index or expose:

- `.env` secrets,
- credentials/tokens,
- private keys,
- OS credential stores,
- Codex/GitHub/local authentication state,
- raw private evidence,
- files outside the authorized workspace/project roots.

Required design properties include:

- canonical workspace-root enforcement,
- path normalization before access,
- no path traversal escape,
- bounded query depth/result size,
- explicit treatment of generated/private files,
- secret-bearing diagnostics/evidence excluded from graph payloads,
- read/write capabilities separated if graph-assisted mutation is ever added.

## 3. Security / Determinism Boundary

Trace2D determinism is an observability, verification and simulation property. It is **not** an authentication, authorization or sandbox guarantee.

A more precise architectural rule is:

> Deterministic where reproducibility is authority. Unpredictable where unpredictability is security.

### 3.1 Deterministic domains

Strong determinism remains desirable for:

- authoritative local gameplay simulation where Trace2D owns the semantics,
- deterministic tie-break ordering,
- replay and exact failure reproduction,
- save/schema migration rules,
- structural verification and Agent assertions,
- explicit simulation RNG whose seed is part of the reproducible contract.

### 3.2 Security-sensitive domains

Security-sensitive values must not be derived from deterministic simulation RNG or replay seeds.

Examples include:

- authentication/session tokens,
- nonces/challenges,
- capability IDs where unpredictability matters,
- secret keys and key material,
- anti-replay/security protocol randomness.

A future API should make misuse difficult by separating concepts such as:

```text
SimulationRng
SecurityRandom
```

rather than exposing one generic random source for both purposes.

### 3.3 Determinism does not imply authority

A deterministic client is not automatically an authoritative client. If Trace2D later supports networking, authority/validation must be defined by the concrete network model. A modified client state is not made trustworthy because its simulation is reproducible.

Server-authoritative deterministic simulation may be valuable for replay, rollback and cheat investigation, but authentication/authorization remain separate concerns.

### 3.4 Untrusted input boundary

Project, scene, asset, import and external structured data are input to validate, not trusted code merely because parsing is deterministic.

Future parser/importer hardening should consider:

- malformed-input regression tests,
- bounded counts/sizes/depth,
- overflow-safe size calculations,
- path traversal rejection,
- allocation budgets for untrusted files,
- fuzzing where the parser surface justifies it,
- ASan/UBSan or equivalent sanitizer evidence where practical.

## 4. Developer Agent Surface vs Shipped Runtime Surface

Trace2D's Agent-first observability can become an attack surface if development authority is shipped or remotely exposed without a deliberate boundary.

Treat these as different capability classes:

```text
Developer / Agent surface
  inspection
  semantic project queries
  authoring/mutation tools
  deterministic verifier internals
  capture/diagnostic evidence

Shipped runtime surface
  only capabilities required by the game/runtime contract
```

A release build must not automatically expose an MCP/Agent endpoint, arbitrary project mutation, unrestricted component inspection, repository scanning or verifier internals.

Possible implementation mechanisms may include build-time feature sets or explicit capability registration; exact macros/API names are not frozen here.

The important invariant is semantic: **developer tooling authority is not shipped-game authority**.

## 5. Bounded Multi-Pass Renderer Foundation

### 5.1 Current baseline

Trace2D currently has an unlit forward-like 2D path: ordered Sprite/particle work is rendered to an offscreen color target and then presented/copied. This is a valid minimal baseline and should remain the zero-extra-feature cost path.

### 5.2 Required direction at #89

#89 Material2D/Shader2D should preserve a small renderer-owned seam that allows a later frame to contain more than one internal render phase without replacing Sprite submission or exposing raw backend APIs.

The intended direction is conceptually:

```text
Scene
 -> optional Lighting
 -> optional PostProcess
 -> transparent/particles as required by the proven composition contract
 -> UI/presentation
 -> Present
```

The exact phase ordering is not frozen until relevant features exist.

A small internal `FramePlan`/phase vocabulary is acceptable. A user-authored arbitrary render DAG is not required.

### 5.3 Why not classic deferred first

Trace2D's likely workloads are dominated by Sprite alpha, painter order, particles, UI and relatively simple 2D lighting. A classic deferred G-buffer introduces attachment memory/bandwidth and complicates transparency/layer composition.

Therefore:

- do not add deferred rendering for parity alone,
- keep painter-order semantics authoritative for transparent 2D composition,
- prefer workload-driven 2D light accumulation, occlusion/shadow masks, normal/light textures and postprocess passes,
- measure attachment memory, bandwidth, pass count, pipeline switches, draw calls and GPU/CPU cost before promoting more complex paths.

### 5.4 Likely evolution

A plausible evidence-driven sequence is:

```text
current single scene color path
 -> #89 Material2D/Shader2D + bounded multipass seam
 -> RenderTarget2D/PostEffect2D when justified
 -> #93 Light2D/normal/occlusion/shadow accumulation
 -> representative workload measurement
 -> deferred-like path only if measured need remains
```

Optional features should not impose their pass/attachment cost when absent.

## 6. Agent-complexity rule

All four directions must preserve Trace2D's Agent-complexity discipline.

Do not expose infrastructure complexity merely because the renderer/index/security implementation needs it internally.

Examples:

- Agent asks for `Light2D`; engine decides internal passes.
- Agent asks for a semantic neighborhood; engine/tool decides index traversal.
- Agent uses a typed authoring transaction; it does not manage filesystem capability mechanics.
- Agent inspects security-relevant diagnostics only through explicitly authorized/scrubbed surfaces.

The preferred public surface remains small, typed, discoverable and bounded.

## 7. Decision summary

| Direction | Decision now | Promotion/evidence gate |
| --- | --- | --- |
| Semantic Project Graph | Preserve design direction; no implementation yet | TraceResearch R01 six-run pilot + postmortem, then separate held-out comparison |
| Security/determinism | Architectural rule applies now | Dedicated hardening issues as concrete surfaces appear |
| Dev vs shipping surface | Preserve explicit capability boundary | Enforce feature-by-feature; release hardening when runtime packaging surface exists |
| Bounded multi-pass | Reserve seam at #89; no public render graph | PostFX/Light2D workloads and GPU evidence |
| Deferred-like rendering | Do not implement for parity | Representative workload proves benefit over simpler 2D multipass path |

## 8. Non-goals

This follow-up register does not authorize:

- interruption of the active B2/core lane,
- speculative repository-wide embedding generation,
- a generic public graph database API,
- a public arbitrary render graph,
- a classic PBR/deferred renderer without workload evidence,
- shipping developer MCP/Agent authority by default,
- treating deterministic simulation as a security sandbox,
- a broad security framework unrelated to demonstrated Trace2D surfaces.
