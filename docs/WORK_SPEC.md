# Machine-readable work intent and readiness

Issue: #97.

Trace2D needs enough committed project/task state that a fresh Agent can recover **what is being built, what remains, what can be verified mechanically, and what still requires review** without reconstructing the answer from chat history.

This contract is deliberately smaller than a project-management system. GitHub remains the live issue/PR/CI source, and engine/subsystem tests remain correctness authority.

## 1. Authority split

Committed work metadata owns stable local facts:

```text
intent + constraints
+ deliverables
+ acceptance criteria
+ capability requirements
+ declared external-truth requirements
```

Live systems own changing facts:

```text
GitHub PR / issue state
CI result
current environment/toolchain readiness
real hardware evidence
license/owner gates
human approval
```

A committed file may state that one of those live facts is required. It must not cache the current answer and later pretend the cached value is still authoritative.

## 2. Work specification

The baseline is versioned TOML:

```toml
format_version = 1

[work]
id = "hit-effect"
intent = "Create and verify the requested hit effect."
state = "implemented"
constraints = ["Backend choice remains explicit."]

[[deliverables]]
id = "effect"
description = "Authored particle effect"
state = "implemented"

[[requirements]]
deliverable = "effect"
capability = "particles.cpu_reference"
minimum = "tested"

[[acceptance]]
id = "semantic-proof"
deliverable = "effect"
description = "Exact CPU-reference assertions pass."
verification = "deterministic"
state = "verified"

[[acceptance]]
id = "creative-approval"
deliverable = "effect"
description = "Owner approves the visual result."
verification = "human"
state = "review_needed"

[[external_truth]]
id = "owner-approval"
kind = "human_approval"
description = "Final approval remains live human state."
```

Supported work states are:

```text
requested
planned
implemented
verified
review_needed
approved
failed
```

They are explicit workflow evidence, not a hidden scheduler. A `failed` state is a real work/result failure. A missing capability is represented separately as a readiness block.

## 3. Acceptance authority

Every acceptance criterion names one verification class:

- `deterministic` — Trace2D-owned semantic/structural truth,
- `presentation` — required presentation artifact/evidence,
- `multimodal` — advisory perceptual review,
- `human` — final subjective/owner judgment.

`deterministic` and `presentation` criteria are locally complete at `verified` or `approved`.

`multimodal` and `human` criteria are complete only at `approved`. A `verified` multimodal/human criterion remains in the review queue. This prevents a model/tool from silently converting advisory or human judgment into completion.

## 4. Capability catalog

`config/trace2d.capabilities.toml` is a small repository-owned catalog. It does **not** infer support from symbol/file existence.

Each declaration keeps these concepts separate:

```text
available
tested
production_supported
deterministic_verification
presentation_evidence
hardware_evidence
human_judgment
```

Positive `available`, `tested`, or `production_supported` claims require explicit repository evidence references. The schema also enforces:

```text
production_supported => tested => available
```

A task requirement asks for a minimum level (`available`, `tested`, or `production_supported`). Eligibility is then **derived for that task**. There is intentionally no single permanent `supported=true` flag.

An unavailable or insufficient capability produces local readiness `blocked`, with a reason such as `unavailable`, `not_tested`, or `not_production_supported`. It is not counted as an Agent implementation failure.

## 5. Derived local readiness

`trace2d_work_state` parses the committed work spec and capability catalog and derives one of:

```text
ready
blocked
review_needed
complete
failed
```

Example:

```powershell
trace2d_work_state `
  --spec tests/data/work_spec.trace2d.toml `
  --capabilities config/trace2d.capabilities.toml `
  --json
```

The JSON result includes capability eligibility, outstanding acceptance IDs, review IDs, and `requires_live_truth`.

`requires_live_truth=true` is deliberately **not** the same thing as `blocked`. The local repository can be internally ready while a later orchestrator still has to query GitHub, CI, hardware, environment, license, or a human before advancing. #102 may compose those live facts for benchmark admission; this local contract does not fabricate them.

## 6. Performance and ownership

Parsing/evaluation is explicit project/tooling work:

- no frame-loop integration,
- no filesystem lookup in gameplay update paths,
- no generic runtime reflection,
- no hidden database,
- no provider/model dependency.

The structures are finite typed C++ values. TOML parsing may allocate because it happens only on explicit work-state/tool requests.

## 7. External-reference review

The #97 design refresh kept the repository's earlier direction:

- **GitHub Spec Kit — ADAPT:** preserve explicit intent/specification before implementation and deterministic cross-artifact consistency where useful; do not install its framework as Trace2D project truth.
- **Beads — ADAPT:** derive ready/blocked state from declared dependencies/evidence rather than asking an Agent to guess; reject a second mandatory issue database/Dolt store.
- **JSON Schema Draft 2020-12 — DEFER:** useful interoperability precedent, but a second schema language is unnecessary for this first finite TOML contract while Trace2D already uses `toml++` and validates the schema in C++.

The borrowed idea becomes Trace2D-owned only through this committed schema, typed parser, tests, capability evidence, and readiness evaluator.

## 8. Handoff to #98 and #102

#98 should compose verification/diagnosis/repair results against the same work/acceptance identities rather than introducing another task identity model.

#102 should consume capability requirements and local eligibility, then combine them with its own live environment/preflight/GitHub evidence. Benchmark tooling is a consumer, not the authority for normal project state.
