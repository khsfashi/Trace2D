# B2 non-scored acceptance loop

This directory contains post-score acceptance evidence for Trace2D issue #104.
It is deliberately outside the immutable nine-slot B2 scored cohort.

## Integrity boundary

The frozen scored cohort is historical evidence and is never rerun, replaced,
rescored, or rewritten. Post-score acceptance versions may add new product
acceptance requirements, but they may not reinterpret the score denominator or
repair a historical scored result.

Acceptance versions are append-only evidence layers:

- `contract-v1.json` / `ember-hall-v1` are consumed historical acceptance
  evidence. The diagnostic captures revealed that deterministic evidence plus
  “some retained image” was not sufficient to prove a game-like autonomous
  result.
- `contract-v2.json` / `ember-hall-v2` add the **playable presentation gate**.
  V2 was frozen and synthetically qualified before its first candidate
  execution. V1 and the scored cohort remain immutable.

## Acceptance v2: playable presentation

The already-qualified `b2-topdown-combat-v1` deterministic verifier remains the
gameplay authority. V2 adds separate presentation evidence; presentation can
never override deterministic failure.

The machine presentation gate requires four exact game-render PNGs:

1. `ember-hall-overview.png`
2. `ember-hall-attack.png`
3. `ember-hall-hit.png`
4. `ember-hall-death.png`

The gate independently decodes the PNG bytes and checks minimum render shape,
non-trivial scene composition, dark-room occupancy, HUD-zone contrast, distinct
cyan/blue player, red/magenta enemy and orange/amber environment families, and
visible state changes across the four captures. A flat/debug-like synthetic
known-bad fixture must be rejected while a synthetic game-like fixture must be
accepted before the gate is allowed to run on a candidate.

A candidate that passes deterministic verification and the machine presentation
gate then receives a separate perceptual review. All six rubric items must pass:

- reads as a game screen,
- player and enemy are distinguishable,
- room and spatial hierarchy are clear,
- HUD is legible and non-obstructive,
- combat feedback is readable,
- presentation is not a debug/test-harness view.

## V2 full loop

1. `/b2 accept-v2-start` runs exactly two fresh non-scored initial attempts on
   the isolated v2 durable root. There are zero retries and zero replacements.
2. The lowest-index candidate with valid agent identity, deterministic pass and
   machine presentation pass becomes the review target.
3. ChatGPT reviews all four exact captures and records the six-item rubric with
   `/b2 accept-v2-review <base64-json>`.
4. Human feedback is permitted only after the initial perceptual rubric passes.
5. The owner supplies exactly one real presentation/usability feedback item,
   relayed with `/b2 accept-v2-feedback <base64-utf8>`.
6. The retained workspace receives exactly one AI revision cycle.
7. The independent deterministic verifier and the machine presentation gate
   both run again.
8. The revised four captures receive a final perceptual confirmation through
   `/b2 accept-v2-final-review <base64-json>`.

V2 passes only after every evidence layer succeeds. No presentation or
perceptual result can repair deterministic failure.

## Durable-state isolation

V1 durable state remains under
`%LOCALAPPDATA%\Trace2D\benchmark-b2-acceptance-v1` and is read-only history.
V2 uses a separate root:
`%LOCALAPPDATA%\Trace2D\benchmark-b2-acceptance-v2`.

Neither acceptance version may use the scored `benchmark-b2-scored-v1` root or
create/append scored `raw.jsonl`. Every acceptance-v2 record contains
`scored: false` and explicit guards for both scored and v1 immutability.
