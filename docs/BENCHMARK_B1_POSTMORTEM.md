# Benchmark B1 Postmortem — Agent Authoring Surface

> Authority: frozen B1 scored head `6d6904e99ad7060341861cb3823e04591a579bf7`, workflow `31763107941`, artifact `9206626314`, SHA-256 `74ab53220927f557621c96ee7b8df7395010e60c191d3959705ab7ba09f8d4d6`.

This document closes the analytical part of #175. It does not modify, rerun, replace, or reinterpret any B1 scored slot. B1 remains the immutable pre-improvement baseline.

## Executive finding

The frozen benchmark score is **Trace2D Agent 3/9**. However, the independent held-out verifier accepted the final workspace in **9/9 Trace2D trials**, including all **6/6** trials scored unsuccessful. Every unsuccessful Trace2D trial was classified `budget_exceeded` because input tokens exceeded the frozen 100,000-token limit; none exceeded the output-token or tool-call limits, timed out, required human intervention, or failed deterministic verification.

Therefore the demonstrated defect is not that these six final Trace2D resources were semantically or deterministically wrong. The demonstrated defect is that the Agent-facing authoring path consumed too much context to produce verifier-valid content. Runtime/content correctness and Agent usability are separate product dimensions, and B1 measured a gap between them.

A second strong signal is that all nine Trace2D trajectories used shell/file-edit operations only. The scored traces recorded **zero engine-native authoring operations**. The public `trace2d` CLI at the B1 baseline exposes runtime/scene commands (`version`, `doctor`, `run`, `inspect`, `query`, `public-alpha`) but no typed Sprite/Particle authoring mutation. The B1 content lane intentionally did not inject the scene-bound MCP server. In practice the Agent fell back to raw TOML/C++ editing and ad-hoc validation.

## Failure taxonomy

| Trial | Task | Input tokens | Output | Tools | Revisions | Final verifier | Primary category | Evidence-backed secondary factors |
|---|---:|---:|---:|---:|---:|---|---|---|
| `b1-particle-budget-repair-trace2d.agent-scored-r1` | b1-particle-budget-repair | 101,586 | 2,032 | 8 | 2 | pass | `input_token_budget_exhaustion` | `typed_authoring_mutation_absent`, `manual_toml_edit_required`, `first_patch_removed_required_keys`, `non_git_workspace_diff_mismatch` |
| `b1-particle-budget-repair-trace2d.agent-scored-r2` | b1-particle-budget-repair | 103,538 | 2,958 | 10 | 2 | pass | `input_token_budget_exhaustion` | `typed_authoring_mutation_absent`, `manual_toml_edit_required`, `duplicate_key_correction`, `ad_hoc_shell_validation` |
| `b1-sprite-normalize-repair-trace2d.agent-scored-r2` | b1-sprite-normalize-repair | 102,304 | 2,182 | 9 | 2 | pass | `input_token_budget_exhaustion` | `typed_authoring_mutation_absent`, `manual_toml_edit_required`, `duplicate_key_correction`, `non_git_workspace_diff_mismatch` |
| `b1-particle-budget-repair-trace2d.agent-scored-r3` | b1-particle-budget-repair | 164,202 | 3,335 | 13 | 2 | pass | `input_token_budget_exhaustion` | `typed_authoring_mutation_absent`, `manual_toml_edit_required`, `first_patch_removed_required_keys`, `non_git_workspace_diff_mismatch`, `ad_hoc_shell_validation`, `sandbox_helper_setup_noise` |
| `b1-sprite-normalize-repair-trace2d.agent-scored-r3` | b1-sprite-normalize-repair | 132,054 | 2,739 | 11 | 2 | pass | `input_token_budget_exhaustion` | `typed_authoring_mutation_absent`, `manual_toml_edit_required`, `duplicate_key_correction`, `non_git_workspace_diff_mismatch`, `ad_hoc_python_validation`, `sandbox_helper_setup_noise` |
| `b1-animation-exact-event-trace2d.agent-scored-r3` | b1-animation-exact-event | 145,011 | 7,604 | 15 | 2 | pass | `input_token_budget_exhaustion` | `typed_authoring_mutation_absent`, `manual_cpp_edit_required`, `non_git_workspace_diff_mismatch`, `redundant_ad_hoc_structural_validation`, `advisory_presentation_artifact_overhead`, `sandbox_helper_setup_noise` |

### What did **not** fail

- **No deterministic verifier rejection:** all six final workspaces were accepted.
- **No output-token exhaustion:** every failed trial stayed well below 20k output tokens.
- **No tool-call exhaustion:** every failed trial stayed well below 80 tool calls.
- **No wall timeout:** the model turns completed.
- **No human intervention:** all trials remained autonomous.
- **No presentation/VLM disagreement determined the score:** deterministic verification remained authoritative.

### Repeated context amplifiers

1. **Raw text mutation was fragile.** Sprite and Particle runs repeatedly needed a second revision because a patch left a duplicate key or removed a required key before restoring it.
2. **The workspace was intentionally not a Git checkout.** Several trajectories still invoked `git diff`/`git status`. Single-file `git diff` failures emitted a long Git help payload, adding context without validating the resource.
3. **Validation was improvised.** With no compact production-authority authoring/validation command in the visible path, the Agent wrote PowerShell regex checks or Python/TOML assertions and reread full files.
4. **Animation r3 expanded advisory work.** The semantic one-line correction was correct, but extra structural scripts, Git checks, and a presentation SVG pushed the turn to 145,011 input tokens. This reinforces the existing separation between deterministic semantic authority and optional presentation evidence.
5. **Windows sandbox helper noise occurred in all r3 Trace2D trajectories.** It did not invalidate the final workspaces and is not the primary failure category, but a compact authoring path should reduce dependence on repeated shell setup.

## Minimum useful Agent surface by task

### Sprite

The task requires one Sprite resource and seven semantic concepts: region identity, source size, trim offset, trim size, packed rectangle, source-space pivot, and sampling. It should not require the Agent to understand TOML patch mechanics, duplicate-key behavior, Git metadata, or an ad-hoc parser.

**Target interaction:** one typed transactional region mutation, followed by one deterministic validation through the same production Sprite authority.

### Particle

The task requires one Particle resource and six semantic concepts: effect identity, capacity constraint, per-step spawn constraint, maximum lifetime, load-emission state, and one-shot/loop intent. It should not require direct TOML edits or regex-based budget checks.

**Target interaction:** one typed transactional constraint mutation that preserves untouched intent, followed by one deterministic parser/compiler-backed validation.

### Animation

Animation was the strongest Trace2D B1 task at 2/3. Preserve the properties already working well: exact integer-time event semantics, compact state, headless exact-time inspection, and separation of semantic verification from presentation evidence.

The r3 miss does not justify a benchmark-specific animation shortcut. It supports a reusable rule: deterministic inspection should be available as a compact production operation so an Agent does not need to reimplement time/frame/event assertions in shell scripts.

## Agent Complexity Budget

The B1 hard benchmark envelope remains unchanged for comparability unless a future suite preregisters a different one:

- wall time: **300 s**
- input tokens: **100,000**
- output tokens: **20,000**
- tool calls: **80**
- human interventions: **0**

Future authoring contracts must additionally record: input/output tokens, tool calls, distinct exposed concepts, resources touched, revisions, visual-feedback calls, and human interventions.

For the demonstrated **single-resource deterministic repair** class, the product-surface target is stricter than the benchmark maximum:

- one discoverable public authoring root reachable from `trace2d --help`,
- raw text editing is **not required**,
- Git metadata is **not required**,
- at most **one primary semantic mutation** for the resource,
- at most **one deterministic validation call** after that mutation,
- expected authoring revisions: **one**,
- deterministic acceptance requires **zero** visual-feedback calls,
- compact structured output by default; do not echo the full resource unless explicitly requested.

These are authoring-surface requirements for this bounded task class, not universal limits for every future complex workflow.

## Architecture rule derived from B1

Do **not** answer B1 by adding many MCP tools or increasing prompt length. Prefer one discoverable `trace2d` entrypoint with small typed, transactional operations that call existing production parsers/serializers/validators internally.

A valid mutation operation should:

- accept semantic intent rather than arbitrary text replacement,
- load through the canonical production parser,
- apply changes in memory,
- validate before commit,
- write atomically only after validation succeeds,
- preserve unspecified resource intent/identity,
- return a compact typed result/change summary,
- perform no renderer/GPU work and add no ordinary frame-path cost.

This preserves Trace2D’s LLM-first goal without creating benchmark-only answer APIs.

## Follow-up scope

Only two immediate implementation defects are sufficiently demonstrated to justify dedicated work:

1. **#178 Sprite transactional authoring:** region geometry/pivot/sampling mutation + production validation through the public `trace2d` entrypoint.
2. **#179 Particle transactional authoring:** deterministic capacity/spawn/lifetime constraints + production parser/compiler validation through the same discoverable command family.

No new animation-specific implementation issue is required from B1 alone. The animation success principles are documented here and should be reused by later subsystem design.

## B2 entry gate

B2 remains blocked until all of the following are true:

1. this postmortem is committed,
2. #178 Sprite transactional authoring is implemented and independently tested on non-B1 fixtures,
3. #179 Particle transactional authoring is implemented and independently tested on non-B1 fixtures,
4. B1 tasks, verifiers, results and artifact identity remain unchanged,
5. B2 uses new held-out tasks or variants frozen before scoring,
6. budgets, verifier authorities, retry/exclusion rules and baseline identities are preregistered again.

B2 exists to test whether the architecture generalizes after improvement. It is not a rerun-until-win mechanism.

## Durable machine evidence

The repository stores the compact machine-readable extract in `benchmarks/b1/postmortem-v1.json`. That file records each Trace2D trial’s score status, metrics, verifier outcome, primary category, secondary factors, minimum surface, and the exact scored artifact authority.
