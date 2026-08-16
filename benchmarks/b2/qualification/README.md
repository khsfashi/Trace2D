# B2 qualification fixtures

These files are **not scored B2 results**. They exist to qualify comparison
bridges and lane-specific independent verifiers before any scored B2 execution.

## Baseline bridge qualification

`gameplay_loop_fixture` is intentionally smaller and semantically distinct from
the frozen top-down combat task. It proves a generic coding-agent feedback loop:
ordinary project files, launch, semantic input, structured runtime observation,
presentation capture, bounded candidate verification, clean teardown, and a
Trace2D-owned independent verifier that accepts the known-good fixture and
rejects a generated known-bad variant.

The scored prompt under `benchmarks/b2/tasks/` must never be supplied to that
baseline qualification driver.

## Trace2D lane verifier qualification

`trace2d_verifier` qualifies the independent deterministic verifier used for the
Trace2D B2 lane. The verifier, not the candidate, owns `Application`, schedules
ordinary physical input through `Application::ScheduleInput`, advances fixed
steps through `StepFrames`, and observes gameplay through `AgentFacade` plus the
public UI automation surface.

The committed qualification pair intentionally differs by one meaningful rule:

- `known_good` implements the frozen six-fixed-step attack cooldown.
- `known_bad_cooldown` implements five steps. It must fail when the verifier
  attempts the frozen early second attack on frame 14.

The same verifier also checks player/enemy semantic identity, initial positions
and HP, exact eight-step movement, first-hit damage, lethal post-cooldown attack,
single death transition, unchanged player HP, HUD convergence, and deterministic
hit/death presentation-hook events.

The presentation-hook counters are deterministic evidence that gameplay emitted
the required hit/death events. They do **not** replace the separately required
particle/presentation capture and perceptual review authorities during scored B2.

Verifier qualification is non-scored evidence and does not open the scoring gate
by itself. `benchmarks/b2/preregistration-v1.json` and
`benchmarks/b2/scored-cohort-v1.json` remain authoritative for the frozen task,
schedule, lane identities, and remaining pre-score blockers.
