# Development-derived content pipeline

Last reviewed: **2026-08-10**.

Trace2D development should naturally leave enough structured evidence that the maintainer can later write technical posts without reconstructing weeks of context from memory, chat history, or scattered pull requests.

This is **not** an automatic marketing-copy system.

Core rule:

> **Trace2D automates evidence collection and content-candidate discovery. The maintainer writes the actual posts.**

The pipeline exists to turn normal engineering work into a durable, searchable **Content Candidate / Fact Pack**. It does not generate final titles, article prose, social-media copy, promotional claims, or automatic external posts.

Related contracts:

- [`COMMIT_KNOWLEDGE.md`](COMMIT_KNOWLEDGE.md) — durable final-main engineering knowledge
- [`REPOSITORY_AUTOMATION.md`](REPOSITORY_AUTOMATION.md) — repository-derived automation principles
- [`COMPETITIVE_STRATEGY.md`](COMPETITIVE_STRATEGY.md) — evidence boundaries for comparative claims
- [`EXTERNAL_REFERENCE_PROTOCOL.md`](EXTERNAL_REFERENCE_PROTOCOL.md) — current-source and attribution discipline

---

## 1. Product boundary

The content pipeline is repository tooling, not engine functionality and not part of the runtime hot path.

It should eventually automate this:

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
    ↓
maintainer chooses whether/where to write
    ↓
maintainer writes platform-specific content manually
    ↓
optional publication record back into the repository
```

It must **not** evolve into:

```text
merge
 -> LLM writes blog post
 -> LLM rewrites it for every platform
 -> automatic cross-post everywhere
```

The authorial voice remains human-owned.

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

The pipeline should preserve and index those facts so the maintainer can later decide what story is worth telling.

It should never invent the story itself.

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

Routine cleanup, typo fixes, dependency churn, or mechanical refactors normally remain `none` unless they contain an independently interesting engineering lesson.

### Suggested significance classes

```text
none
candidate
major
release
```

These classes are advisory metadata, not publishing commands.

A future extractor may infer a class from repository facts, while an explicit maintainer/Agent classification can override it. Classification must never trigger external posting automatically.

---

## 4. Fact Pack — the automation output

A Fact Pack is structured source material, not prose.

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

The exact serialized format is future implementation work. The important contract is the separation between **facts/evidence** and **written content**.

### Evidence requirements

Every non-trivial fact should point back to one or more repository sources where practical.

For example:

```text
Fact: real-GPU smoke passed
Source: exact CI/local evidence record

Fact: CPU reference remains the particle semantic oracle
Source: owning particle contract / final-main decision

Fact: benchmark success rate = X/Y
Source: committed benchmark aggregate + trial set
```

If evidence is unavailable, the field remains absent/unknown. Do not fill gaps with plausible prose.

---

## 5. Human-authored content only

The initial Trace2D content pipeline has a hard authoring boundary:

- no automatic article body generation,
- no automatic social-post body generation,
- no automatic title generation as a required workflow,
- no automatic translation presented as the maintainer's authored post,
- no automatic publication,
- no automatic replies/comments/community engagement.

A coding Agent may maintain the **Fact Pack** and metadata required to make manual writing easier. It should not impersonate the maintainer's personal voice.

Future optional writing assistance may only be added by an explicit owner decision that changes this contract. It is not implied by the existence of the pipeline.

---

## 6. One event does not imply one universal post

The same Trace2D development event may become different content for different audiences.

For example, one benchmark milestone might later be written manually as:

```text
Korean long-form blog
    architecture + implementation history

English engineering community
    benchmark methodology + reproducibility

short-form social network
    one measured result + demo artifact

game-development community
    practical workflow / game-production implications
```

These are **independent authored pieces** that happen to share evidence.

Hard rule:

> **Platform variants share source facts when appropriate; they do not need to share wording, length, angle, language, or even the exact subset of facts.**

The pipeline must therefore model platform/audience selection separately from the canonical Fact Pack.

---

## 7. Dynamic platform registry

Do not hard-code Tistory, X, Reddit, Hacker News, GeekNews, or any other site into the core candidate/fact schema.

Platforms should be optional registry/configuration entries.

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

A new platform should normally require only a new registry entry and, if useful, platform-specific metadata/rules. It must not require changing the Fact Pack schema or the engine/repository knowledge model.

### Platform record

A candidate may have zero or more independent platform records:

```yaml
platform_id: show-hn
state: considered | selected | authored | published | skipped
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

`angle_tags` are indexing metadata only. They are not generated article outlines or copy.

### Platform independence

- publishing to one platform does not imply publishing to another,
- `skipped` is a valid terminal state,
- the maintainer may write different content for every platform,
- one platform may receive multiple distinct posts from one candidate over time,
- one post may combine several related candidates,
- platform registration must not create a requirement to maintain an account or API integration.

---

## 8. Candidate lifecycle

Suggested lifecycle:

```text
discovered
 -> reviewed
 -> selected | parked | discarded
 -> authored
 -> published
```

Only `discovered` and evidence refresh are candidates for automatic repository tooling.

The following remain human decisions:

- whether the topic is interesting,
- whether to write now or later,
- intended audience,
- platform selection,
- angle,
- title,
- structure,
- wording,
- publication timing,
- whether to publish at all.

A candidate can stay parked indefinitely without becoming repository debt.

---

## 9. Relationship to structured commit knowledge

`docs/COMMIT_KNOWLEDGE.md` is a strong source for content extraction because final-main squash commits already preserve durable engineering context.

The content pipeline may use facts such as:

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

It is a **hint**, not a required field for every commit and not a publication instruction.

Rules:

- absence of `Content:` does not mean a future candidate cannot be discovered,
- `Content: major` does not authorize generated prose or external posting,
- the final merged source/evidence remains authoritative over the trailer,
- content metadata must never encourage exaggerating `Tested` or hiding `Not-tested`,
- secrets/private paths/raw prompts/private chain-of-thought remain forbidden source material.

---

## 10. Candidate discovery rules

A future implementation should prefer stable deterministic inputs rather than an LLM reading every diff and improvising marketing value.

Good signals include:

- explicit `Content:` trailer,
- release/tag creation,
- benchmark-result publication,
- owning issue category/milestone,
- presence of new evidence artifacts,
- substantive `Decision` / `Rejected` / measured `Tested` knowledge,
- merge completion of a user-facing subsystem.

An LLM may optionally assist classification later, but deterministic metadata and final-main evidence remain authoritative.

No candidate discovery step may modify code, acceptance evidence, or project priority merely to make a better story.

---

## 11. Content queue storage

Do not create a second database unless the simple repository model proves insufficient.

Preferred first implementation options are repository-native and reviewable, for example:

```text
content/candidates/<id>.yaml
```

or a small generated index backed by final-main sources.

The owning implementation issue should choose the smallest representation that supports:

- stable candidate identity,
- source/evidence references,
- duplicate detection,
- lifecycle state,
- platform records,
- future platform addition,
- manual publication URL/history.

Generated indexes may be rebuilt from authoritative source records when practical.

Do not store actual article drafts in the required machine-generated candidate format.

---

## 12. Publication records

External publication is not part of the automatic pipeline, but the maintainer may optionally record where an authored post was published.

The record should contain only useful metadata such as:

```text
platform_id
published_url
published_at
candidate/source relationship
language
```

This allows future questions such as:

- Which major Trace2D milestones have never been written about?
- Which benchmark result was already discussed publicly?
- Which platforms have received content about particles versus Agent verification?

The repository does not need to archive a duplicate copy of every externally authored article unless the maintainer explicitly wants that later.

---

## 13. Claims and evidence safety

Technical-content evidence follows the same truth boundaries as the project itself.

Never derive a publishable fact that converts:

- `planned` into `implemented`,
- `implemented` into `tested`,
- `hosted CI` into `real GPU evidence`,
- `candidate benchmark design` into `benchmark result`,
- one favorable run into a comparative conclusion,
- a subjective multimodal opinion into deterministic correctness,
- an Agent's self-report into independent verification.

For comparative content, [`COMPETITIVE_STRATEGY.md`](COMPETITIVE_STRATEGY.md) remains authoritative: unfavorable results are valid results and must not be hidden by the content pipeline.

The Fact Pack should preserve limitations and `Not-tested` evidence alongside positive results so manual writing starts from the complete technical record.

---

## 14. Interaction with `Trace2D next/continue`

Content extraction is **non-blocking parallel repository work**.

It must never delay or reorder the owner-fixed core engine lane.

A normal core completion may conceptually become:

```text
implement / verify / merge
    ↓
final-main knowledge atom
    ↓
optional asynchronous-style repository event processing
    ↓
create or refresh Content Candidate / Fact Pack
```

The candidate creation itself must not be treated as part of engine acceptance.

If candidate tooling is broken, core engine development continues; the content index can be rebuilt/repaired later from durable repository evidence.

---

## 15. Initial implementation boundary

The first implementation should stop at:

```text
merged repository evidence
 -> candidate detection
 -> machine-readable Fact Pack
 -> optional platform registry/records
```

Explicitly out of scope for the initial implementation:

- generated post bodies,
- generated social copy,
- cross-platform rewriting,
- direct Tistory/X/Reddit/HN/GeekNews APIs,
- browser automation for posting,
- scheduled publication,
- engagement analytics/optimization,
- auto-comment/reply behavior.

Those are separate product choices and are not required to gain the main benefit: **Trace2D development continuously accumulates clean technical source material that the maintainer can turn into content whenever desired.**

---

## 16. Acceptance for a future implementation

A future non-core implementation is sufficient when it can demonstrate from repository fixtures that:

1. a substantive merged event produces one stable candidate/fact record,
2. trivial work can remain `none`,
3. facts retain source/evidence links,
4. `Tested` and `Not-tested` stay distinct,
5. repeated processing is idempotent and does not duplicate candidates,
6. a candidate can target zero, one, or many platforms,
7. adding a new platform registry entry does not require changing the Fact Pack schema,
8. different platform records can select different fact subsets/angle tags,
9. no article/social prose is generated,
10. no external publication occurs,
11. content-tooling failure cannot block the core engine lane,
12. candidate/index state can be rebuilt or reconciled from durable repository evidence where practical.

The implementation should remain small enough that maintaining the content pipeline never becomes more work than writing the posts it is intended to support.
