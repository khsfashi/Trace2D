# Trace2D Workspace implementation baseline

Issue: #99. Product contract: [`WORKSPACE.md`](WORKSPACE.md).

This document records the first concrete Workspace client contract implemented on top of #97 `WorkSpec`, #98 `WorkResult`, and the existing protocol-independent Agent inspection vocabulary.

## 1. Boundary

The Workspace is a **derived review client**.

```text
WorkSpec
 + WorkResult
 + optional existing Agent InspectionSnapshot
        |
        v
WorkspaceSnapshot
        |
        +-> text / JSON client
        +-> self-contained HTML review surface
        +-> future native/web client

human feedback / approval
        |
        v
versioned Workspace action packet
        |
        v
external Agent/user workflow
 -> source/content edit
 -> deterministic re-verification
 -> appended WorkResult revision
```

There is no Workspace-owned gameplay/project truth. The Workspace does not silently mutate source, authored assets, runtime state, verification outcomes, or human approval.

The file-backed CLI deliberately does not instantiate a hidden project runtime. A client already attached to the engine may pass the existing `AgentFacade::Inspect()` `InspectionSnapshot` into `BuildWorkspaceSnapshot`; scene/entity semantic IDs and component state therefore remain the same shared Agent state rather than a GUI mirror.

## 2. Derived snapshot

`trace2d::agent::WorkspaceSnapshot` contains only review-facing derived/copy state:

- work ID and human intent,
- current WorkResult state and revision,
- deliverable progress (`planned`, `working`, `verified`, `review_needed`, `failed`, `approved`),
- acceptance class/outcome/evidence/failure context,
- current subjective/human review queue,
- current changed paths,
- current artifacts and limitations,
- revision history and historical feedback,
- live external-truth requirements declared by the WorkSpec,
- optional existing protocol-independent runtime/scene inspection snapshot.

Progress is not read from an opaque Agent session. It is derived from the current revision's verification records plus authored WorkSpec identities.

### Review eligibility

The Workspace review queue is built from `EvaluateWorkResult`.

That means:

- deterministic/presentation failure remains machine-owned failure,
- deterministic/presentation `review_needed` remains incomplete rather than becoming a subjective queue item,
- multimodal/human `passed` or `review_needed` remains review work,
- multimodal/human work becomes complete only at explicit `approved`.

A screenshot or video cannot promote a failing deterministic check into human review.

## 3. Human action packets

Human feedback is intent, not an engine mutation. The first client emits strict versioned TOML packets:

```toml
format_version = 1

[action]
kind = "feedback"
work_id = "workspace-feedback-loop"
revision = "revision-2"
acceptance = "feel-review"
target = "effect/hit"
message = "Make the impact feel heavier without changing gameplay damage."
```

Approval is separate:

```toml
format_version = 1

[action]
kind = "approve"
work_id = "workspace-feedback-loop"
revision = "revision-2"
acceptance = "feel-review"
target = "acceptance/feel-review"
```

Rules:

- actions are bound to the current work ID and current revision,
- stale-revision actions are rejected,
- feedback requires a non-empty message,
- feedback may target the whole result or carry a stable semantic target/acceptance identity,
- approval must target an acceptance item that is currently present in the Workspace review queue,
- unknown packet fields and unsupported format versions are rejected,
- consuming an action does not itself mark WorkResult verification or approval complete; the external Agent/user flow must append the resulting evidence/revision explicitly.

## 4. CLI / local HTML client

Review the committed representative flow:

```powershell
trace2d_workspace `
  --spec tests/data/workspace_spec.trace2d.toml `
  --result tests/data/workspace_result.trace2d.toml
```

Machine-readable view:

```powershell
trace2d_workspace `
  --spec tests/data/workspace_spec.trace2d.toml `
  --result tests/data/workspace_result.trace2d.toml `
  --json
```

Generate a local review surface:

```powershell
trace2d_workspace `
  --spec tests/data/workspace_spec.trace2d.toml `
  --result tests/data/workspace_result.trace2d.toml `
  --html workspace.html
```

The HTML is dependency-free and self-contained except for referenced review artifacts. Image/video/audio extensions are previewed with native browser media elements; other artifacts remain explicit links. Artifact generation/readback itself is not performed by the Workspace.

Keep the generated HTML where project-relative artifact paths resolve correctly, normally at the project/work-result review root.

Create targeted feedback:

```powershell
trace2d_workspace `
  --spec tests/data/workspace_spec.trace2d.toml `
  --result tests/data/workspace_result.trace2d.toml `
  --feedback "Make the impact feel heavier without changing gameplay damage." `
  --acceptance feel-review `
  --target effect/hit `
  --action-out workspace-action.toml
```

Create an approval packet only for the current review queue:

```powershell
trace2d_workspace `
  --spec tests/data/workspace_spec.trace2d.toml `
  --result tests/data/workspace_result.trace2d.toml `
  --approve feel-review `
  --action-out workspace-action.toml
```

## 5. Committed feedback-loop proof

`tests/data/workspace_spec.trace2d.toml` and `tests/data/workspace_result.trace2d.toml` preserve this exact representative flow:

```text
revision-1 AI/authored change
 -> deterministic semantic verification PASS
 -> human feel review item + preview artifact
 -> human feedback: make the impact heavier without changing gameplay damage
 -> revision-2 authored presentation change
 -> deterministic semantic re-verification PASS
 -> revised preview returns to human review
```

The feedback is stored on revision 1, the changed result is a distinct revision 2, and the deterministic acceptance is rerun after the subjective change. This prevents the Workspace from treating user feedback as a hidden in-place mutation.

## 6. Current Agent/world inspection seam

#99 does not add another scene/world API.

The existing `InspectionSnapshot` already contains:

- runtime frame/seed/fixed-step time,
- scene semantic ID/name,
- entities by stable semantic ID/name/tags,
- authoritative transforms/bounds/components.

`BuildWorkspaceSnapshot(spec, result, &inspection)` copies that existing snapshot into the review packet when a live client has one. Offline WorkSpec/WorkResult review leaves it absent rather than inventing world state from files.

Future Workspace UI technology may render this state differently, but it must keep the same authority boundary.

## 7. External-reference review — 2026-08-11

Primary current references reviewed before freezing the baseline:

### GitHub Copilot coding-agent output review

Official: `https://docs.github.com/en/copilot/how-tos/copilot-on-github/use-copilot-agents/review-copilot-output`

**ADAPT**:

- finished Agent work enters an explicit human review state,
- the reviewer may request changes and let the Agent revise,
- final human merge/approval remains a distinct act.

**REJECT as Trace2D truth**:

- PR comments/conversation/session state are not a substitute for engine-owned semantic verification or committed WorkResult state.

### GitHub Copilot app issue/PR sessions

Official: `https://docs.github.com/en/enterprise-cloud@latest/copilot/how-tos/github-copilot-app/managing-issues-and-pull-requests`

**ADAPT**:

- one review surface should summarize work, checks, review activity, and an ask-Agent-to-fix path,
- users should be able to move from result inspection directly into targeted revision requests.

**REJECT**:

- hidden/background session state as a required project state model,
- UI-specific state becoming authoritative over source/tests/engine semantics.

The useful precedent is the review/steer loop, not GitHub's underlying product/session architecture.

## 8. Performance / ownership

- Workspace snapshot construction is explicit tooling work.
- No Workspace snapshot, JSON, HTML, action TOML, revision list, or artifact preview is produced in the normal frame loop.
- The HTML client performs no continuous GPU readback or filesystem rescan.
- `InspectionSnapshot` is copied only when explicitly supplied by a live client.
- Histories remain bounded by the explicit WorkResult supplied to the review operation; the Workspace does not maintain an independent unbounded session log.
- There is no browser framework, HTTP server, WebSocket layer, editor database, tracing GC, or runtime reflection dependency introduced by #99.

## 9. #102 handoff

The Workspace makes review/feedback observable, but it does not prove autonomous-engine superiority.

#102 Benchmark B0 remains responsible for:

- matched competitor harnesses,
- independent verifier/provenance,
- task eligibility and environment preflight,
- multi-run success/revision/token/tool/human-intervention measurements,
- replay/self-determinism and oracle/known-bad validation.

Workspace output may be useful evidence to a benchmark trial, but benchmark truth must not depend on trusting a Workspace or Agent-authored completion claim.
