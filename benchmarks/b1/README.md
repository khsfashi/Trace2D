# Benchmark B1 — Sprite / animation / particle authoring

Status: **scored suite frozen; fixture qualification is the next gate.**  
Parent: #100  
Issue: #103  
Predecessors: #59 Complete Sprite and #102 Benchmark B0 are complete.

B1 extends the matched benchmark into content authoring that is expensive to judge from pixels alone:

- import or normalize Sprite content,
- repair trim / pivot / alignment defects,
- author deterministic animation with exact semantic events,
- author or repair particle content under structural/performance constraints,
- retain exact-frame/headless evidence separately from presentation review,
- diagnose and repair at least one seeded content defect.

## Frozen baseline decision

`baseline-qualification.json` is the machine-readable B1 baseline contract.

The preregistered non-scored qualification is complete. `hi-godot/godot-ai` is selected at exact pin `v3.0.6@f3d99dfbd38c9e095edf1467f85bee507ace2c3a`. The comparison evidence and rejection rationale for the other leading candidate are frozen in [`qualification/SELECTION.md`](qualification/SELECTION.md).

## Frozen scored suite

`benchmarks/b1/suite.json` freezes the scored task membership **before any scored comparative run**. The three matched scenarios cover every #103 task class exactly once:

1. `b1-sprite-normalize-repair`
   - Sprite import/normalization,
   - trim/pivot/alignment repair.
2. `b1-animation-exact-event`
   - deterministic animation with exact semantic event timing,
   - exact-frame/headless evidence separated from presentation evidence.
3. `b1-particle-budget-repair`
   - particle structural/performance budget,
   - diagnosis and repair of an intentionally seeded content defect.

Each task keeps the frozen B0 Agent budget unchanged: 300 wall seconds, 80 tool calls, 100k input tokens, 20k output tokens and zero human interventions. `godot.generic` and `godot.agent` receive byte-for-byte identical starter/known-good/known-bad paths and verifier identity; only the interaction adapter differs. `trace2d.agent` receives a semantically matched native Trace2D fixture.

`benchmarks/b1/verifiers.json` freezes verifier identities, deterministic check sets and authority seams. Scored execution is not eligible until the next gate proves every known-good fixture is accepted and every known-bad fixture is rejected through the frozen dispatch; that evidence is recorded separately so the frozen suite and registry do not change.

The required ordering is now:

```text
current primary-source refresh                 complete
 -> non-scored Godot Agent qualification       complete
 -> select/pin strongest credible Godot lane   complete
 -> freeze matched B1 tasks + budgets + fixtures   complete
 -> qualify frozen known-good/known-bad verifier dispatch   NEXT
 -> run scored cohort with same coding agent
 -> independent verification / aggregate report
 -> presentation + multimodal/human review evidence
```

The B0 schema, trial isolation, append-only trace rules, retry/exclusion policy and raw metric vocabulary are reused rather than forked. B1 extends only the content task/verifier layer required by #103.

## Product-authority boundary

B1 consumes already-public Trace2D production contracts:

- canonical `.sprite.toml` CPU truth for Sprite metadata,
- `SpriteAnimationClip2D` / `SpriteAnimator2D` exact integer-nanosecond animation authority,
- `.trace2d.particle.toml` particle definitions and existing particle analysis/verification surfaces.

The Trace2D animation fixture uses the ordinary public C++ `SpriteAnimationClip2D::Prepare` surface rather than introducing a benchmark-only animation format. Verifier metadata remains benchmark evidence only and never becomes runtime/content authority.

The benchmark is not permission to add a benchmark-shaped production asset model, hidden answer API, benchmark-detection path, normal-frame report maintenance, or a second animation/particle authority.

## Next implementation step

Implement and run the frozen verifier qualification only. Every known-good/known-bad pair must prove the intended independent acceptance/rejection behavior on the pinned Godot 4.7.1 and Trace2D production source, then record that result in `benchmarks/b1/fixture-qualification.json`. The frozen suite and verifier registry must not change. Do not run a scored Agent cohort yet.
