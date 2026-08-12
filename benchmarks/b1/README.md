# Benchmark B1 — Sprite / animation / particle authoring

Status: **scored suite frozen; fixture qualification passed; scored cohort is the next gate.**  
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

`benchmarks/b1/verifiers.json` freezes verifier identities, deterministic check sets and authority seams. Fixture qualification has now proven every known-good fixture is accepted and every known-bad fixture is rejected through the frozen dispatch; the result is recorded separately in [`fixture-qualification.json`](fixture-qualification.json), so the frozen suite and registry remain unchanged.

The required ordering is now:

```text
current primary-source refresh                 complete
 -> non-scored Godot Agent qualification       complete
 -> select/pin strongest credible Godot lane   complete
 -> freeze matched B1 tasks + budgets + fixtures   complete
 -> qualify frozen known-good/known-bad verifier dispatch   complete
 -> run scored cohort with same coding agent   NEXT
 -> independent verification / aggregate report
 -> presentation + multimodal/human review evidence
```

The B0 schema, trial isolation, append-only trace rules, retry/exclusion policy and raw metric vocabulary are reused rather than forked. B1 extends only the content task/verifier layer required by #103.

## Fixture qualification result

The frozen dispatch is qualified at source head `557c7edf9ee30fd9dca0cc33379731887e79f29a` without changing the frozen task fixtures or verifier registry.

- Godot: official `4.7.1.stable.official.a13da4feb`; all three known-good fixtures accepted and all three seeded known-bad fixtures rejected in workflow `31651157113` / job `94295573573`.
- Trace2D: the native Sprite parser, animation runtime contract and particle parser/compiler were exercised by six qualification CTests; all six passed in workflow `31651157103` / job `94295573606`.
- No scored B1 run has been observed. Fixture qualification proves verifier discrimination only; it is not a comparative benchmark result.

Machine-readable evidence: [`fixture-qualification.json`](fixture-qualification.json).

## Product-authority boundary

B1 consumes already-public Trace2D production contracts:

- canonical `.sprite.toml` CPU truth for Sprite metadata,
- `SpriteAnimationClip2D` / `SpriteAnimator2D` exact integer-nanosecond animation authority,
- `.trace2d.particle.toml` particle definitions and existing particle analysis/verification surfaces.

The Trace2D animation fixture uses the ordinary public C++ `SpriteAnimationClip2D::Prepare` surface rather than introducing a benchmark-only animation format. Verifier metadata remains benchmark evidence only and never becomes runtime/content authority.

The benchmark is not permission to add a benchmark-shaped production asset model, hidden answer API, benchmark-detection path, normal-frame report maintenance, or a second animation/particle authority.

## Next implementation step

Run the frozen scored B1 cohort only, using the same suite, fixtures, verifier identities, budgets and selected Agent pins. Preserve isolated trial workspaces and append-only raw evidence under the benchmark run path. Do not mutate the frozen suite/verifier registry after observing results, and do not begin #69 until B1 has reviewable multi-run acceptance evidence.
