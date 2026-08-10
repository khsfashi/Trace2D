# Development-derived content pipeline

Last reviewed: **2026-08-10**.

Trace2D development should naturally leave enough structured evidence that the maintainer can later write technical posts without reconstructing weeks of context from memory, chat history, or scattered pull requests.

This is **not** an automatic marketing or cross-posting system.

Core rule:

> **Evidence is accumulated automatically. Prose is generated only when the maintainer explicitly asks for a draft, from selected evidence and an explicit editorial direction. The maintainer reviews, edits, owns and publishes every post.**

The pipeline therefore has two deliberately separate layers:

```text
Evidence layer
    repository development
      -> Content Candidate
      -> Fact Pack

On-demand authoring layer
    maintainer request
      + selected Fact Pack(s)
      + Editorial Brief
      + Author Style Profile / reference corpus
      + optional Platform Profile
      -> editable draft
      -> maintainer edit / approval / manual publication
```

A merge, release, benchmark or candidate discovery must never create or publish prose by itself.

Related contracts:

- [`COMMIT_KNOWLEDGE.md`](COMMIT_KNOWLEDGE.md) — durable final-main engineering knowledge
- [`REPOSITORY_AUTOMATION.md`](REPOSITORY_AUTOMATION.md) — repository-derived automation principles
- [`COMPETITIVE_STRATEGY.md`](COMPETITIVE_STRATEGY.md) — evidence boundaries for comparative claims
- [`EXTERNAL_REFERENCE_PROTOCOL.md`](EXTERNAL_REFERENCE_PROTOCOL.md) — current-source and attribution discipline
- GitHub Issue #108 — evidence/candidate implementation
- GitHub Issue #109 — on-demand authoring implementation

---

## 1. Product boundary

The content pipeline is repository tooling, not engine functionality and never runtime-frame work.

Normal automatic processing may eventually do this:

```text
normal Trace2D development
    ↓
merged PR / release / benchmark / milestone
    ↓
repository evidence extraction
    ↓
Content Candidate
    ↓
Fact Pack + artifact references + source links
```

Writing begins only after an explicit maintainer request such as:

```text
Use the GPU-particle Fact Packs.
Write a Tistory development log about why CPU remains the semantic oracle.
Focus on the design tradeoff rather than feature promotion.
```

Then the authoring layer may do:

```text
selected Fact Pack(s)
 + Editorial Brief
 + Author Style Profile / reference examples
 + optional Platform Profile
    ↓
AI-assisted draft
    ↓
maintainer revision / approval
    ↓
manual publication
```

It must **not** evolve into:

```text
merge
 -> unsolicited LLM article
 -> automatic rewriting for every platform
 -> automatic cross-post
 -> automatic engagement/replies
```

The authorial voice and publication decision remain human-owned.

---

## 2. Why derive content from development state

Technical content is easiest to trust when it is grounded in the same evidence used to build the project.

Useful source material already exists in Trace2D development:

- issue goals and acceptance contracts,
- final merged code and tests,
- final-main structured commit knowledge,
- design decisions and rejected alternatives,
- constraints and human/environment gates,
- `Tested` / `Not-tested` boundaries,
- performance measurements,
- deterministic repro cases,
- benchmark results,
- captures/GIFs/videos/diagrams,
- external references that influenced a decision,
- known limitations,
- follow-up work.

The evidence layer preserves and indexes those facts. The authoring layer may arrange them into a story only after the maintainer chooses the story to tell.

The draft generator must never invent missing engineering facts merely because they would make the article smoother.

---

## 3. Content Candidate

A **Content Candidate** means only:

> A development event appears substantial enough that a future technical post may be worth writing.

Candidate discovery may be triggered by events such as:

- a substantial PR merge,
- a release/tag,
- a benchmark suite/result,
- a new production subsystem becoming usable,
- a meaningful architecture decision,
- an interesting failure/repair investigation,
- a measured performance improvement,
- a public demo/game milestone.

Routine cleanup, typo fixes, dependency churn or mechanical refactors normally remain `none` unless they contain an independently interesting engineering lesson.

Suggested significance classes:

```text
none
candidate
major
release
```

These classes are advisory metadata, not drafting or publishing commands.

A future extractor may infer a class from repository facts, while explicit maintainer/Agent classification can override it. Classification alone must never trigger prose generation.

---

## 4. Fact Pack — canonical technical source material

A Fact Pack is structured evidence, not prose.

Conceptually:

```yaml
schema_version: 1
candidate_id: trace2d-...
source_event: merge | release | benchmark | milestone
significance: candidate | major | release

sources:
  issues: []
  pull_requests: []
  commits: []
  tags: []

areas: []
topics: []

facts:
  decisions: []
  constraints: []
  rejected_alternatives: []
  directives: []
  implemented: []
  tested: []
  not_tested: []
  gates: []
  limitations: []
  benchmark_metrics: []

artifacts:
  captures: []
  videos: []
  diagrams: []
  benchmark_reports: []
  repro_cases: []

references: []
related_candidates: []
platform_records: []
```

The exact serialization is implementation work. The stable contract is the separation between **facts/evidence** and **written content**.

### Evidence requirements

Every non-trivial fact should point back to repository evidence where practical.

For example:

```text
Fact: real-GPU smoke passed
Source: exact local evidence record

Fact: CPU reference remains the particle semantic oracle
Source: owning particle contract / final-main decision

Fact: benchmark success rate = X/Y
Source: committed benchmark aggregate + trial set
```

If evidence is unavailable, the field remains absent/unknown. Do not fill gaps with plausible narrative.

---

## 5. On-demand authoring layer

Prose generation is permitted only after an explicit maintainer authoring request.

The minimum authoring inputs are conceptually:

```text
selected Fact Pack(s)
+ Editorial Brief
+ Author Style Profile or approved style references
+ optional Platform Profile
```

The output is an **editable draft**, never an automatically publishable authority.

### Editorial Brief

The Editorial Brief owns the direction of one requested piece. It may specify:

```yaml
goal: >
  Explain why the GPU-particle design keeps a deterministic CPU semantic oracle
  and leaves backend selection explicit.

audience:
  - game-client-programmer
  - engine-programmer
angle:
  - architecture-decision
  - verification
emphasize:
  - alternatives considered
  - why automatic CPU-to-GPU conversion was rejected
  - real validation boundaries
deemphasize:
  - feature checklist
avoid:
  - superiority claims without benchmark evidence
  - generic AI marketing language
language: ko
length: long
```

The brief may be supplied entirely in natural language. A structured form exists only so repeated tooling can preserve intent without guessing.

The draft generator must not silently choose a fundamentally different angle because another angle seems more promotional or engaging.

---

## 6. Author Style Profile and Author Reference Corpus

The style layer should learn from **actual maintainer-authored writing**, not only adjectives such as `friendly` or `technical`.

Current public author reference corpus:

```text
https://woodroot.tistory.com/
```

This public blog is an approved style reference source for Trace2D writing assistance.

Do **not** vendor the whole blog into the repository merely to imitate style. Prefer:

1. a compact derived Author Style Profile,
2. a small relevant sample of maintainer-authored posts retrieved at drafting time when available,
3. optional future repository-local examples only when the maintainer deliberately adds them.

The external corpus is style evidence, not Trace2D engineering truth. Trace2D facts still come from Fact Packs and current cited external research where the requested article needs it.

### Initial style fingerprint

The current corpus suggests these high-level tendencies, which should be refreshed rather than treated as immutable rules:

- start from a concrete problem, surprising observation, practical failure or direct question rather than a ceremonial introduction,
- state the central proposition relatively early,
- explain causes in a sequential progression instead of front-loading every qualification,
- use short declarative Korean paragraphs mixed with headings, lists, Q&A or small causal-flow blocks when structure helps,
- define a concept before expanding its implications,
- prefer concrete developer/game-engine examples over generic abstractions,
- make engineering tradeoffs explicit rather than presenting one technique as universally correct,
- use personal experience when it genuinely motivates the problem, but keep the technical subject central,
- distinguish observation, claim, evidence and speculation,
- include counterarguments/limitations when the article makes a broad thesis,
- when applicable, finish by compressing the discussion into a short principle or practical implication,
- use public references for research-heavy claims rather than making authority claims from memory.

This profile describes recurring structure, not a template every post must obey.

### Style integrity rules

- emulate high-level voice, pacing and explanatory structure; do not mechanically copy memorable sentences from old posts,
- never invent personal anecdotes, employment history, feelings or past decisions merely because they sound like the author,
- autobiographical details require an explicit user-provided fact, approved source, or Editorial Brief,
- newer maintainer-authored examples may supersede older style tendencies,
- platform/audience requirements may intentionally override parts of the default style,
- factual accuracy outranks style imitation.

---

## 7. One event does not imply one universal post

The same Trace2D event may become different manually requested drafts for different audiences.

For example, one benchmark milestone might become:

```text
Korean long-form blog
    architecture + implementation history + design reasoning

English engineering community
    benchmark methodology + reproducibility + limitations

short-form social network
    one measured result + demo artifact

game-development community
    practical workflow / production implications
```

These are independent pieces sharing evidence.

Hard rule:

> **Platform variants may share source facts, but they do not need to share wording, length, angle, language, structure, or even the exact subset of facts.**

The authoring request chooses the intended piece. The system does not automatically fan one draft out into synchronized copies.

---

## 8. Dynamic platform registry

Do not hard-code Tistory, X, Reddit, Hacker News, GeekNews or any other site into the core candidate/fact schema.

Platforms are optional registry/configuration entries.

Conceptually:

```yaml
platforms:
  - id: tistory
    enabled: true
    audience: ko-technical-longform
    default_language: ko
    format_class: longform
    publication_mode: manual

  - id: show-hn
    enabled: true
    audience: en-technical-builders
    default_language: en
    format_class: project-showcase
    publication_mode: manual
```

A new platform should normally require only a registry entry and optional platform-specific metadata/rules. It must not require changing the Fact Pack schema or engine/repository knowledge model.

### Platform record

A candidate may have zero or more independent platform records:

```yaml
platform_id: show-hn
state: considered | selected | drafted | authored | published | skipped
language: en
audience: technical-builders
angle_tags:
  - benchmark
  - deterministic-verification
source_fact_ids: []
published_url: null
published_at: null
notes: null
```

`angle_tags` are indexing metadata, not mandatory outlines.

Platform rules may guide an explicitly requested draft, but they do not automatically trigger generation or publication.

---

## 9. Candidate and draft lifecycle

Evidence lifecycle:

```text
discovered
 -> reviewed
 -> selected | parked | discarded
```

Optional authoring lifecycle after an explicit request:

```text
selected evidence
 -> drafting-requested
 -> draft
 -> maintainer-edited / approved
 -> published | parked
```

The following remain maintainer decisions:

- whether the topic is interesting,
- whether to write now or later,
- intended audience,
- platform selection,
- editorial angle,
- final title,
- final structure and wording,
- publication timing,
- whether to publish at all.

A generated draft can be discarded with no repository consequence.

---

## 10. Relationship to structured commit knowledge

`docs/COMMIT_KNOWLEDGE.md` is a strong source for Fact Pack extraction because final-main squash commits already preserve durable engineering context.

The evidence pipeline may use:

```text
Area:
Decision:
Constraint:
Rejected:
Directive:
Tested:
Not-tested:
Gate:
Reference:
Related:
Supersedes:
```

A new optional trailer is reserved:

```text
Content: none | candidate | major | release
```

It is a **discovery/significance hint**, not a required field, draft command or publication instruction.

Rules:

- absence of `Content:` does not prevent later candidate discovery,
- `Content: major` does not authorize prose generation,
- final merged source/evidence remains authoritative over the trailer,
- content metadata must never encourage exaggerating `Tested` or hiding `Not-tested`,
- secrets/private paths/raw prompts/private chain-of-thought remain forbidden source material.

---

## 11. Candidate discovery rules

A future implementation should prefer stable deterministic inputs rather than an LLM reading every diff and improvising marketing value.

Good signals include:

- explicit `Content:` trailer,
- release/tag creation,
- benchmark-result publication,
- owning issue category/milestone,
- new evidence artifacts,
- substantive `Decision` / `Rejected` / measured `Tested` knowledge,
- merge completion of a user-facing subsystem.

An LLM may assist classification, but deterministic metadata and final-main evidence remain authoritative.

Candidate discovery may never modify code, acceptance evidence or project priority merely to make a better story.

Candidate discovery must not automatically invoke the authoring layer.

---

## 12. Repository storage

Do not create a second database unless the simple repository model proves insufficient.

Preferred first evidence representation:

```text
content/candidates/<stable-id>.yaml
content/platforms.yaml
```

A later authoring representation may add small, reviewable files such as:

```text
content/author/style-profile.yaml
content/briefs/<draft-id>.yaml
```

Do not require repository storage for the full public blog corpus.

If drafts are stored later, clearly distinguish:

```text
generated draft != published maintainer-authored article
```

The evidence store should support stable candidate identity, source/evidence references, duplicate detection, lifecycle state, platform records and optional publication metadata.

Generated indexes should be rebuildable from authoritative source records where practical.

---

## 13. Publication records

External publication remains manual.

The maintainer may optionally record:

```text
platform_id
published_url
published_at
candidate/source relationship
language
```

This supports queries such as:

- Which major milestones have never been written about?
- Which benchmark result was already discussed publicly?
- Which platforms have received content about particles versus Agent verification?

The repository does not need to duplicate every externally published article unless the maintainer explicitly chooses to archive it later.

---

## 14. Claims and evidence safety

Drafting follows the same truth boundaries as the project itself.

Never convert:

- `planned` into `implemented`,
- `implemented` into `tested`,
- `hosted CI` into `real GPU evidence`,
- `candidate benchmark design` into `benchmark result`,
- one favorable run into a comparative conclusion,
- a subjective multimodal opinion into deterministic correctness,
- an Agent self-report into independent verification.

For comparative content, [`COMPETITIVE_STRATEGY.md`](COMPETITIVE_STRATEGY.md) remains authoritative: unfavorable results are valid results and must not be hidden by drafting choices.

Fact Packs must preserve limitations and `Not-tested` evidence. An Editorial Brief may de-emphasize irrelevant facts, but it must not conceal a limitation that materially changes the truth of a claim being made.

When a requested article needs current external facts, research them under the current-source/attribution policy rather than relying on stale Fact Pack prose or old model memory.

---

## 15. Interaction with `Trace2D next/continue`

Content evidence and authoring are **non-blocking parallel repository work**.

They must never delay or reorder the owner-fixed core engine lane.

A normal core completion may conceptually become:

```text
implement / verify / merge
    ↓
final-main knowledge atom
    ↓
optional candidate extraction
```

Nothing else happens unless the maintainer explicitly requests writing assistance.

If candidate or authoring tooling is broken, engine development continues. Evidence indexes can be rebuilt later from durable repository state.

---

## 16. Implementation stages

### C0 — Evidence / Fact Pack (#108)

Initial automation stops at:

```text
merged repository evidence
 -> candidate detection
 -> machine-readable Fact Pack
 -> optional platform registry/records
```

C0 does **not** generate article/social prose.

C0 acceptance should prove:

1. a substantive merged event produces one stable candidate/fact record,
2. trivial work can remain `none`,
3. facts retain source/evidence links,
4. `Tested` and `Not-tested` stay distinct,
5. repeated processing is idempotent,
6. a candidate can target zero, one or many platforms,
7. adding a platform registry entry does not require changing the Fact Pack schema,
8. platform records can select different fact subsets/angle tags,
9. candidate discovery does not invoke drafting,
10. no external publication occurs,
11. content-tooling failure cannot block the core engine lane,
12. candidate/index state can be rebuilt or reconciled from durable evidence where practical.

### C1 — Explicit on-demand authoring (#109)

C1 may start after C0 provides stable evidence inputs.

C1 accepts requests equivalent to:

```text
selected candidate(s)
+ editorial direction
+ audience/platform
+ desired language/length
 -> evidence-grounded editable draft
```

C1 acceptance should prove:

1. no draft is generated without an explicit maintainer request,
2. selected Fact Packs remain factual authority for Trace2D claims,
3. an Editorial Brief materially controls angle/emphasis/audience,
4. the approved public author corpus / derived style profile can guide voice and explanatory structure,
5. style imitation never fabricates personal anecdotes or engineering history,
6. several Fact Packs can be combined into one coherent requested article,
7. one Fact Pack can produce intentionally different drafts for different platforms/audiences,
8. a new platform does not require changing Fact Pack or author-style schemas,
9. generated drafts are clearly drafts and never automatically published,
10. `Not-tested`, limitations and losing benchmark evidence cannot be silently converted into stronger claims,
11. the maintainer can edit/discard the draft without affecting engine/project state,
12. C1 failure never blocks `Trace2D next/continue`.

Direct Tistory/X/Reddit/HN/GeekNews APIs, browser posting automation, scheduled publication and auto-comment/reply behavior remain out of scope unless a later explicit owner decision adds them.

The objective is not to automate the author's existence. It is to make Trace2D development continuously produce trustworthy material from which the maintainer can request a high-quality draft in a chosen direction whenever useful.
