# Trace2D Asset Studio

Tracking issue: #318. Existing review surface: #99 / `docs/WORKSPACE.md`.

## Product role

Asset Studio is the Trace2D-owned human-facing product for AI-operated sprite production.

It is not a separate TracePixel app and it is not a Unity/Aseprite-style mandatory manual editor.

Target human loop:

```text
Request -> Review candidates -> Give feedback / choose -> Approve
```

Target production loop:

```text
project asset-production request
 -> replaceable generation backend(s)
 -> bounded candidate set
 -> deterministic Sprite processing/import validation
 -> Workspace showroom/review queue
 -> owner approve / reject / request revision / request alternatives
 -> approved project asset library
 -> canonical SpriteAsset + animation
 -> immediate use by the external game
```

A representative request should eventually look like:

```text
"현재 프로젝트 스타일로 64x64 숲 몬스터 10종을 만들고
idle / walk / attack까지 준비해줘."
```

The goal is not one lucky generated image. The goal is a repeatable production system for coherent game-asset sets.

## Existing Trace2D authority to reuse

Asset Studio must compose existing production contracts rather than creating parallel truth.

- **Sprite S1 / SA / SR** own canonical Sprite assets, deterministic animation and runtime/render semantics.
- **SPP0-SPP5** own deterministic Sprite analysis/extraction/quality-repair/import/generator-manifest/provider-neutral generation orchestration.
- **#97** owns project-visible intent / Definition of Done.
- **#98** owns WorkResult, verification, diagnosis, revision evidence and result composition.
- **#99 Workspace** owns human review, previews, review queue, feedback and approval presentation.
- **#178** owns transactional semantic Sprite resource mutation.

Generated pixels and external provider state remain inputs/evidence. Canonical Trace2D asset/runtime state remains product authority.

## Missing product concepts to investigate first

The first #318 stage is **AS0 contract + responsibility gap analysis**, not broad implementation.

Before adding new code, classify the desired experience into:

```text
already solved by existing Trace2D
missing production-orchestration state
missing Workspace/showroom presentation state
missing project asset-library metadata
optional provider adapter needed for a real proof
research-only idea that has not earned promotion
```

Do not add a new raster QA vocabulary merely because another repository has one.

## Asset Production Request

The Studio should eventually support set-level intent rather than only one opaque generation call.

Only fields proven useful should be promoted, for example:

- project/set identity,
- asset class and requested count,
- target dimensions,
- required animations/directions,
- explicit structural constraints,
- art-profile/reference-set identity,
- candidate-count/provider budget,
- human review requirements.

This state should extend/reuse #97 rather than live only in chat history.

## Art Profile / approved references

Coherent mass production needs project style memory.

Prefer compact project-owned creative intent plus references to approved canonical assets over ever-growing prose prompts.

Potential dimensions include:

- palette family,
- outline/contrast policy,
- lighting direction,
- pixel density,
- character proportions,
- material treatment,
- approved example identities,
- rejected example identities where useful.

These are creative constraints/reference evidence. They do not make aesthetics deterministic truth.

## Candidate Set / showroom

Generation should be bounded and explicit:

```text
generate N candidates
 -> reject objective structural failures cheaply
 -> retain survivors and failures
 -> present candidates in Workspace
 -> owner choose / reject / request alternative / targeted revision
```

The showroom should expose, as relevant:

- native and nearest-neighbor preview,
- animation loop,
- deterministic findings,
- provider/provenance and cost evidence,
- revision lineage,
- approval/rejection state.

Do not use an autonomous `generate until aesthetic_score >= threshold` loop.

## Approved Asset Library

Approved assets become ordinary project-owned assets with enough structured lineage to support future AI production and retrieval.

Candidate metadata includes only what proves useful:

- canonical SpriteAsset / animation identity,
- source candidate/provider provenance,
- art-profile/reference lineage,
- deterministic import/QA evidence,
- owner approval/rejection history,
- revision ancestry,
- content digests,
- semantic tags for retrieval/production when justified.

The library must not become a GUI-only database that disagrees with canonical project assets.

## Generator policy

Trace2D does not need to own the best generative model.

Providers are replaceable inputs behind the existing SPP5 boundary. Candidate sources may include image models, specialized sprite generators, local workflows or a research backend.

When live generation is first needed, integrate **one** practical provider/backend, freeze current version/license/deployment facts, and prove the complete owner loop before adding more.

## TracePixel relationship

TracePixel is now a deterministic raster R&D lab, not Asset Studio product authority.

Potential research that may later be useful includes:

- protected-region pixel editing,
- exact collateral-damage measurement,
- precision local repair,
- exact before/after raster replay/diff evidence.

No TracePixel technique is automatically upstreamed. Promotion requires matched evidence that it improves an explicit Trace2D workflow.

Generic alpha/palette/grid/frame processing already covered by Trace2D SPP should not be reimplemented merely to preserve TracePixel scope.

## Production proof

Asset Studio must eventually prove a real set, not a cherry-picked single sprite.

A first production proof should target roughly:

- 10-20 coherent static assets, or
- a smaller coherent character set with multiple animations.

Keep result layers separate:

- owner acceptance rate,
- candidates generated per accepted asset,
- provider calls/tokens/cost/time where measurable,
- deterministic rejection/repair count,
- owner interventions and revision rounds,
- perceptual cross-asset style/identity consistency,
- canonical import/runtime playback success,
- Agent discovery/context/revision cost.

## Sequencing

Current owner-fixed order:

```text
#315 tiny playable product proof
 -> #318 Asset Studio AS0 contract/gap analysis
 -> only owner-approved Asset Studio implementation slices
 -> resume #89+ engine breadth when #318's checkpoint says so
```

#318 must not interrupt active #315. Likewise, AS0 must not silently become a large provider/UI/database build before the responsibility gap is proven.
