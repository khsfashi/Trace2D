# Unified verification, diagnosis, repair, and WorkResult

Issue: #98.

This contract sits directly on top of the machine-readable `WorkSpec` from #97. It does not create another task identity model and it does not move repair logic into the engine runtime.

## 1. Purpose

Trace2D needs one reviewable record for the loop:

```text
WorkSpec intent / acceptance
 -> subsystem verification
 -> structured failure context
 -> external Agent edits source/content
 -> re-verification
 -> revision result package
 -> presentation / multimodal / human review where required
 -> human feedback or approval
```

The engine and its deterministic subsystem tools remain correctness authorities for facts they own. `WorkResult` composes their evidence; it does not make an Agent's prose authoritative.

## 2. Versioned result record

The baseline result format is strict versioned TOML and references the exact `work_id` from the corresponding `WorkSpec`. Unknown fields and invalid field types are rejected rather than silently ignored.

```toml
format_version = 1

[result]
work_id = "hit-effect"

[[revisions]]
id = "r1"
changed_paths = ["content/effects/hit.trace2d.toml"]
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "failed"
summary = "Expected semantic state was not observed."
evidence = ["artifacts/r1/verify.json"]
failure_code = "state_mismatch"
failure_target = "effect/hit/alive_count"
failure_message = "Expected 4, observed 3."
reproduction = "trace2d ..."

[[revisions.feedback]]
id = "feedback-1"
target = "effect/hit"
message = "Fix the semantic mismatch before requesting visual approval."

[[revisions]]
id = "r2"
parent = "r1"
changed_paths = ["content/effects/hit.trace2d.toml"]
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "passed"
summary = "Deterministic verification passes after repair."
evidence = ["artifacts/r2/verify.json"]
```

Revision lineage is intentionally linear in V1. The first revision has no parent; every later revision names the immediately preceding valid revision. Malformed revision input is diagnosed instead of being assumed to contain a usable predecessor. This is enough to make an Agent repair loop reviewable without creating a generic version-control graph inside project metadata.

## 3. Verification outcomes

A verification record uses one of:

```text
not_run
passed
failed
review_needed
approved
```

The verification class must correspond to the `WorkSpec` acceptance criterion:

- `deterministic` — engine-owned semantic/structural correctness,
- `presentation` — required presentation evidence,
- `multimodal` — advisory perceptual review,
- `human` — final human judgment.

`passed` or `approved` requires at least one evidence reference. A deterministic/presentation criterion is complete at `passed` or `approved`. Multimodal/human criteria require `approved`; a `passed` or `review_needed` outcome remains review work.

A deterministic/presentation criterion cannot move into the human review queue merely by declaring `review_needed`; if it has not passed its machine-owned check, it remains locally incomplete or failed. This keeps machine-verifiable truth out of the subjective review path.

## 4. Structured failure context

A failed verification record must include:

```text
failure_code
failure_target
failure_message
reproduction
evidence[]
```

The target should be a stable semantic location where possible: acceptance ID, entity/component/property identity, asset/content ID, subsystem fixture, or another engine-owned target. Do not require an Agent to infer a semantic failure from pixels or an unstructured console transcript when the engine already owns the state.

The reproduction string is a bounded actionable invocation/description, not hidden chain-of-thought. Failure-only fields are rejected on non-failed records so stale failure metadata cannot survive unnoticed beside a later pass.

## 5. Revision, artifact, and feedback evidence

Each revision may record:

- changed source/assets/content paths,
- verification records,
- explicit artifacts,
- human feedback,
- known limitations.

Artifacts are typed by a small string `kind` so current and future producers can retain evidence such as:

```text
structured report
performance/resource report
capture/image
video
audio
multimodal review
human approval record
```

The artifact path/ID is evidence metadata. It does not automatically establish the semantic truth of the artifact contents.

Human feedback is explicit revision data rather than hidden chat memory:

```toml
[[revisions.feedback]]
id = "feedback-2"
target = "effect/hit" # optional when no stable target exists
message = "Make the impact feel heavier without changing gameplay damage."
```

`id` and `message` are required. `target` is optional because feedback may refer to the whole result, but when Trace2D has a stable semantic identity the Workspace should provide it. Feedback records user intent; they never mutate engine state by themselves.

Human **approval** remains an acceptance outcome (`approved`) plus any referenced evidence/external truth. Feedback and approval are intentionally separate concepts.

## 6. Result evaluation

`EvaluateWorkResult(spec, result)` evaluates the **current revision** against the acceptance identities and verification classes from the `WorkSpec`.

The local result state is one of:

```text
incomplete
failed
review_needed
complete
```

Rules:

- any current verification failure makes the result `failed`,
- missing/not-run current acceptance remains `incomplete`,
- deterministic/presentation work must pass its machine-owned criterion instead of entering subjective review,
- completed machine verification with pending multimodal/human acceptance is `review_needed`,
- only all locally satisfied acceptance criteria produce `complete`,
- `external_truth` requirements from #97 remain visible separately through `requires_live_truth`; local completion never auto-clears CI/hardware/license/human gates.

Historical failed revisions remain in the `WorkResult` even after a later repair passes. This preserves the diagnose -> repair -> re-verify trail instead of overwriting the evidence that caused the change.

## 7. CLI surface

The first project-level composition tool is:

```powershell
trace2d_verify `
  --spec tests/data/work_spec.trace2d.toml `
  --result tests/data/work_result.trace2d.toml `
  --json
```

It emits machine-readable current state, outstanding/review acceptance IDs, current structured failures, revision count, historical failure count, current revision identity, per-revision verification/artifact/feedback counts, and whether live external truth is still required.

This baseline command **composes** verification records produced by subsystem/tooling work. It does not pretend a manually written `passed` token is an independent verifier. #102's benchmark harness adds stronger trial isolation, verifier provenance, replay, and oracle/mutation self-validation for comparative claims.

## 8. Repair ownership

Trace2D runtime does not silently edit source or assets.

```text
structured failure or human feedback
 -> Agent/user inspects code/content + evidence
 -> external edit
 -> affected deterministic verification reruns
 -> new revision is appended
```

This keeps the engine provider/model independent and prevents a runtime self-repair system from becoming hidden project truth.

## 9. Performance boundary

WorkResult parsing, JSON serialization, captures, report aggregation, feedback records, and revision history are explicit tooling work:

- no frame-loop JSON/TOML generation,
- no per-frame filesystem result writes,
- no generic runtime reflection,
- no unbounded result history retained in gameplay memory,
- no screenshot/readback required for engine-owned semantic verification.

The result record may reference performance/resource evidence, but machine timing remains environment-labelled evidence under the owning subsystem's contract.

## 10. Handoff to #99 and #102

#99 Workspace should consume the same `WorkSpec` + `WorkResult` identities to present intent, recent revisions, deterministic status, artifacts, limitations, review queue, feedback, and approval. It must not reconstruct project truth by scraping console logs.

#102 benchmark should reuse this result vocabulary where practical but retain an independent verifier boundary. Benchmark verdicts cannot be self-declared by the candidate Agent merely by authoring a favorable WorkResult.
