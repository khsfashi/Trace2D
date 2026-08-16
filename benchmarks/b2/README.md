# Benchmark B2 — top-down combat micro-game

B2 is the first coherent autonomous micro-game comparison. It is deliberately
separate from the frozen B1 content-authoring cohort.

## Integrity order

B2 uses a two-stage pre-score freeze:

1. **P0 task/policy freeze** — freeze the held-out task, token/tool/time budget,
   verifier authority, retry/exclusion policy, human-feedback rule, B1 identity
   anchor, and the exact strongest-baseline candidates.
2. **P1 baseline qualification/final freeze** — run a non-scored capability
   fixture, select the strongest credible normal Godot Agent lane, pin its exact
   identity/dependencies, and only then create the scored cohort schedule.

No scored B2 result may be observed while `preregistration-v1.json` has
`scoring_gate.allowed = false`.

The P0 freeze intentionally does **not** select a Godot Agent from README
claims. Selection requires Trace2D-owned non-scored qualification evidence.

## Frozen task

`b2-topdown-combat-v1` asks the same coding Agent to build a one-room playable
combat slice with movement, a normal attack path, one enemy, health/damage/death,
sprite animation, hit/death particles, a small HUD, deterministic gameplay
acceptance, and presentation evidence.

The task prompt is frozen at
`tasks/b2-topdown-combat-v1/PROMPT.md`. Later baseline qualification may not
change task membership or task semantics.

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
`preregistration-v1.json`. The B2 preregistration validator refuses a repository
where that tree identity has changed.
