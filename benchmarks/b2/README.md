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
   on committed known-good and meaningful known-bad fixtures.
5. **Scoring gate** — pin the exact verifier qualification heads, workflow runs,
   artifacts and digests, then open scoring without changing any frozen task,
   lane, schedule, budget, retry or authority field.

No scored B2 result may be observed while `preregistration-v1.json` has
`scoring_gate.allowed = false`. The explicit verifier qualification record is
`verifier-qualification-v1.json`.

## Scoring gate status

The scoring gate is open. No scored B2 result had been observed when the gate
was frozen open.

The Trace2D verifier qualification is pinned to exact head
`12ada922f37fdd4e004a39cdb168dc610b8fdd4c`, workflow run `31938580902`,
artifact `9261437919`, and artifact SHA-256
`5c4bf6b0e253b480ddee5ad2a06836c96943d0b77def251d9481ff02f2707635`.
The same verifier accepted the committed six-step cooldown fixture and rejected
the committed five-step cooldown mutation at the frame-14 early-attack check.

The shared Godot runtime verifier qualification for `godot.generic` and
`godot.agent` is pinned to exact head
`a11592be1da77867e6e6792f97de6dbc21ea6b23`, workflow run `31939889431`,
artifact `9261721033`, and artifact SHA-256
`a7934b2cf2c55894b969d3a429aeb8afbcdb4cca9fe9033328da5e2ebf83c3cb`.
It used official Godot `4.7.1.stable.official.a13da4feb`, accepted the committed
six-step cooldown fixture, and rejected the five-step mutation at the same
frame-14 checkpoint.

Opening the gate does not authorize a reroll, task edit, lane substitution,
budget increase, verifier change, or best-of-N selection. The nine committed
slots remain the only scored cohort.

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

The schedule is now executable as scored B2 because both independent verifier
families have been qualified and their evidence is frozen. Execution must still
follow the exact committed slot order and reporting policy.

## Frozen task

`b2-topdown-combat-v1` asks the same coding Agent to build a one-room playable
combat slice with movement, a normal attack path, one enemy, health/damage/death,
sprite animation, hit/death particles, a small HUD, deterministic gameplay
acceptance, and presentation evidence.

The task prompt is frozen at
`tasks/b2-topdown-combat-v1/PROMPT.md`. Scored work may not change task membership
or task semantics.

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
