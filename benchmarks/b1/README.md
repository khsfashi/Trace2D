# Benchmark B1 — Sprite / animation / particle authoring

Status: **baseline qualification active; scored task freeze is intentionally blocked.**  
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

## Current gate

`baseline-qualification.json` is the machine-readable preregistration for the B1 strongest-baseline refresh.

No `benchmarks/b1/suite.json` may be committed while the manifest remains in
`baseline_qualification` with `scored_suite_allowed = false`.

That ordering is deliberate:

```text
current primary-source refresh
 -> non-scored Godot Agent capability qualification
 -> selected/pinned strongest credible Godot lane
 -> freeze matched B1 tasks + budgets + known-good/known-bad fixtures
 -> run scored cohort with the same pinned coding agent
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

Qualify the reviewed Godot Agent candidates on one non-scored content fixture. Record the selected candidate and exact pin before adding the scored B1 suite or looking at comparative results.
