# Trace2D autonomous benchmark

Tracking issue: #100. Product umbrella: #96.

Trace2D should measure its central product claim rather than rely on statements such as "AI-first" or "agent-friendly" without evidence.

The benchmark asks:

> **Given the same eligible 2D game task and the same coding agent, does Trace2D improve autonomous completion and verifiability while reducing repair cost, visual guessing and human intervention?**

## 1. Primary comparison

The initial matched comparison is:

```text
A. Godot + generic coding tools
B. Godot + pinned reviewed Godot MCP/agent bridge
C. Trace2D + public Agent/CLI/MCP-compatible surface
```

The benchmark is not a claim that Trace2D currently has feature parity with Godot. Capability eligibility and autonomous-operability results are separate dimensions.

A task that requires a subsystem Trace2D has not implemented is **not eligible** for the Trace2D autonomy comparison yet. It is recorded as a capability gap, not counted as an autonomous coding failure.

## 2. Why compare all three

`Godot + generic tools` measures the ordinary code/file/shell workflow.

`Godot + Godot-MCP` measures how much an agent bridge over a mature editor/runtime improves the workflow.

`Trace2D` measures whether designing the engine itself around deterministic stepping, semantic observation and machine-verifiable state provides additional value beyond attaching an agent adapter to an existing engine.

## 3. Fairness controls

Each matched trial should pin or record as much of the following as the environment allows:

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
- OS/CPU/GPU/driver/backend metadata when relevant.

Do not give Trace2D a task-specific hidden command, sample-only shortcut or privileged internal API that a normal external user would not receive.

If an environment fundamentally requires a different tool shape, document the difference instead of pretending the interfaces are identical.

## 4. Primary metrics

At minimum record per trial:

### Completion

- success / partial / fail,
- reason for final failure,
- deterministic acceptance coverage,
- unresolved subjective review items.

### Iteration cost

- implementation/revision cycles,
- build/run failure count,
- diagnose/repair loops,
- total tool calls.

### Agent cost

- input tokens,
- output tokens,
- total tokens,
- tokens per successful task,
- context compaction/restart count when measurable.

### Visual/perceptual dependence

- screenshot/capture calls,
- video/presentation-review calls,
- multimodal model calls,
- cases where visual inference was used for a fact available as structured state.

### Human dependence

- human intervention count,
- intervention type,
- whether intervention was required for objective correctness or only final subjective judgment,
- successful tasks per required intervention.

### Runtime/build evidence

- wall-clock completion time with environment metadata,
- optional build/run/profile timing when comparable,
- failures due to tool/environment setup.

Timing is environment-labelled evidence, not a universal portable truth.

## 5. Evaluation layers

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

## 6. Benchmark task ladder

Grow the benchmark together with engine capability.

### B0 — foundational micro tasks

Examples:

- create/load a scene and move an entity,
- create semantic UI and interact with it,
- import a sprite/texture and produce a capture,
- create a deterministic particle effect,
- verify a fixed-frame gameplay interaction.

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

## 7. Trial record

A committed machine-readable record should eventually capture fields equivalent to:

```text
trial_id
benchmark_task_version
environment
engine_commit
adapter_commit
agent_model
agent_configuration
start_commit
end_commit
success
acceptance_results
iterations
tool_calls
token_usage
visual_feedback_calls
multimodal_calls
human_interventions
elapsed_time
failure_reason
artifacts
```

The exact schema is implementation work for #100.

## 8. Aggregate reporting

Report both per-task and aggregate evidence.

Useful aggregate views include:

- autonomous success rate,
- success rate among capability-eligible tasks,
- median revisions per success,
- median tokens per success,
- median tool calls per success,
- visual-feedback calls per success,
- human interventions per success,
- deterministic acceptance coverage,
- failure taxonomy.

A single score may be derived for visualization later, but raw metrics remain available and authoritative.

## 9. Statistical/repetition policy

LLM behavior is stochastic even when Trace2D runtime state is deterministic.

Therefore one trial is not enough for comparative claims.

For published claims:

- use multiple independent runs per task/configuration,
- record seeds/settings exposed by the agent provider when available,
- report sample count,
- report uncertainty/distribution rather than only one best run,
- do not discard failed trials without a predefined exclusion rule.

The exact repetition count depends on cost and benchmark maturity, but published claims must make the sample size explicit.

## 10. Benchmark integrity

Avoid contamination and overfitting:

- separate benchmark fixtures from implementation examples where practical,
- do not copy expected solutions into agent instructions,
- keep hidden acceptance details only when the harness can preserve them fairly across environments,
- publish enough task/spec information for independent reproduction,
- version tasks when acceptance criteria change.

## 11. External benchmark inspiration

Trace2D may learn from public work such as game-development agent benchmarks and Godot agent bridges, but the committed Trace2D benchmark must remain independently reproducible and pin external dependencies instead of relying on mutable latest versions.

External projects are references, not runtime dependencies by default. License/dependency review is required before vendoring or redistribution.

## 12. Success criterion

The benchmark is successful when it can make a statement of the following form with committed evidence:

```text
On N capability-eligible matched 2D tasks using agent/model X,
Trace2D achieved Y% autonomous completion,
required Z% fewer visual-feedback calls,
and required W% fewer human interventions
than the recorded baselines.
```

Until such evidence exists, the README may describe the goal and benchmark program but must not advertise unmeasured superiority.