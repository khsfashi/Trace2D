# Benchmark B2 scored execution freeze

This directory now contains the neutral starting workspaces and execution
integrity contract required before Benchmark B2 slot 1 can run.

`execution-v1.json` was frozen from `main`
`80edeef32d593d02979f7dea86690fc857a115ed` while both existing scoring-gate
records still stated that no scored B2 result had been observed.

## What is frozen here

- `godot.generic` and `godot.agent` copy the exact same byte-identical ordinary
  Godot starter project before the Agent begins.
- `trace2d.agent` copies a neutral external Trace2D project scaffold using the
  installed-package/public-project contract.
- every starter file is pinned by SHA-256,
- the already-preregistered nine-slot order is repeated exactly,
- isolated per-slot workspaces and append-only hash-chained raw records are
  mandatory,
- zero retries, zero replacement trials, no early stopping and no best-of-N are
  preserved,
- deterministic gameplay verdict remains owned by the already-qualified
  independent verifier,
- presentation evidence remains required but cannot repair deterministic
  failure,
- the single shared lane-agnostic human feedback event remains gated on initial
  deterministic acceptance and requires deterministic re-verification.

## What is deliberately not in the starters

The starters contain no player/enemy implementation, no frozen semantic input
actions, no cooldown rule, no benchmark detector, no expected-result file and no
verifier-owned state mutation hook. The scored Agent must construct the task
through each lane's normal public workflow.

The execution freeze validator rejects both starter-byte drift and accidental
seeding of the frozen task vocabulary into a starter.

## Integrity boundary

This freeze adds execution plumbing only. It does **not** change
`preregistration-v1.json`, `scored-cohort-v1.json`,
`verifier-qualification-v1.json`, the frozen task prompt, the selected Godot
Agent pin, budgets, retry policy or verifier authority.

After this freeze is merged green, the next B2 action is to implement/use the
owner-local scored runner against these exact starters and execute slot 1
(`godot.generic`) first. Once scored execution starts, starter or schedule drift
must be treated as benchmark-integrity failure rather than repaired by rerolling.
