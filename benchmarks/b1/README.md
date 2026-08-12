# Benchmark B1 — Sprite / animation / particle authoring

Status: **strongest Godot Agent selected; scored-suite freeze gate open.**  
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

The required ordering is now:

```text
current primary-source refresh                 complete
 -> non-scored Godot Agent qualification       complete
 -> select/pin strongest credible Godot lane   complete
 -> freeze matched B1 tasks + budgets + fixtures   NEXT
 -> run scored cohort with same coding agent
 -> independent verification / aggregate report
 -> presentation + multimodal/human review evidence
```

The B0 schema, trial isolation, append-only trace rules, retry/exclusion policy and raw metric vocabulary are reused rather than forked. B1 may extend verifier dispatch for new task kinds, but it must not create Trace2D-only task-specific shortcuts.

## Product-authority boundary

B1 consumes already-public Trace2D production contracts:

- canonical `.sprite.toml` CPU truth for Sprite metadata,
- `SpriteAnimationClip2D` / `SpriteAnimator2D` exact integer-nanosecond animation authority,
- `.trace2d.particle.toml` particle definitions and existing particle analysis/verification surfaces.

The benchmark is not permission to add a benchmark-shaped asset model, hidden answer API, benchmark-detection path, normal-frame report maintenance, or a second animation/particle authority.

## Next implementation step

Freeze `benchmarks/b1/suite.json`, matched lane mappings, budgets, verifier dispatch, and known-good/known-bad fixtures before observing any scored comparative result. The selected `godot.agent` pin may not change after that point without invalidating the cohort.
