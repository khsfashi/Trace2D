# Structured commit knowledge protocol

Last reviewed: 2026-08-10.

Trace2D is expected to receive a large amount of work from coding agents. A normal diff records **what changed**, but often loses the decision context that future agents need to modify the same code safely: constraints, rejected alternatives, deliberate non-goals, validation evidence and unresolved gates.

This document defines a small Git-native knowledge protocol for preserving that context in the history that reaches `main`.

The design is inspired by the public Lore project/paper and long-established Git trailer practice, but it is intentionally adapted to Trace2D's squash-merge workflow and repository-owned source-of-truth hierarchy.

Core rule:

> **The final squash commit on `main` is the authoritative commit-knowledge atom for a substantive Trace2D change.**

Intermediate branch commits may contain useful trailers, but they are not relied on because the repository uses squash merges and those messages may disappear.

Commit knowledge is historical evidence, not a second architecture database. Current compiling code, tests, live PR state, owner decisions, active issue acceptance criteria and committed subsystem contracts still outrank old commit trailers.

---

## 1. Why this exists

A future agent should be able to answer questions such as:

- Why is this implementation intentionally broad or narrow?
- Which alternatives were already considered and rejected?
- Which external or architectural constraint shaped this design?
- What was actually tested before the change entered `main`?
- Which important behavior remained untested?
- Did a human/hardware/license gate exist, and how was it resolved?
- Which earlier decision did this commit supersede?
- Which issue/PR/subsystem does this decision belong to?

without requiring the original chat transcript.

The protocol therefore stores **distilled durable facts**, not private reasoning or raw conversation.

```text
raw Agent conversation / private chain-of-thought
        X

durable project knowledge
        O

Issue
Area
Decision
Constraint
Rejected
Directive
Tested
Not-tested
Gate
Reference
Related
Supersedes
Agent / Model when known and useful
```

---

## 2. Why Git trailers

Git trailers are a native structured footer format supported by Git itself. They can be added or parsed with `git interpret-trailers` and extracted from `git log` through `%(trailers:...)` pretty-format placeholders.

Trace2D therefore does **not** need a separate decision database merely to make commit rationale queryable.

Use one trailer fact per line:

```text
Key: value
```

Repeat a key when multiple independent facts exist.

Keep a blank line between the normal commit body and the trailer block.

Do not put the trailer block in the middle of the message.

---

## 3. Scope: when the protocol is required

Use the structured knowledge block for a substantive final squash commit that does one or more of the following:

- implements or completes an issue/roadmap child,
- changes an engine/runtime/public API or authored-data contract,
- changes determinism, replay, ordering, ownership or concurrency behavior,
- changes performance-sensitive or resource-lifetime behavior,
- introduces or rejects a dependency/integration strategy,
- changes benchmark/evaluator methodology,
- resolves a meaningful bug whose root cause or repair is non-obvious,
- passes through a human/environment/license/hardware gate,
- changes a previously frozen architectural/product decision.

Do **not** force a long trailer block onto a trivial typo, formatting-only edit, mechanical rename or similarly obvious low-risk change that creates no durable decision context.

The goal is high-signal institutional memory, not metadata volume.

---

## 4. Trace2D trailer vocabulary

All values must be factual and concise. Keys may repeat where useful.

### 4.1 Baseline identity

For substantive final squash commits, normally include:

```text
Issue: #<number>
Area: <stable subsystem or program name>
Tested: <actual validation that ran>
```

`Issue:` links the knowledge atom to its owning work item.

`Area:` should use a stable subsystem/program label such as:

```text
particles
sprite
renderer
agent
benchmark
workspace
physics2d
resources
build
```

Do not invent a different spelling for the same area on every commit.

`Tested:` records **what actually ran and passed**, not what should have been run.

Repeat `Tested:` for meaningfully different validation layers.

### 4.2 Decision context

Use when the commit contains a non-obvious design choice:

```text
Decision: <chosen durable design or policy>
Constraint: <fact/limit that materially shaped the choice>
Rejected: <alternative> | <reason it was rejected>
Directive: <high-value warning/instruction for future modifiers>
```

`Decision:` captures the chosen policy when the diff alone does not explain it.

`Constraint:` captures an external or repository-level fact, not a restatement of the implementation.

`Rejected:` must include both the alternative and the reason, separated by ` | `.

`Directive:` is reserved for something a future modifier is genuinely likely to break or rediscover. Do not fill every commit with generic advice such as "keep tests passing."

### 4.3 Verification gaps and gates

Use whenever applicable:

```text
Not-tested: <known meaningful validation gap>
Gate: <gate identity> | <status/evidence at final merge>
```

`Not-tested:` is not an embarrassment; it is durable evidence of the validation boundary that existed when the commit landed.

Never claim a check under `Tested:` if it skipped, was unavailable or was inferred from a different environment.

`Gate:` records a meaningful owner/hardware/license/environment gate and its final disposition. A blocking gate must still be satisfied according to the owning issue/contract before merge; the trailer does not waive it.

Example after a gate has actually passed:

```text
Gate: local-real-gpu-smoke | passed on owner Windows presentation GPU before merge
```

A substantive commit should not enter `main` with an unresolved blocking gate merely because the message documents it.

### 4.4 Decision lineage

Use when useful:

```text
Related: <issue/PR/commit/document identifier>
Supersedes: <commit or decision identifier> | <what is replaced>
Reference: <source/version/date> | ADOPT|ADAPT|REJECT|DEFER | <short lesson>
```

`Related:` links related work without claiming replacement.

`Supersedes:` is important when an old `Directive:`, `Decision:` or `Constraint:` no longer applies. Do not silently leave future agents with two apparently authoritative contradictory history records.

`Reference:` is optional and compact. Durable detailed research belongs in the owning contract or `docs/REFERENCE_PROJECTS.md`; the commit trailer should record only the external lesson that materially shaped this change.

### 4.5 Agent provenance

When known reliably and useful, an agent-authored substantive squash commit may include:

```text
Agent: <agent/product name>
Model: <model/version name>
```

These are declarative provenance, not cryptographic proof.

Do not guess a model/version when it is not exposed reliably.

Provider-generated provenance trailers such as `Agent-Logs-Url:` may be preserved when they are stable, intentional and appropriate for the repository, but Trace2D does not require a proprietary session-log service.

Do not put secrets, private URLs, credentials, raw user prompts, private chain-of-thought or sensitive conversation content into Git history.

---

## 5. Recommended final squash commit shape

Example:

```text
Add explicit GPU particle runtime (#52)

Implement the explicit GPU execution path while preserving the
CPU-reference semantics and authored backend authority.

Issue: #52
Area: particles
Decision: GPU backend remains explicit authored selection
Constraint: GPU runtime consumes the #51 minimized ParticleProgram artifact
Constraint: Normal-frame GPU readback and fence waits are forbidden
Rejected: automatic CPU fallback | violates explicit backend-selection contract
Rejected: per-frame live-count readback | introduces synchronization cost
Directive: Do not make CPU-to-GPU backend selection automatic
Tested: hosted windows-msvc configure/build/ctest
Tested: local ParticleGpuSmokeTests on a presentation GPU
Gate: local-real-gpu-smoke | passed before merge
Related: PR #95
Agent: ChatGPT
Model: GPT-5.6 Sol
```

This is an example of shape, not evidence that #52 has already passed the outstanding real-GPU gate. The actual final message must reflect the real state at merge time.

---

## 6. Squash-merge rule

Trace2D uses squash merges. Therefore:

1. Branch-local commits are working history and may be rewritten/squashed.
2. Useful branch trailers may inform the final record but are not assumed to survive.
3. The ready-to-merge PR should contain enough durable decision/validation context to construct the final squash message.
4. Before a substantive PR is merged, prepare a compact final trailer block that reflects the **final PR head and final validation state**, not an earlier revision.
5. If the agent/tool performing the merge can set the squash commit title/message, it should use the prepared final message.
6. If the owner must merge in GitHub UI, the PR should expose the prepared trailer block so it can be preserved without reconstructing the decision from chat history.
7. If validation changes after the message was prepared, update the trailer block before merge.

A stale `Tested:` or `Gate:` line is worse than omitting it.

---

## 7. Querying the knowledge history

Start with ordinary path/issue history, then ask for the specific trailer facts relevant to the active work.

### Show all structured trailers for commits touching a path

```bash
git log --all --format='%h %s%n%(trailers:only)' -- engine/path
```

### Show directives affecting a path

```bash
git log --all --format='%h %s%n%(trailers:key=Directive,valueonly)' -- engine/path
```

### Show constraints

```bash
git log --all --format='%h %s%n%(trailers:key=Constraint,valueonly)' -- engine/path
```

### Show rejected alternatives

```bash
git log --all --format='%h %s%n%(trailers:key=Rejected,valueonly)' -- engine/path
```

### Show known validation gaps

```bash
git log --all --format='%h %s%n%(trailers:key=Not-tested,valueonly)' -- engine/path
```

### Show gate history

```bash
git log --all --format='%h %s%n%(trailers:key=Gate,valueonly)'
```

### Search issue ownership

```bash
git log --all --format='%h %s%n%(trailers:key=Issue,valueonly)' --grep='#52'
```

### Parse the trailers of one commit with Git itself

```bash
git show -s --format=%B <commit> | git interpret-trailers --parse
```

The exact query may be narrowed with paths, dates, commit ranges or normal Git search options.

Do not treat a non-empty query result as proof that the old decision still applies. Read the current code/contracts and look for later `Supersedes:` records.

---

## 8. Knowledge recovery during `Trace2D next/continue`

For substantive work, the preferred recovery loop is:

```text
recover live task / PR / gates
        ↓
identify affected subsystem and paths
        ↓
read current Trace2D contracts
        ↓
query relevant final-main commit knowledge
        ↓
recover past Decision / Constraint / Rejected / Directive / Not-tested / Gate
        ↓
refresh current external precedents under EXTERNAL_REFERENCE_PROTOCOL
        ↓
ADOPT / ADAPT / REJECT / DEFER
        ↓
implement + validate
        ↓
prepare final squash knowledge atom
        ↓
merge only after real acceptance gates pass
```

Commit history is especially useful for discovering **why an apparently attractive change was deliberately not made**.

If current code/contracts conflict with an old trailer, follow the normal Trace2D source-of-truth hierarchy. If the old decision is being intentionally replaced, preserve the transition with `Supersedes:`.

---

## 9. PR handoff rule

For substantive PRs, the PR description or final pre-merge comment should preserve the information needed to produce the squash record.

A compact section equivalent to this is sufficient:

```text
Squash commit knowledge
Issue: #...
Area: ...
Decision: ...
Constraint: ...
Rejected: ... | ...
Directive: ...
Tested: ...
Not-tested: ...
Gate: ... | ...
Reference: ... | ADAPT | ...
Related: PR #...
Agent: ...
Model: ...
```

Include only fields that carry real signal. `Issue`, `Area`, and the actual `Tested` evidence are the normal substantive baseline; other fields are conditional.

The PR narrative may remain richer than the final commit. The commit stores the durable distilled facts.

---

## 10. Anti-noise and integrity rules

Do not:

- copy the diff into trailers,
- add every trailer key mechanically when no meaningful value exists,
- claim `Confidence: high` as a substitute for evidence,
- record a test that did not actually run,
- turn skipped hardware tests into `Tested:` evidence,
- record an unresolved blocking gate as if documentation satisfies it,
- paste private reasoning or entire chat transcripts into commits,
- store credentials/tokens/private links,
- make a trailer override a current issue/contract,
- preserve obsolete directives without an explicit superseding record when changing them,
- use agent provenance as proof of authorship/security without signatures or another actual attestation mechanism.

Trace2D intentionally does **not** require Lore's `Confidence:`, `Scope-risk:` or `Reversibility:` fields by default. They may be added in a rare case where they carry concrete value, but mandatory subjective metadata tends to become low-signal boilerplate.

---

## 11. External precedents and adoption decisions

### Lore (`tmdgusya/lora`) / Lore paper

References:

- https://github.com/tmdgusya/lora
- https://arxiv.org/abs/2603.15566

Decision: **ADAPT**.

Adopt the core idea that commit messages can preserve otherwise-lost decision context as native Git trailers. Adapt it around Trace2D's squash-main authority, source-of-truth hierarchy, hardware/human gates and verification-heavy workflow. Do not install Lore as a mandatory dependency merely to obtain a format Git already supports natively.

### Git native trailers

References:

- https://git-scm.com/docs/git-interpret-trailers
- https://git-scm.com/docs/pretty-formats

Decision: **ADOPT**.

These are the underlying portable mechanism. Prefer them over a bespoke metadata parser/database until measured needs justify additional tooling.

### Git / Linux patch trailer culture

References:

- https://www.kernel.org/pub/software/scm/git/docs/SubmittingPatches.html
- https://docs.kernel.org/process/submitting-patches.html

Decision: **ADAPT**.

Long-running open-source practice demonstrates that structured footer tags such as `Signed-off-by:`, `Reviewed-by:`, `Tested-by:` and `Fixes:` can carry durable provenance/review/testing information. Trace2D adds decision-specific keys while preserving the same machine-readable footer concept.

### Gerrit `Change-Id`

Reference:

- https://gerrit-review.googlesource.com/Documentation/user-changeid.html

Decision: **REFERENCE / REJECT AS REQUIRED TRACE2D ID**.

It demonstrates that a structured commit footer can bind history to an external review lifecycle across rewritten patch sets. Trace2D already has GitHub issue/PR/commit identity and does not need to invent its own mandatory Change-Id system.

### GitHub Copilot coding-agent provenance

Reference:

- https://github.blog/changelog/2026-03-20-trace-any-copilot-coding-agent-commit-to-its-session-logs/

Decision: **ADAPT**.

GitHub's `Agent-Logs-Url:` trailer is strong current evidence that agent-authored commit provenance can live in structured commit metadata. Trace2D keeps `Agent:` / `Model:` optional and provider-independent and may preserve provider-generated stable log URLs when appropriate.

### `git-ai`

Reference:

- https://github.com/git-ai-project/git-ai

Decision: **DEFER**.

Line-level attribution/session metadata can be useful for auditing, but Trace2D's immediate problem is durable architectural/verification knowledge between coding agents. Do not introduce Git Notes, line-level AI attribution or additional provenance infrastructure until a concrete audit requirement justifies it.

---

## 12. Success criterion

This protocol succeeds when a future agent can inspect a subsystem and recover, from repository state plus Git history:

1. what the current code/contract says,
2. why important design choices were made,
3. which tempting alternatives were already rejected and why,
4. what validation actually existed when those changes landed,
5. which historical gaps/gates matter,
6. whether a later change intentionally superseded an older decision,
7. and enough context to continue work without the original chat transcript.

The desired loop is:

```text
Agent A implements
    ↓
final squash stores durable knowledge
    ↓
Agent B queries current contracts + history
    ↓
Agent B avoids repeated mistakes / stale assumptions
    ↓
new validated decision
    ↓
new final squash knowledge atom
```

The history becomes a lightweight, Git-native institutional memory without becoming a second source of gameplay or architecture truth.