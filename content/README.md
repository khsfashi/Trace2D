# Trace2D content evidence (C0)

This directory stores **structured development evidence only**. It is not an article archive, drafting queue, or publication system.

The C0 boundary is:

```text
final-main repository evidence
 -> deterministic candidate discovery
 -> Fact Pack JSON
 -> optional review/platform metadata
 -> stop
```

Drafting belongs to #109 and starts only after an explicit maintainer request. C0 never generates titles, article bodies, social copy, translations, comments, or external posts.

## Storage

```text
content/
  platforms.json
  candidates/
    <source-event>-<stable-event-id>.json
```

JSON is used deliberately for the first implementation so the tooling can stay standard-library-only, deterministic, and easy to consume from C++, Python, PowerShell, CI, and LLM tooling without adding a YAML dependency. The schema is versioned independently of the platform registry.

A candidate contains two ownership classes:

- **derived evidence**: source commit/event identity, significance signal, `Area`, decisions, constraints, rejected alternatives, directives, `Tested`, `Not-tested`, gates, supersession and references;
- **maintainer-owned metadata**: lifecycle, optional significance override, topics/artifacts, related candidates, platform records and notes.

Re-extraction refreshes derived evidence while preserving maintainer-owned metadata. Deleting a candidate and running `rebuild` reconstructs the derived baseline from durable Git history.

## Candidate discovery

The extractor reads the final commit message with `git interpret-trailers`.

`Content:` is the strongest deterministic discovery signal:

```text
Content: none | candidate | major | release
```

If it is absent, discovery is conservative. Structured decision/gate/reference evidence or a complete `Issue` + `Area` + `Tested` knowledge atom may produce `candidate`; an ordinary commit without those signals remains `none`.

`Content: none` always suppresses automatic candidate creation for that event.

## Commands

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
  --candidate <candidate-id-or-json> \
  --state reviewed
```

Attach platform metadata to selected source facts:

```bash
python scripts/content_fact_pack.py platform \
  --candidate <candidate-id-or-json> \
  --platform show-hn \
  --state selected \
  --angle-tag deterministic-verification \
  --source-fact-id decisions-001
```

The platform command only edits repository metadata. Even `state: published` is a manual record of something that happened elsewhere; this tool performs no network publication.

## Dynamic platform registry

`platforms.json` is data, not schema. Adding another destination normally means adding one registry object with a unique `id` and `publication_mode: manual`.

Fact Packs do not contain hard-coded Tistory, Hacker News, Reddit, X, GeekNews or other platform fields. A candidate may have zero, one or many `platform_records`, and each record may select a different subset of `source_fact_ids` and different `angle_tags`.

## Validation and non-blocking automation

Run the fixture suite locally with:

```bash
python -m unittest discover -s scripts/tests -p "test_content_fact_pack.py" -v
```

`.github/workflows/content-evidence.yml` runs this validation as **advisory/non-blocking** repository tooling. On a push to `main`, it derives a candidate from the new final-main commit and, when a candidate exists, attempts to open a small draft repository PR containing only the generated Fact Pack.

If that workflow, GitHub token permission, or candidate generation fails, the core Trace2D engine lane remains unaffected. The candidate can be regenerated later from Git history.
