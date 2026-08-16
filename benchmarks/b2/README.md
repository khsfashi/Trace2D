# Benchmark B2 — top-down combat micro-game

B2 is the first coherent autonomous micro-game comparison. It is deliberately
separate from the frozen B1 content-authoring cohort.

## Integrity order

B2 uses a staged pre-score freeze:

1. **P0 task/policy freeze** — freeze the held-out task, token/tool/time budget,
   verifier authority, retry/exclusion policy, human-feedback rule, B1 identity
   anchor, and the exact strongest-baseline candidates.
2. **P1a baseline qualification** — run a non-scored capability fixture against
   current credible Godot Agent candidates without exposing the scored task.
3. **P1b selection/cohort freeze** — select the strongest qualified normal
   external-user lane, pin its exact source/package/evidence identity, and freeze
   all nine scored slots before any scored result is observed.
4. **P1c verifier qualification** — qualify each lane's independent B2 verifier
   on committed known-good and meaningful known-bad fixtures. Only this step may
   open the scoring gate.

No scored B2 result may be observed while `preregistration-v1.json` has
`scoring_gate.allowed = false`.

## Frozen strongest Godot Agent lane

P1a qualified both `satelliteoflove/godot-mcp@4.1.0` and
`hi-godot/godot-ai==3.1.5` on the generic non-scored gameplay-loop fixture.
Godot AI 3.1.5 is frozen for the scored `godot.agent` lane because its broader
normal scene/script/resource/animation/particle/UI authoring surface is the more
conservative baseline for B2's construction workload. Deterministic gameplay
pass/fail remains owned by the independent B2 verifier, not by the bridge.

The selected source commit, Python wheel SHA-256, qualification workflow/artifact
identity, and the qualifying artifact digest are pinned in
`baseline-candidates.json` and `scored-cohort-v1.json`.

## Frozen scored schedule

`scored-cohort-v1.json` commits exactly nine slots: three repetitions of the one
frozen task across `godot.generic`, `godot.agent`, and `trace2d.agent`. Lane order
rotates so each lane occupies first, second, and third temporal position exactly
once. There are no automatic or replacement retries and no best-of-N selection.

The schedule is frozen but **not yet executable as scored B2**. The only
remaining scoring-gate blocker is lane-specific independent verifier
qualification on known-good and meaningful known-bad fixtures.

## Frozen task

`b2-topdown-combat-v1` asks the same coding Agent to build a one-room playable
combat slice with movement, a normal attack path, one enemy, health/damage/death,
sprite animation, hit/death particles, a small HUD, deterministic gameplay
acceptance, and presentation evidence.

The task prompt is frozen at
`tasks/b2-topdown-combat-v1/PROMPT.md`. Later qualification work may not change
task membership or task semantics.

## Budget rationale

B2 retains B1's `100000` input-token and `20000` output-token ceilings. The
larger coherent task receives more wall time and tool-call headroom because it
must build and exercise several eligible subsystems, but B2 does not hide B1's
context-pressure finding by simply increasing the token budget.

## Human feedback

Initial deterministic acceptance occurs without human repair. After the first
accepted presentation exists, a blinded human review may provide exactly one
shared, lane-agnostic feedback instruction. The same instruction is then applied
to every eligible lane, followed by one recorded Agent revision and a mandatory
deterministic re-verification. Human or multimodal judgment never overrides a
failed deterministic verifier.

## B1 immutability

The B1 benchmark directory tree is anchored by Git tree identity in
`preregistration-v1.json`. The active B2 freeze validator refuses a repository
where that tree identity has changed.
