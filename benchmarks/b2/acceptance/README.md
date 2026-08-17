# B2 non-scored acceptance loop

This directory contains the post-score acceptance proof for Trace2D issue #104.
It is deliberately outside the immutable nine-slot B2 scored cohort.

## Why this exists

The frozen scored cohort is historical evidence and is never rerun, replaced, or
rewritten. The initial cohort exposed real agent-surface and harness problems.
Those problems were remediated after scoring. This acceptance loop validates the
improved product surface without changing the score.

`contract-v1.json` freezes a new held-out presentation variant before its first
execution. Two independent `trace2d.agent` initial runs provide matched
multi-run evidence. The already-qualified B2 deterministic gameplay verifier is
reused as the gameplay authority.

## Full loop

1. `/b2 accept-start` runs the two non-scored initial attempts on the owner
   self-hosted Windows runner.
2. The first deterministic pass with a retained presentation capture becomes
   the review target.
3. ChatGPT performs an advisory perceptual review of that exact capture and the
   review is recorded with `/b2 accept-review <base64-json>`.
4. The owner supplies one real presentation/usability feedback item.
5. The exact owner feedback is relayed with
   `/b2 accept-feedback <base64-utf8>`.
6. The retained workspace receives one AI revision cycle and the independent
   deterministic verifier runs again.

The loop passes only when the revision changes the retained workspace, keeps a
presentation capture, preserves the immutable scored freeze, and passes
deterministic re-verification.

## Isolation

Durable state lives under
`%LOCALAPPDATA%\Trace2D\benchmark-b2-acceptance-v1`, never under the scored
`benchmark-b2-scored-v1` root. This harness does not create or append
`raw.jsonl`. Every record contains `scored: false`.
