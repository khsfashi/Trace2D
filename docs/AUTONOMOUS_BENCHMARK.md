# Trace2D autonomous benchmark

Tracking issue: #100. Product umbrella: #96.

Trace2D should measure its central product claim rather than rely on statements such as "AI-first" or "agent-friendly" without evidence.

The benchmark asks:

> **Given the same eligible 2D game task and the same coding agent, does Trace2D improve autonomous completion and verifiability while reducing repair cost, visual guessing and human intervention?**

This document defines the target evaluation contract. It does not require the engine to be "finished" before benchmarking begins; #102/#103/#104 grow the benchmark as capabilities become eligible.

External methodological references are maintained in `docs/REFERENCE_PROJECTS.md`. Every benchmark implementation/change must also follow `docs/EXTERNAL_REFERENCE_PROTOCOL.md` and refresh current primary sources before freezing the harness design.

## 1. Primary comparison

The initial matched comparison is:

```text
A. Godot + generic coding tools
B. Godot + pinned reviewed Godot MCP/agent bridge
C. Trace2D + public Agent/CLI/MCP-compatible surface
```

The benchmark is not a claim that Trace2D currently has feature parity with Godot. Capability eligibility and autonomous-operability results are separate dimensions.

A task that requires a subsystem Trace2D has not implemented is **not eligible** for the Trace2D autonomy comparison yet. It is recorded as a capability gap, not counted as an autonomous coding failure.

## 2. What is actually being compared

The benchmark compares **model + agent harness + engine/tool environment**, not a model in isolation.

Hold the coding agent/model constant as far as the provider/environment permits, then vary the development environment:

```text
same task/spec
same provided assets
same agent/model/configuration
same budget policy
same success verifier

        ↓ vary only the environment/harness lane

Godot generic | Godot + reviewed bridge | Trace2D
```

Harness-Bench, Claw-SWE-Bench and ADK Arena all reinforce that harness/framework choice can materially alter observed agent performance. Therefore every published result must identify both the model/agent configuration and the engine/adapter/harness configuration.

Do not attribute a result to "the model" when the measured unit is the complete model+harness configuration.

## 3. Why compare all three

`Godot + generic tools` measures the ordinary code/file/shell workflow.

`Godot + Godot-MCP` measures how much an agent bridge over a mature editor/runtime improves the workflow.

`Trace2D` measures whether designing the engine itself around deterministic stepping, semantic observation and machine-verifiable state provides additional value beyond attaching an agent adapter to an existing engine.

A future additional environment such as Unity + reviewed Agent bridge may be useful as a secondary control, but it is not required for B0 and must not delay the initial matched three-way experiment.

## 4. Harness architecture

The benchmark harness stays outside Trace2D gameplay authority.

Conceptually:

```text
┌──────────────────────────────────────────────────────────┐
│ Versioned Task / Run Manifest                            │
│ task, assets, budgets, model, adapter, engine, env       │
└──────────────────────────┬───────────────────────────────┘
                           │
                           v
┌──────────────────────────────────────────────────────────┐
│ Isolated Agent Workspace                                 │
│ agent edits/builds/runs/inspects/interacts/verifies      │
└──────────────────────────┬───────────────────────────────┘
                           │ append-only observations/actions
                           v
┌──────────────────────────────────────────────────────────┐
│ Trial Trace / Artifact Package                           │
│ tool calls, patches, builds, runs, structured state,     │
│ captures, token/resource usage, action/replay evidence   │
└──────────────────────────┬───────────────────────────────┘
                           │ candidate result
                           v
┌──────────────────────────────────────────────────────────┐
│ Independent Verifier                                     │
│ hidden/public acceptance tests as appropriate,           │
│ semantic scenarios, replay validation, visual checks     │
└──────────────────────────┬───────────────────────────────┘
                           │
                           v
┌──────────────────────────────────────────────────────────┐
│ Machine-readable Verdict / Aggregate Report              │
└──────────────────────────────────────────────────────────┘
```

The Agent may self-verify during development, but its declaration that work is complete is never the final benchmark verdict.

### 4.1 Agent adapter

Owns only the integration required to run the selected coding agent/model under the benchmark's tool/budget contract.

It must not contain task-specific solution logic.

### 4.2 Environment adapter

Normalizes launching/resetting/collecting evidence from each compared development environment while preserving the environment's native public capabilities.

It must not secretly give Trace2D privileged internal actions that an external Trace2D user would not have.

### 4.3 Independent verifier

The verifier owns final objective acceptance. It must be runnable without trusting the candidate Agent's narrative of what it changed or what allegedly passed.

Where possible the same conceptual acceptance criteria are translated into environment-appropriate checks. Any unavoidable verifier asymmetry is documented.

### 4.4 Reporter

Aggregation/reporting consumes trial records and verifier results. It does not mutate candidate results or silently discard failed runs.

## 5. Trial isolation and environment identity

Each trial starts from a known clean workspace/project revision and must not inherit successful artifacts, hidden state or caches from another candidate run except explicitly allowed immutable dependency/build caches.

Record/pin as much of the following as the environment allows:

- exact task/spec version,
- starting repository/project commit,
- provided assets/fixtures,
- coding agent/model/version,
- reasoning configuration,
- system/repository instructions,
- context/token budget,
- available tools and permissions,
- retry/revision policy,
- maximum tool calls/iterations/time where bounded,
- benchmark harness/adapter version,
- engine/tool/plugin commits,
- compiler/toolchain/build configuration where material,
- OS/CPU/RAM/GPU/driver/backend metadata where relevant,
- network/service policy,
- environment variables that affect behavior,
- exclusion/retry reason if infrastructure failure occurs.

SWE-bench-style isolated task environments and cached immutable environment layers are useful references. Caching may improve setup time, but no task result/state may leak across trials.

## 6. Determinism contract

LLM/Agent behavior may remain stochastic. Trace2D benchmark reproducibility does **not** require the model to emit the same trajectory twice.

The stronger and more useful contract is:

```text
same execution identity
+ same initial authoritative state
+ same Trace2D/runtime seed(s)
+ same exact ordered external actions/inputs

→ same authoritative outcome within the claimed determinism domain
```

This separates:

```text
Agent stochasticity
from
engine/environment nondeterminism
```

A seed alone is not sufficient evidence when code, compiler, generator version or uncontrolled external state changes. FoundationDB and Dropbox Nucleus/Trinity are important precedents here.

## 7. Authoritative replay artifact

When the compared environment supports it, preserve replayable execution evidence separately from visual artifacts.

The preferred Trace2D conceptual form follows Box2D/TAS-style principles:

```text
ReplayHeader
- schema/version
- task/run identity
- engine/build/environment fingerprint
- initial authoritative state or state fingerprint
- deterministic seed domains

OrderedOperations
- frame/time identity
- input/action/tool-induced runtime operation
- required arguments
- authoritative state hash/checkpoint where useful

OptionalKeyframes
- generated for bounded seek/review speed
- not required every simulation step

PresentationArtifacts
- screenshots/video/audio references
- not the authoritative replay itself
```

Exact binary/text encoding is #102/later implementation work. The contract is that **video is evidence of presentation, not the only record of what the simulation did**.

## 8. Harness self-determinism / replay validation

Do not assume a replay is valid because it has a seed.

For Trace2D deterministic domains, the harness should be able to take a recorded execution trajectory and replay it without the original stochastic Agent, then compare the authoritative checkpoints/final state.

Conceptually:

```text
Agent run
  ↓ records exact actions
Replay without Agent
  ↓
authoritative hashes/state
  == original authoritative hashes/state
```

A mismatch is a determinism/replay defect or an uncontrolled-input boundary, not an Agent task failure.

Dropbox Trinity's practice of rerunning the same randomized test and checking the same final state is a direct methodological precedent.

## 9. Benchmark task/verifier self-validation

The benchmark itself can be wrong. Therefore task/verifier quality must be tested before candidate results are trusted.

### 9.1 Known-good / oracle validation

Each task should have at least one known-good implementation/result capable of passing the verifier from a clean environment.

For publication-quality tasks, repeat the known-good run enough times to expose flaky setup/verifier behavior. Frontier-Bench's repeated oracle-run policy is a useful precedent.

### 9.2 Known-bad validation

Keep deliberately incorrect fixtures/variants for important acceptance dimensions when practical, for example:

```text
player moves at wrong speed
score does not increment
collected object is not removed
animation event fires one frame late
particle capacity exceeds the task budget
UI displays stale value
```

The verifier must reject them.

Mutation-testing systems such as Mull provide the general lesson: tests/verifiers should demonstrate that they detect plausible semantic defects, not merely that the happy path passes.

### 9.3 Infrastructure failure classification

A broken environment, provider outage, GPU-driver failure or harness bug is not silently scored as ordinary implementation failure.

Infrastructure exclusions/retries require a predefined policy and remain visible in raw records. Inspect AI's separation of runtime errors, retries and scored outcomes is a useful model.

## 10. Fairness controls

Do not give Trace2D a task-specific hidden command, sample-only shortcut or privileged internal API that a normal external user would not receive.

If an environment fundamentally requires a different tool shape, document the difference instead of pretending the interfaces are identical.

Fairness rules include:

- same user-level goal and acceptance criteria,
- same starting assets/information unless the environment inherently supplies documented public knowledge,
- comparable time/tool/token limits,
- no benchmark detection or input-specific engine optimization,
- pinned adapters/engines/plugins for recorded claims,
- public description of material differences in available capabilities,
- capability-ineligible tasks excluded before observing which environment would have won,
- no cherry-picking favorable seeds/runs,
- no hidden post-hoc retry policy that differs by environment.

MLPerf-style "no benchmark-specific behavior" and reproducibility discipline is a methodological reference even though Trace2D is not an MLPerf benchmark.

## 11. Primary metrics

At minimum record per trial.

### 11.1 Completion

- success / partial / fail,
- independent verifier result,
- reason for final failure,
- deterministic acceptance coverage,
- unresolved subjective review items,
- capability eligibility.

### 11.2 Iteration cost

- implementation/revision cycles,
- build count,
- runtime launch count,
- build/run failure count,
- diagnose/repair loops,
- total tool calls,
- time/tool calls/tokens to first independently verified success where measurable.

### 11.3 Agent cost

- input tokens,
- output tokens,
- total tokens,
- tokens per successful task,
- monetary/API cost when provider reporting is reliable and comparable,
- context compaction/restart count when measurable.

### 11.4 Observation / verification dependence

- structured inspect/query calls,
- deterministic assertion/verify calls,
- screenshot/capture calls,
- video/presentation-review calls,
- multimodal model calls,
- cases where visual inference was used for a fact available as structured state.

### 11.5 Human dependence

- human intervention count,
- intervention type,
- whether intervention was required for objective correctness, environment access/hardware, or only final subjective judgment,
- successful tasks per required intervention.

### 11.6 Runtime/build evidence

- wall-clock completion time with environment metadata,
- CPU time/peak memory when measured reliably,
- optional build/run/profile timing when comparable,
- failures due to tool/environment setup.

Timing is environment-labelled evidence, not a universal portable truth. BenchExec-style process/resource measurement is a useful reference for serious timing/resource claims.

## 12. Verification escalation cost

In addition to raw counts, retain enough trace information to understand **how expensive a fact was to verify**.

Conceptual levels:

```text
0 structured inspect/query
1 deterministic headless scenario
2 deterministic assertion/replay check
3 render/capture
4 multimodal/perceptual review
5 human intervention
```

This is not necessarily a single weighted score. Raw counts remain authoritative. The purpose is to test Trace2D's claim that expensive visual/human escalation is unnecessary for many engine-owned facts.

## 13. Evaluation layers

Do not collapse all evaluation into one AI score.

### Layer 1 — deterministic / structured

Use engine-owned facts wherever possible:

- expected entities/components/state,
- exact-frame events,
- input/action outcomes,
- animation state,
- particle semantics,
- UI semantic state/layout assertions,
- collision/physics queries,
- resource validity/memory evidence,
- structural performance budgets,
- save/migration checks.

### Layer 2 — interactive / presentation evidence

Use replay/capture/gameplay artifacts when the task requires presentation or interaction evidence.

### Layer 3 — multimodal / human review

Use perceptual review only for requirements that are genuinely visual/auditory/subjective. Keep automated multimodal scores/findings separate from final human judgment.

A multimodal grader does not overrule failed deterministic acceptance.

## 14. Benchmark task ladder

Grow the benchmark together with engine capability.

### B0 — foundational micro tasks

Examples:

- create/load a scene and move an entity,
- create semantic UI and interact with it,
- import a texture and produce a capture,
- create a deterministic particle effect,
- verify a fixed-frame gameplay interaction.

B0's job is also to prove the harness, trial schema, adapters, independent verifier and self-validation flow.

### B1 — content tasks

After Sprite production work is available:

- import/generate a sprite animation,
- correct pivot/trim/frame issues,
- create an attack animation with an exact event,
- create a particle effect under an explicit budget,
- produce deterministic and perceptual review evidence.

### B2 — autonomous micro-game

A deliberately small top-down combat slice, for example:

```text
one room
player movement
one attack
one enemy
health/damage/death
sprite animation
hit particle
small HUD
```

The goal is to test the closed loop early, not to wait until a full RPG engine exists.

### B3 — production-system slices

As P8 capabilities land:

- TileMap + collision,
- menu/HUD + pointer/gamepad navigation,
- Physics2D interactions,
- audio,
- persistence,
- project/package flow.

### B4 — flagship mini-game / genre slice

Only after the required capabilities exist, run larger tasks such as a small RPG/roguelike/platformer slice.

JAMER/JamBench is an important reminder that project-scale architectural coherence can degrade sharply even when small tasks compile. Later suites should therefore increase not only feature breadth but project dependency depth/scale.

## 15. Task taxonomy and suite size

As the suite grows, classify tasks by at least:

- subsystem/domain,
- required capabilities,
- project scale,
- interaction horizon,
- deterministic vs perceptual acceptance mix,
- difficulty/failure class.

A small calibrated/challenging subset may be maintained for cheap iteration, with a broader suite for publishable claims. Claw-SWE-Bench and OmniaBench provide useful examples of keeping lower-cost subsets while preserving a larger evaluation corpus.

Do not tune Trace2D only against the cheap subset.

## 16. Trial record

A committed machine-readable record should eventually capture fields equivalent to:

```text
trial_schema_version
trial_id
benchmark_task_version
suite_version
capability_eligibility

environment_id
engine_version_or_commit
adapter_version_or_commit
start_commit
end_commit_or_artifact_hash
build/toolchain fingerprint
OS / CPU / RAM / GPU / driver / backend metadata

agent_name
agent_model
agent_version
agent_configuration
reasoning configuration
budget configuration

success
verifier_version
acceptance_results
iterations
repair_loops
build_count
runtime_launch_count
tool_calls
structured_observation_calls
deterministic_verify_calls
visual_feedback_calls
multimodal_calls
human_interventions
token_usage
API cost when reliable
wall_time
CPU_time / peak_memory when reliably measured

engine/runtime seeds
ordered action/replay artifact
state/checkpoint hashes
presentation artifacts
trajectory/tool trace

infrastructure_error
retry/exclusion reason
failure_reason
known limitations
```

The exact schema is implementation work for #102. Fields that a provider/environment cannot supply are explicit `unavailable`/missing data, not fabricated estimates.

## 17. Append-only trace philosophy

A final score without a process trace is insufficient for diagnosing why one environment won.

Where feasible preserve:

- Agent messages/decisions available through the public harness,
- tool calls and normalized tool outcomes,
- code/authored-data patches,
- build/run events,
- structured observations,
- screenshots/captures,
- token/resource accounting,
- verifier/scorer outputs.

Do not require exposure of private model chain-of-thought. The trace contains externally observable execution artifacts/events, not hidden reasoning.

Inspect AI and Exgentic are useful references for machine-readable traces and post-run analysis.

## 18. Aggregate reporting

Report both per-task and aggregate evidence.

Useful aggregate views include:

- autonomous success rate,
- success rate among capability-eligible tasks,
- paired per-task win/loss/tie outcomes,
- median/distribution of revisions per success,
- median/distribution of tokens per success,
- median/distribution of tool calls per success,
- success-vs-iteration curve,
- success-vs-token/cost curve,
- visual-feedback calls per success,
- structured verification calls per success,
- human interventions per success,
- deterministic acceptance coverage,
- failure taxonomy,
- replay/self-determinism failure count,
- infrastructure error/exclusion count.

A single score may be derived for visualization later, but raw metrics remain available and authoritative. Do not hide important tradeoffs in one weighted number.

## 19. Statistical/repetition policy

LLM behavior is stochastic even when Trace2D runtime state is deterministic.

Therefore one trial is not enough for comparative claims.

For published claims:

- use multiple independent runs per task/configuration,
- record seeds/settings exposed by the agent provider when available,
- report sample count,
- report distributions/uncertainty rather than only one best run,
- preserve paired task structure where useful,
- do not discard failed trials without a predefined exclusion rule,
- do not stop a configuration early merely because early results are unfavorable unless the same preregistered rule applies to all environments.

The exact repetition count depends on cost and benchmark maturity, but published claims must make the sample size explicit.

## 20. Benchmark integrity and contamination

Avoid contamination and overfitting:

- separate benchmark fixtures from implementation examples where practical,
- do not copy expected solutions into Agent instructions,
- keep hidden acceptance details only when the harness can preserve them fairly across environments,
- publish enough task/spec information for independent reproduction,
- version tasks when acceptance criteria change,
- keep held-out variants where practical for mature suites,
- ensure benchmark adapters do not inspect hidden verifier data,
- scan for benchmark-specific engine hacks when results become important.

Meta-Agent Challenge provides a strong example of separating development feedback from held-out verification under explicit budgets.

## 21. Benchmark-to-product feedback

Benchmark failures are not just leaderboard numbers. Classify them into actionable categories such as:

```text
missing engine capability
unclear public API/authoring contract
insufficient Agent observability
weak diagnostic
verification gap
visual-only dependency
adapter/harness failure
model reasoning failure
performance/resource problem
flaky task/verifier
```

Repeated failure classes may become evidence for future work, including #106 verified recipe/skill knowledge.

However:

- a benchmark result does not silently promote a future subsystem ahead of the owner-fixed order,
- one anecdotal failure does not justify new generic infrastructure,
- the smallest owning future stage should absorb validated lessons.

AgentOmnia's evaluation-failure-to-targeted-product-improvement loop is a useful conceptual reference for this evidence feedback process.

## 22. External source pinning and license review

External benchmark bridges/tools are references until actual integration begins.

Before B0 runs against an external Godot bridge:

- choose the exact repository/project,
- review current behavior and maintenance state,
- review license and redistribution obligations,
- pin an exact version/commit,
- record installation/configuration steps,
- avoid silently following mutable `latest` behavior in published results.

If an external checkout can remain a local optional prerequisite instead of a redistributed dependency, prefer that simpler ownership model where appropriate.

## 23. Success criterion

The benchmark is successful when it can make a statement of the following form with committed independently verifiable evidence:

```text
On N capability-eligible matched 2D tasks using agent/model X
under disclosed harness/budget/environment configuration,
Trace2D achieved Y% autonomous completion,
required Z% fewer visual-feedback calls,
and required W% fewer human interventions
than the recorded baselines,
with replay/verifier/task self-validation evidence attached.
```

Until such evidence exists, the README may describe the goal and benchmark program but must not advertise unmeasured superiority.
