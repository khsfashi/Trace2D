# Repository self-description, automation, and trust contract

Last reviewed: **2026-08-10**.

Trace2D is intended to be developed repeatedly by coding agents from repository state alone. The repository therefore needs more than prose instructions: where inexpensive and stable, it should be able to **check its own contracts, describe its environment/capabilities, expose what work is actually ready, and prove where release artifacts came from**.

This document defines those repository/tooling responsibilities without creating a second project-management database or moving future engine work ahead of the owner-fixed core lane.

Core rule:

> **Prefer repository-derived facts over duplicated planning state, and prefer machine-readable diagnostics over an Agent guessing from prose.**

The references here are precedents, not mandatory dependencies. Trace2D should absorb the useful pattern into its own small tooling surface.

---

## 1. External precedents and decisions

### GitHub Spec Kit

References:

- https://github.com/github/spec-kit
- https://github.github.com/spec-kit/

Useful precedent:

- intent/specification feeds planning/tasks/implementation,
- explicit checklists and cross-artifact analysis detect inconsistency before implementation,
- large features are decomposed only when the simpler workflow is insufficient.

Trace2D decision: **ADAPT**.

Do not install Spec Kit as a required project framework. Trace2D already owns detailed issue/subsystem/roadmap contracts. Absorb the valuable part as a deterministic **repository contract audit** that detects stale or contradictory machine-checkable invariants.

### Beads

Reference:

- https://github.com/gastownhall/beads

Useful precedent:

- dependency-aware task graph,
- `ready` work is computed from blockers rather than manually guessed,
- machine-readable JSON output,
- links such as blocked/related/superseded preserve long-horizon task context.

Trace2D decision: **ADAPT / DEFER implementation**.

Do not introduce Dolt or a second issue database. GitHub issues/PRs plus committed Trace2D contracts remain authoritative. Once #97 provides machine-readable intent/task state, expose a small repository/project-state query that derives `ready`, `blocked`, and `next-after-unblock` from those sources plus live GitHub state when available.

### Flutter `doctor`

Reference:

- https://docs.flutter.dev/reference/flutter-cli

Useful precedent: one command summarizes installed tooling/environment readiness before the developer edits code to compensate for an environment problem.

Trace2D decision: **ADAPT / DEFER implementation to the owning project/toolchain stages**.

A future Trace2D diagnostic command should provide stable JSON for Agents as well as readable text for humans.

### Reproducible Builds

Reference:

- https://reproducible-builds.org/docs/getting-started/

Useful precedent:

- build the same source more than once and compare artifacts,
- record/control environment inputs that affect output,
- treat differing artifacts as a reproducibility defect to investigate rather than hand-wave.

Trace2D decision: **ADAPT** for package/release artifacts. Runtime determinism and reproducible binary packaging are related but separate claims.

### GitHub artifact attestations

Reference:

- https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations

Useful precedent:

- cryptographically bind released artifacts to repository/workflow/commit provenance,
- optionally attest an SBOM,
- let consumers verify the artifact rather than merely trusting a filename/checksum pasted into release notes.

Trace2D decision: **DEFER implementation until a real packaged/released artifact exists under #70/release work**. Attestation is useful only when users have an artifact they are expected to verify.

### OpenSSF Scorecard / GitHub security tooling

References:

- https://github.com/ossf/scorecard-action
- https://docs.github.com/en/code-security/concepts/code-scanning/codeql/codeql-code-scanning
- https://docs.github.com/en/code-security/how-tos/secure-your-supply-chain/secure-your-dependencies/configure-version-updates

Useful precedent:

- explicit security policy,
- least-privilege GitHub Actions permissions,
- automated static analysis,
- dependency/action update visibility,
- repeatable repository security-health checks.

Trace2D decision: **ADOPT a small public-OSS baseline now**, without pretending a score is proof that the engine is secure.

---

## 2. Repository Contract Auditor — active now

The repository includes `scripts/repository_contract_audit.ps1` and runs it in hosted CI.

Its purpose is deliberately narrower than understanding every sentence in the roadmap. It checks **stable, machine-checkable invariants** that should never silently drift while Agents edit multiple documents.

Initial checks include:

- required governance/contract files exist,
- core Agent entrypoint still points to the external-reference and commit-knowledge protocols,
- benchmark contract still requires the three matched environments and an independent verifier,
- commit-knowledge vocabulary still preserves `Tested`, `Not-tested`, `Gate`, and `Supersedes`,
- Spine remains explicitly human/license gated,
- important local Markdown links in governance documents resolve to real repository files,
- repository automation/security baseline files expected by this contract exist.

The auditor intentionally does **not** infer live PR/issue state from stale Markdown. Live GitHub state remains higher authority than `PROJECT_STATUS.md`.

A failed audit means one of two things:

1. an accidental contract drift should be repaired, or
2. an intentional contract change must update the auditor in the same PR.

Do not weaken an invariant merely to make CI green without documenting the changed owner/product decision.

### Future audit growth

Add checks only when all of the following are true:

- the invariant is durable rather than temporary live state,
- the rule can be checked deterministically with low false-positive risk,
- forgetting it has caused or plausibly causes repeated Agent mistakes,
- there is a single clear contract that owns the rule.

Do not turn the auditor into a second parser for all natural-language documentation.

---

## 3. Machine-readable project readiness — owned by #97/#102

A future repository/project-state query should answer questions equivalent to:

```text
What is the active core work?
Is it ready or blocked?
What exact blocker remains?
What becomes next if that blocker clears?
Which independent/community tasks are currently non-conflicting?
```

Preferred conceptual output:

```json
{
  "core": {
    "issue": 52,
    "state": "blocked",
    "blockers": ["owner-real-gpu-validation"],
    "pull_request": 95
  },
  "next_after_unblock": 53
}
```

The exact command/name is intentionally not frozen before #97.

Hard rules:

- derive rather than duplicate GitHub issue/PR and committed task state,
- live PR/CI/human-gate state outranks cached prose,
- output must have a versioned machine-readable form suitable for an Agent/harness,
- do not introduce Beads/Dolt or another mandatory project-management database without evidence that GitHub + committed project state cannot satisfy the requirement,
- do not auto-clear a recognized human/environment gate.

#102 may consume the same readiness/capability metadata for benchmark eligibility, but benchmark tooling must not become the authority for normal project progression.

---

## 4. Environment doctor — owned primarily by #70 and extended by #78/#92

A future diagnostic command/tool should let an Agent distinguish **environment failure from code failure before editing source**.

Conceptual machine-readable fields include:

```json
{
  "cmake": {"available": true, "version": "..."},
  "compiler": {"available": true, "id": "MSVC", "version": "..."},
  "vcpkg": {"available": true, "baseline": "..."},
  "project_manifest": {"valid": true},
  "headless": {"eligible": true},
  "presentation_gpu": {"available": true},
  "gpu_smoke": {"eligible": true}
}
```

Ownership:

- #70 establishes project-root/toolchain/build/package diagnostics needed by an external consumer,
- #78 extends maintained compiler/platform diagnostics,
- #92 extends real-GPU/backend eligibility and evidence boundaries.

Hard rules:

- JSON keys/status categories are stable/versioned once public,
- diagnostics are request/setup tooling, never per-frame work,
- a missing tool/device produces an explicit diagnostic rather than causing an Agent to rewrite engine code,
- `available`, `eligible`, `tested`, and `supported` are different states and must not be conflated.

---

## 5. Capability manifest/query — owned by #97/#98 and consumed by #102

Trace2D needs an authoritative way for an Agent and benchmark harness to ask which capabilities are actually implemented and what verification modes exist.

Conceptual output:

```json
{
  "sprite_renderer": {
    "available": true,
    "headless_verifiable": true,
    "visual_evidence": true
  },
  "physics2d": {
    "available": false
  },
  "particles_gpu": {
    "available": true,
    "semantic_oracle": "cpu-reference",
    "real_gpu_evidence_required": true
  }
}
```

Hard rules:

- capability truth is derived from versioned engine/project contracts and tests; it is not marketing copy,
- benchmark tasks requiring an unavailable capability are `not eligible`, not Agent failures,
- availability does not imply production-ready breadth,
- semantic verification, presentation evidence, hardware evidence, and human judgment are represented separately where material,
- do not create a giant generic reflection system merely to expose capability metadata.

#97 owns the machine-readable project/task intent vocabulary; #98 may compose capability/verification results; #102 consumes the resulting eligibility surface.

---

## 6. Reproducible package/release provenance — owned by #70 and release work

Once Trace2D produces supported external packages, release evidence should grow toward:

```text
source commit / tag
+ build preset/toolchain identity
+ pinned dependency identity
+ artifact SHA-256
+ repeated-build reproducibility result where practical
+ package dependency/license notices
+ SBOM when useful
+ GitHub artifact attestation for published binaries/packages
```

Do not promise bit-identical binaries before measuring the real toolchain. Code signing, timestamps, debug information, archive metadata and compiler/linker behavior may need explicit normalization.

Start with the Reproducible Builds baseline: build the same source twice and compare artifacts/checksums. Expand environmental variance only after the basic case is stable.

Artifact attestations complement checksums; they do not replace deterministic/reproducible build investigation.

---

## 7. Public open-source security baseline — active now

Trace2D's public-repository baseline is:

- `SECURITY.md` with a non-public vulnerability reporting route,
- least-privilege workflow token permissions,
- Dependabot version updates for GitHub Actions,
- CodeQL C/C++ analysis on maintained repository state,
- OpenSSF Scorecard as an advisory repository-health signal.

Security tooling is **defense in depth**, not engine correctness authority.

Rules:

- a Scorecard number is never a product/security claim by itself,
- security scanners do not replace deterministic tests, fuzz/property tests or manual review,
- external actions should remain current and their permissions should stay minimal,
- security workflow failure is investigated as repository/tooling state; it does not silently waive a core acceptance gate,
- changes to dependencies, distribution or external services still follow the explicit license/security review in `docs/EXTERNAL_REFERENCE_PROTOCOL.md`.

---

## 8. Relationship to `Trace2D next/continue`

The intended mature loop becomes:

```text
Trace2D next/continue
    ↓
live PR / CI / recognized gate
    ↓
repository-ready/project-state query when available
    ↓
current contracts + commit knowledge
    ↓
current external-reference pass
    ↓
environment doctor when relevant
    ↓
capability/eligibility query when relevant
    ↓
implement + verify
    ↓
repository contract audit
    ↓
PR / required human or hardware gate
    ↓
final squash commit knowledge
```

Today, live GitHub inspection and committed documents fill the slots for future `ready`, `doctor`, and `capabilities` queries. Their absence does not authorize guessing or bypassing a gate.

---

## 9. Explicit non-goals

Do not introduce, without a new evidenced requirement:

- a mandatory Beads/Dolt database,
- a mandatory Spec Kit installation,
- a second issue tracker mirroring GitHub,
- a generic policy-as-code language for every sentence in the documentation,
- line-by-line AI provenance as a prerequisite for contribution,
- a release-attestation workflow before there is a real artifact users consume,
- automated claims that a capability is supported merely because a symbol/file exists,
- a security score gate that blocks normal development without an explicit remediation policy.

The objective is a repository that can explain and check itself **just enough to make autonomous development safer and cheaper**, not a repository whose tooling becomes larger than the engine.