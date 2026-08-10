# Trace2D development-derived content pipeline

Trace2D content tooling has two deliberately separate layers:

```text
C0 — evidence
final-main repository evidence
 -> deterministic candidate discovery
 -> Fact Pack JSON
 -> optional review/platform metadata
 -> stop

C1 — explicit authoring
explicit maintainer request
 + selected Fact Pack(s)
 + Editorial Brief
 + maintainer style contract
 + optional platform profile
 -> Authoring Request Packet
 -> current Agent writes one editable draft
 -> maintainer edit / discard / manual publication
```

A repository event may create C0 evidence. It may **never** create C1 prose by itself.

Detailed C1 implementation: [`../docs/CONTENT_AUTHORING.md`](../docs/CONTENT_AUTHORING.md).

## C0 storage

```text
content/
  platforms.json
  candidates/
    <source-event>-<stable-event-id>.json
```

JSON is used deliberately so the tooling stays standard-library-only, deterministic, and easy to consume from C++, Python, PowerShell, CI, and LLM tooling without adding a YAML dependency. The candidate schema is versioned independently of the platform registry.

A candidate contains two ownership classes:

- **derived evidence**: source commit/event identity, significance signal, `Area`, decisions, constraints, rejected alternatives, directives, `Tested`, `Not-tested`, gates, supersession and references;
- **maintainer-owned metadata**: lifecycle, optional significance override, topics/artifacts, related candidates, platform records and notes.

Re-extraction refreshes derived evidence while preserving maintainer-owned metadata. Deleting a candidate and running `rebuild` reconstructs the derived baseline from durable Git history.

## C0 candidate discovery

The extractor reads the final commit message with `git interpret-trailers`.

`Content:` is the strongest deterministic discovery signal:

```text
Content: none | candidate | major | release
```

If it is absent, discovery is conservative. Structured decision/gate/reference evidence or a complete `Issue` + `Area` + `Tested` knowledge atom may produce `candidate`; an ordinary commit without those signals remains `none`.

`Content: none` always suppresses automatic candidate creation for that event.

## C0 commands

Derive or reconcile one merged-event candidate:

```bash
python scripts/content_fact_pack.py extract --commit HEAD
```

Rebuild/reconcile a range from final Git history:

```bash
python scripts/content_fact_pack.py rebuild --rev-range v0.1.0-alpha.1..HEAD
```

Update review state without changing derived facts:

```bash
python scripts/content_fact_pack.py review \
  --candidate content/candidates/<candidate>.json \
  --state reviewed
```

Attach platform metadata to selected source facts:

```bash
python scripts/content_fact_pack.py platform \
  --candidate content/candidates/<candidate>.json \
  --platform show-hn \
  --state selected \
  --angle-tag deterministic-verification \
  --source-fact-id decisions-001
```

The platform command only edits repository metadata. Even `state: published` is a manual record of something that happened elsewhere; C0 performs no network publication.

## C1 explicit authoring

C1 is provider-neutral Agent tooling. The repository does not embed a second LLM provider SDK/API key.

Build an authoring packet only after an explicit maintainer request:

```bash
python scripts/content_authoring.py prepare \
  --candidate content/candidates/<candidate>.json \
  --maintainer-request "GPU 파티클 설계를 Tistory 개발로그로 써줘" \
  --platform tistory \
  --mode development-log \
  --length long \
  --output /tmp/trace2d-authoring-request.json
```

The packet contains:

- exact explicit request,
- Editorial Brief,
- selected Fact Pack identities/hashes,
- qualified facts and evidence,
- mandatory truth-boundary fact refs,
- dynamic platform profile/hints,
- exact `docs/CONTENT_AUTHOR_STYLE.md` text + hash,
- approved maintainer reference-corpus URLs,
- external-research requirement,
- hard authoring rules.

The active Agent then writes the requested draft from that packet. There is intentionally **no `generate` command** and no merge-triggered drafting workflow.

If the maintainer wants the draft stored, `wrap-draft` can mark it and record truth-boundary treatment:

```bash
python scripts/content_authoring.py wrap-draft \
  --request /tmp/trace2d-authoring-request.json \
  --draft /tmp/body.md \
  --ack-boundary '<candidate-id>#not-tested-001' \
  --output /tmp/draft.md \
  --metadata-output /tmp/draft.meta.json
```

Every `not_tested`, `gates`, `limitations`, and `benchmark_metrics` fact must receive an explicit stored-draft disposition:

```text
included-or-addressed
or
not-material + reason
```

The wrapped output is visibly marked `DRAFT — maintainer review required. Not published.` and metadata remains `publication_mode: manual`.

A chat-only draft does not have to be committed or stored at all.

## Dynamic platform registry

`platforms.json` is data, not schema. Adding another destination normally means adding one registry object with a unique `id` and `publication_mode: manual`.

Fact Packs do not contain hard-coded Tistory, Hacker News, Reddit, X, GeekNews or other platform fields. A candidate may have zero, one or many `platform_records`, and each record may select a different subset of `source_fact_ids` and different `angle_tags`.

C1 reads that same registry at request time. The same evidence can intentionally produce different requested pieces for different platforms/audiences without automatic cross-post rewriting.

## Style and truth authority

`docs/CONTENT_AUTHOR_STYLE.md` guides voice, pacing, explanatory structure and article mode. It is not factual authority.

Trace2D claims come from selected Fact Packs/current repository evidence. Current external facts, when needed by a requested article, must be refreshed under `docs/EXTERNAL_REFERENCE_PROTOCOL.md`.

Style imitation may never fabricate personal anecdotes, history, feelings or motives.

Truth boundaries remain distinct:

```text
implemented != tested
hosted CI != real-GPU evidence
benchmark design != benchmark result
one favorable run != comparative superiority
Agent self-report != independent verification
```

## Validation and non-blocking automation

Run all content-tool fixtures locally with:

```bash
python -m unittest discover -s scripts/tests -p "test_content_*.py" -v
```

`.github/workflows/content-evidence.yml` runs these tests as **advisory/non-blocking** repository tooling.

Only C0 has a `main` push action, and that action stops at candidate/Fact Pack evidence. C1 has no automatic invocation path.

If C0/C1 tooling, GitHub token permission, candidate generation or authoring validation fails, the core Trace2D engine lane remains unaffected. Evidence can be regenerated from Git history, and authoring requests can be retried independently.

## Publication boundary

Neither C0 nor C1 implements:

```text
automatic Tistory/X/Reddit/HN/GeekNews posting
browser posting automation
scheduled publication
automatic comments/replies
engagement-maximization rewriting
automatic platform fan-out
```

The maintainer owns final wording, approval and publication.
