# Trace2D Asset Studio

Tracking issue: #318. Existing review surface: #99 / `docs/WORKSPACE.md`.

## Product role

Asset Studio is the long-term Trace2D-owned human-facing product for AI-operated game-asset production.

It is not a separate TracePixel app and it is not a Unity/Aseprite-style mandatory manual editor.

Target eventual human loop:

```text
Request -> Compare candidates/evidence -> Give feedback / choose -> Approve
```

The product direction remains valid, but its deeper library/provider model is intentionally **not** being completed while Trace2D's production capability set is still Sprite-heavy.

## Existing authority to reuse

Asset Studio composes existing production contracts rather than creating parallel truth.

- Sprite S1 / SA / SR own canonical Sprite, animation and rendering semantics.
- SPP0-SPP5 own Sprite processing/import/provider-neutral generation orchestration.
- #97 owns project-visible intent / Definition of Done.
- #98 owns WorkResult, verification, diagnosis, revision and artifact evidence.
- #99 Workspace owns human review, feedback and approval presentation.
- #178/#179 own transactional Sprite/Particle authoring.
- Later Material/Tween/Physics/Audio/etc. add production primitives that the eventual Studio should compose rather than anticipate with a Sprite-only database.

Generated pixels and external provider state remain inputs/evidence. Canonical Trace2D project/runtime state remains authority.

## Completed immediate foundation

### #320 — set-level production intent and Art Profile — complete

`AssetProductionSpec` makes set-level creative intent recoverable from committed project state:

- production set and item identities,
- exact target dimensions,
- required deliverables/directions,
- structural constraints,
- bounded candidate/provider-call budget intent,
- exact #97 human-review acceptance reference,
- compact Art Profile and approved canonical references.

It owns no completion state, candidate lifecycle, provider configuration or aesthetic score.

### #321 — bounded Candidate Set comparison substrate — current

`AssetCandidateSet` is intentionally smaller than the original showroom proposal. It owns only:

- production-set/item binding,
- exact WorkResult/work revision binding,
- stable candidate identity and bounded ordinal,
- references to artifacts already owned by that revision.

Workspace composition validates that the revision is current and every referenced artifact is owned by that exact current revision. Approval/rejection/feedback remain ordinary WorkResult/Workspace truth.

See `docs/ASSET_CANDIDATE_SET.md`.

## Deferred depth

### #322 — approved Asset Library / lineage — deferred

Do not implement immediately after #321. Return only after the broader production foundation through #79 is complete. At that point the library contract can be evaluated against the asset classes and authoring primitives that actually exist instead of being prematurely Sprite-specific.

### #323 — one-provider end-to-end production proof — deferred

Return after #322. At implementation time, review one then-current practical provider/backend and prove a real coherent game-ready batch. Do not add provider SDK/auth/network/retry infrastructure now.

## Owner-fixed sequencing — 2026-08-19

```text
#315 tiny playable product proof       complete
 -> #320 production intent             complete
 -> #321 candidate comparison substrate
 -> #89 Material2D / Shader2D
 -> #90 deterministic Tween
 -> #76 Physics2D
 -> #77 Audio
 -> #91 Profiler
 -> #78 Linux / non-MSVC
 -> #92 tiered GPU QA
 -> #79 Save / Migration
 -> #322 approved asset lineage
 -> #323 one-provider batch proof
 -> #12 broad flagship external game
```

#318 remains open as a product umbrella but is not an operational blocker after #321.

## Art Profile / approved references

Coherent production still needs project style memory. Prefer compact project-owned creative constraints plus references to approved canonical assets over ever-growing prose prompts.

These references are creative evidence, not deterministic aesthetic truth. Final visual quality remains human judgment.

## Candidate comparison rule

Candidate production/review is bounded:

```text
finite candidates
 -> objective processing evidence where available
 -> exact result artifacts
 -> Workspace comparison
 -> human feedback/approval through existing authority
```

Never implement `generate until aesthetic_score >= threshold`.

## Generator policy when #323 resumes

Trace2D does not need to own the best generative model. Providers remain replaceable inputs behind the provider-neutral boundary.

When live generation is actually needed, integrate one practical provider/backend, freeze current license/version/deployment facts, and prove the owner loop before considering more.

## TracePixel relationship

TracePixel remains a deterministic raster R&D lab/reference backend. A technique may be promoted only after matched evidence shows a concrete Trace2D advantage. Generic processing already owned by SPP is not reimplemented merely to preserve TracePixel scope.
