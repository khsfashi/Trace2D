# Explicit on-demand content authoring (C1)

Last reviewed: **2026-08-10**.

This document is the implementation guide for GitHub Issue #109 and the C1 stage of [`DEVELOPMENT_CONTENT_PIPELINE.md`](DEVELOPMENT_CONTENT_PIPELINE.md).

C1 is **Agent-operated and provider-neutral**. Trace2D does not embed an OpenAI, Anthropic, Gemini, or other model SDK/API key merely to write posts. The currently selected coding/writing Agent may author the prose, but it must consume the same repository-owned request packet and truth boundaries.

Core rule:

> **A maintainer request is the authoring trigger. Repository events are never authoring triggers.**

The implemented flow is:

```text
explicit maintainer request
 + selected C0 Fact Pack(s)
 + Editorial Brief
 + CONTENT_AUTHOR_STYLE.md
 + optional dynamic platform profile
        ↓
scripts/content_authoring.py prepare
        ↓
Authoring Request Packet
        ↓
current Agent writes one requested editable draft
        ↓
optional scripts/content_authoring.py wrap-draft
        ↓
clearly marked DRAFT + truth-boundary dispositions
        ↓
maintainer edits / discards / manually publishes
```

No GitHub Actions merge/push event invokes this flow.

## 1. Why the repository owns the packet, not the model

The authoring model can change over time. The factual and editorial contract should not.

The Authoring Request Packet therefore contains:

- exact explicit maintainer request,
- normalized Editorial Brief,
- one or more selected Fact Pack identities and hashes,
- all usable structured facts and evidence,
- qualified fact references (`<candidate-id>#<fact-id>`),
- truth-boundary facts that require deliberate treatment,
- optional platform profile from `content/platforms.json`,
- optional candidate platform hints,
- the exact repository-local author-style contract and its SHA-256,
- approved maintainer reference-corpus URLs,
- whether current external research is required,
- hard authoring rules.

This makes the request inspectable and reproducible without making a proprietary model/provider part of the content schema.

## 2. Explicit trigger

`prepare` requires a non-empty `--maintainer-request`.

Example:

```bash
python scripts/content_authoring.py prepare \
  --candidate content/candidates/<candidate>.json \
  --maintainer-request "GPU 파티클 설계 과정을 Tistory 개발로그로 써줘. CPU semantic oracle을 유지한 이유에 집중해줘." \
  --platform tistory \
  --mode development-log \
  --length long \
  --output /tmp/trace2d-authoring-request.json
```

The request text is not inferred from a merge, release, benchmark, `Content:` trailer, candidate significance, platform state, or lifecycle state.

If no explicit maintainer writing request exists, C1 does nothing.

## 3. Selecting Fact Packs

A request may use one or many C0 candidates.

```bash
python scripts/content_authoring.py prepare \
  --candidate content/candidates/merge-aaa.json \
  --candidate content/candidates/merge-bbb.json \
  --maintainer-request "두 작업을 하나의 설계 변화 이야기로 묶어서 써줘" \
  --mode engineering-thesis
```

Fact IDs are only unique inside one candidate, so C1 qualifies them as:

```text
<candidate-id>#<fact-id>
```

If the relevant merged event has no committed candidate file yet, a working-tree candidate may first be reconstructed with the C0 extractor from durable Git history. This does not require committing the candidate merely to request a draft.

## 4. Editorial Brief

The explicit request itself can be the minimum brief. Optional structured flags make repeated requests less ambiguous:

```text
--goal
--audience        repeatable
--platform
--angle           repeatable
--emphasize       repeatable
--deemphasize     repeatable
--required-limitation repeatable
--language
--length          short | medium | long
--mode            engineering-thesis | practical-technical-explanation | development-log
--requires-current-research
--selected-fact-ref repeatable
```

The brief controls the requested piece. Platform metadata and style guidance must not silently replace the requested angle with a more promotional or engagement-optimized one.

The same Fact Pack can therefore produce separate requests such as:

```text
Tistory / Korean / development-log / architecture history
Show HN / English / engineering-thesis / reproducibility and limitations
X / Korean / short / one measured result
```

These are different requested pieces, not automatic cross-post variants.

## 5. Factual authority and truth boundaries

Every Fact Pack fact remains available to the Agent as factual authority.

C1 conservatively treats these C0 categories as explicit truth-boundary facts:

```text
not_tested
gates
limitations
benchmark_metrics
```

These categories are not automatically forced into every paragraph. They are forced into the **decision process** so they cannot disappear silently.

Examples:

- `Not-tested: real GPU smoke not run` cannot become “GPU verified”.
- an unresolved hardware gate cannot become a completed acceptance claim.
- a benchmark loss cannot be rewritten away because a positive article angle was requested.
- a limitation may be omitted only when it is genuinely immaterial to the claims in that requested piece.

When a draft is stored through `wrap-draft`, every boundary fact requires one of:

```text
included-or-addressed
not-material + explicit reason
```

This is a lightweight evidence discipline, not a semantic proof that every sentence is correct. The maintainer still owns final review.

## 6. Style

The default compact style authority is:

```text
docs/CONTENT_AUTHOR_STYLE.md
```

`prepare` embeds:

- the exact file text,
- its SHA-256,
- the approved public reference-corpus URLs discovered from it.

The current approved corpus includes:

```text
https://woodroot.tistory.com/
```

The Agent may use the profile for voice, pacing, explanatory structure and article-mode tendencies.

It may **not** use style imitation to invent:

- personal anecdotes,
- employment history,
- past experiences,
- feelings,
- motives,
- decisions not present in approved evidence/context.

If the maintainer explicitly asks for fresh style sampling, current public writing may be sampled at authoring time. The full blog remains external and is not vendored as a mandatory repository dependency.

## 7. Current external facts

Fact Packs are Trace2D evidence, not a cache of forever-current external facts.

When the requested article depends on current external facts, run `prepare` with:

```text
--requires-current-research
```

The packet then marks `docs/EXTERNAL_REFERENCE_PROTOCOL.md` as required. The Agent must refresh current attributable sources before writing those external claims.

Current external research must remain distinguishable from Trace2D repository evidence.

## 8. Writing the draft

`scripts/content_authoring.py` deliberately does not contain a `generate` subcommand.

The active Agent is the prose generator after receiving the explicit maintainer request. This avoids a second embedded LLM stack and keeps C1 usable from ChatGPT, Codex, or another future Agent.

The Agent should:

1. build/read the Authoring Request Packet,
2. obey the Editorial Brief,
3. ground Trace2D claims in `factual_authority`,
4. research current external facts only when needed,
5. apply the requested article mode and style contract,
6. preserve material truth boundaries,
7. return a clearly identified editable draft,
8. stop before publication.

For a draft returned directly in chat, repository storage is optional.

## 9. Optional draft storage

If the maintainer wants a repository/local draft file, first write the Agent-authored Markdown body, then wrap it:

```bash
python scripts/content_authoring.py wrap-draft \
  --request /tmp/trace2d-authoring-request.json \
  --draft /tmp/body.md \
  --used-fact-ref '<candidate-id>#decisions-001' \
  --ack-boundary '<candidate-id>#not-tested-001' \
  --not-material-boundary '<candidate-id>#limitations-001|이 글의 주장 범위와 무관함' \
  --output /tmp/draft.md \
  --metadata-output /tmp/draft.meta.json
```

The wrapped Markdown starts with:

```text
DRAFT — maintainer review required. Not published.
```

Metadata always records:

```text
status: draft
publication_mode: manual
```

Editing or deleting the draft does not modify Fact Packs, engine code, roadmap state or acceptance evidence.

## 10. Dynamic platforms

C1 reads `content/platforms.json` at request time.

A new platform remains registry data. The core Fact Pack schema, author-style schema and C1 code do not gain platform-specific fields merely because another destination is added.

C1 also rejects any platform profile whose `publication_mode` is not `manual`.

Platform profiles are audience/format hints, not publication integrations.

## 11. No publication surface

The C1 CLI intentionally has only:

```text
prepare
wrap-draft
```

It has no:

```text
generate-on-merge
publish
schedule
cross-post
reply
engagement-optimize
```

Direct external posting remains out of scope.

## 12. Failure isolation

C1 is non-core repository tooling.

Its fixture tests run only in the advisory `Content Evidence` workflow, whose validation job uses `continue-on-error: true`.

A C1 failure:

- does not fail the normal engine CI job,
- does not clear or change an engine gate,
- does not block `Trace2D next/continue`,
- does not change `PROJECT_STATUS.md`,
- can be repaired independently.

## 13. Agent interpretation of maintainer requests

When a maintainer explicitly asks for Trace2D writing, for example:

```text
이 GPU 파티클 작업으로 개발로그 써줘
Fact Pack 기반으로 Tistory 글 써줘
Show HN용으로 benchmark 방법론 위주로 써줘
```

that request authorizes **one requested authoring operation** with the stated direction.

The Agent should recover the relevant candidate(s), build the equivalent C1 packet, and write the draft without asking the maintainer to restate facts that are already recoverable from the repository.

If the user provides only a topic but enough repository evidence exists, infer a conservative Editorial Brief from that request. If the requested angle/audience is genuinely ambiguous and materially changes the piece, ask only for that missing editorial choice.

A completed draft never authorizes another platform variant, scheduled follow-up, publication or community reply unless the maintainer separately requests it.
