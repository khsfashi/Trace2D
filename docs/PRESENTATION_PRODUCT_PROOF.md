# Presentation Product Proof

This bounded proof runs immediately after #89 Material2D/Shader2D and #90 Tween/Sequence.

## Goal

Prove that the new presentation primitives are sufficient to produce practical reusable 2D game-feel/UI effects through public Trace2D surfaces without adding a new subsystem per effect.

## Representative proof set

At minimum, compose a small external `presentation_playground` with a bounded subset such as:

- hit flash,
- outline,
- dissolve,
- button punch,
- panel slide,
- damage-number pop,
- screen flash,
- one composite hit-impact effect.

Do not turn this checkpoint into a broad recipe-catalog implementation. A few representative recipes are enough to expose whether the primitive layer is adequate.

## Evidence

Retain:

- public authored/project input,
- deterministic/material/tween structural assertions,
- manual-step presentation trajectory where applicable,
- windowed capture/video or frame evidence,
- CPU/GPU/material/tween counters available at this point,
- owner visual verdict,
- any missing-primitive finding.

## Exit rule

- If the representative effects compose cleanly, freeze the lesson and continue to #76 Physics2D.
- If several independent recipes hit the same missing primitive or a measured performance problem, add only that smallest common primitive and repeat the affected proof.
- Do not promote game-specific art direction or one-off visual tricks into engine core.

The official recipe policy is `docs/PRESENTATION_RECIPES.md` and recurring proof policy is `docs/PRODUCT_PROOF_POLICY.md`.
