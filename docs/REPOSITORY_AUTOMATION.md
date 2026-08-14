# Repository self-description, automation, and trust contract

Last reviewed: **2026-08-14**.

Trace2D is intended to be developed repeatedly by coding agents from repository state alone. The repository therefore needs more than prose instructions: where inexpensive and stable, it should be able to **check its own contracts, describe its environment/capabilities, expose what work is actually ready, and prove where release artifacts came from**.

This document defines those repository/tooling responsibilities without creating a second project-management database or moving future engine work ahead of the owner-fixed core lane.

Core rule:

> **Prefer repository-derived facts over duplicated planning state, and prefer machine-readable diagnostics over an Agent guessing from prose.**

The references here are precedents, not mandatory dependencies. Trace2D absorbs useful patterns into its own small tooling surface.

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

Trace2D decision: **ADAPT / DEFER separate database**.

Do not introduce Dolt or a second issue database. GitHub issues/PRs plus committed Trace2D contracts remain authoritative. #97 provides machine-readable intent/task state; repository-state tooling derives live `ready`, `blocked`, and next-core state from committed contracts plus GitHub state rather than mirroring them into another database.

### Flutter `doctor`

Reference:

- https://docs.flutter.dev/reference/flutter-cli

Useful precedent: one command summarizes installed tooling/environment readiness before the developer edits code to compensate for an environment problem.

Trace2D decision: **ADAPTED in #70 E1; EXTEND in #78/#92**.

`scripts/trace2d_doctor.ps1` is the first public machine-readable environment preflight for external projects. It is deliberately narrow: project/package/CMake/current Windows-MSVC/vcpkg readiness. #78 extends maintained non-MSVC compiler/platform diagnostics; #92 extends presentation-GPU/backend/smoke eligibility and hardware-evidence boundaries.

### Reproducible Builds

Reference:

- https://reproducible-builds.org/docs/getting-started/

Useful precedent:

- build/package the same source more than once and compare artifacts,
- record/control environment inputs that affect output,
- treat differing artifacts as reproducibility evidence to investigate rather than hand-wave.

Trace2D decision: **ADAPT** for package/release artifacts. Runtime determinism and reproducible binary packaging are related but separate claims.

#70 establishes the first install/package provenance baseline and records the limits of the comparison rather than claiming independent compiler/linker bit reproducibility before measuring it.

### GitHub artifact attestations

Reference:

- https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations

Useful precedent:

- cryptographically bind released artifacts to repository/workflow/commit provenance,
- optionally attest an SBOM,
- let consumers verify the artifact rather than merely trusting a filename/checksum pasted into release notes.

Trace2D decision: **DEFER until a real user-facing binary/SDK release artifact exists**.

#70's CPack ZIP is CI acceptance/provenance evidence, not a published user release. When Trace2D publishes a binary/package users are expected to consume, release work must add attestation/SBOM where supported or explicitly record why the release path cannot yet support it.

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

Trace2D decision: **ADOPT a small public-OSS baseline**, without pretending a score is proof that the engine is secure.

---

## 2. Repository Contract Auditor — active

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

## 3. Machine-readable project readiness — #97 foundation active

#97 established committed WorkSpec/capability readiness state and the repository-state direction needed to answer questions equivalent to:

```text
What is the active core work?
Is it ready or blocked?
What exact blocker remains?
What becomes next if that blocker clears?
Which independent/community tasks are currently non-conflicting?
```

Hard rules remain:

- derive rather than duplicate GitHub issue/PR and committed task state,
- live PR/CI/human-gate state outranks cached prose,
- output has a versioned machine-readable form suitable for an Agent/harness,
- do not introduce Beads/Dolt or another mandatory project-management database without evidence that GitHub + committed project state cannot satisfy the requirement,
- do not auto-clear a recognized human/environment gate.

#102 consumes the same readiness/capability metadata for benchmark eligibility; benchmark tooling is not the authority for normal project progression.

---

## 4. Environment doctor — E1 active, extended by #78/#92

#70 establishes the first practical external-project doctor at:

```text
scripts/trace2d_doctor.ps1
```

Installed/packaged SDKs also carry it under:

```text
share/Trace2D/tools/trace2d_doctor.ps1
```

The command lets an Agent distinguish **environment/setup failure from code failure before editing source**. It emits JSON `format_version = 1` and stable diagnostic categories/exit codes.

The E1 report covers:

```json
{
  "format_version": 1,
  "status": "ok",
  "project": {
    "manifest": {"available": true, "valid": true}
  },
  "cmake": {"available": true, "version": "...", "supported": true},
  "compiler": {
    "available": true,
    "identity": "Visual Studio 17 2022 / x64",
    "tested": false,
    "supported": true
  },
  "vcpkg": {
    "available": true,
    "expected_baseline": "...",
    "actual_revision": "...",
    "baseline_matches": true
  },
  "trace2d_sdk": {"available": true, "version": "..."},
  "headless": {
    "available": true,
    "eligible": true,
    "tested": false,
    "supported": true
  },
  "diagnostics": []
}
```

E1 validates/discovers:

- project root and `trace2d.project.json`,
- stable project ID plus startup/content/package-policy fields,
- CMake availability/minimum version,
- manifest-selected configure/build/test presets,
- maintained Windows-MSVC generator/toolchain identity,
- vcpkg root/toolchain and exact pinned checkout revision,
- installed/extracted SDK metadata/version/baseline,
- local headless eligibility.

The doctor does not execute gameplay, so it must not set `tested=true` merely because files exist. The external-consumer CI gate owns actual configure/build/headless-test evidence.

The E1 test suite includes both a healthy environment and a deliberately missing vcpkg root. The latter must return a stable `vcpkg.missing` setup diagnostic instead of sending an Agent toward engine/game source edits.

Ownership/extensions:

- #70: project-root/manifest/package/CMake/current Windows-MSVC/vcpkg baseline,
- #78: maintained non-MSVC compiler/platform diagnostics,
- #92: presentation GPU/backend/smoke eligibility and real-hardware evidence boundaries.

Hard rules:

- JSON keys/status categories are versioned once public,
- diagnostics are request/setup tooling, never per-frame work,
- a missing tool/device produces an explicit diagnostic rather than causing an Agent to rewrite engine code,
- `available`, `eligible`, `tested`, and `supported` are different states and must not be conflated,
- a doctor cannot auto-clear live CI, real hardware, license, credential, or human gates.

Detailed E1 command/schema/exit contract: `docs/EXTERNAL_PROJECT_E1.md`.

---

## 5. Capability manifest/query — #97/#98 foundation, consumed by #102

Trace2D has an authoritative direction for an Agent and benchmark harness to ask which capabilities are actually implemented and what verification modes exist.

Representative semantics:

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

#97 owns the machine-readable project/task intent vocabulary; #98 composes capability/verification results; #102 consumes the resulting eligibility surface.

---

## 6. Reproducible package/release provenance — E1 baseline active

#70 introduces an installable/CPack SDK and `scripts/external_consumer_gate.ps1`.

The E1 evidence grows from the Reproducible Builds pattern without making a stronger claim than measured:

```text
source commit
+ configure preset/configuration
+ CMake/generator/platform/toolset/compiler-cache identity
+ pinned + actual vcpkg revision
+ canonical installed-tree SHA-256
+ CPack ZIP SHA-256
+ repeated install/package comparison
+ package dependency/license notices
+ explicit reproducibility boundaries
```

The gate requires two installs from the same compiled build tree to produce the same canonical file-tree digest. It generates the ZIP twice and records both hashes plus whether the archive bytes are equal.

Known E1 boundary: this is not yet an independent double compilation. Compiler/linker/debug/archive metadata may make separately rebuilt binaries differ; that variance must be measured before Trace2D claims bit-reproducible binaries. Code signing, release SBOMs and attestations are likewise release-layer work.

Artifact attestations complement checksums; they do not replace deterministic/reproducible build investigation.

When a user-facing binary/SDK release exists, release evidence should grow toward:

```text
source commit / tag
+ build preset/toolchain identity
+ pinned dependency identity
+ artifact SHA-256
+ independent repeated-build result where practical
+ exact dependency/license notices
+ SBOM when useful
+ GitHub artifact attestation for published binaries/packages
```

---

## 7. Public open-source security baseline — active

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

The intended loop is now partially concrete:

```text
Trace2D next/continue
    ↓
live PR / CI / recognized gate
    ↓
repository-ready/project-state query
    ↓
current contracts + commit knowledge
    ↓
current external-reference pass
    ↓
environment doctor when external project/toolchain state is relevant
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

The E1 doctor does not run on every continuation unconditionally. Use it when the active work depends on external project/build/package/toolchain state. Normal engine code changes should not pay setup diagnostics that do not help classify the current task.

---

## 9. Explicit non-goals

Do not introduce, without a new evidenced requirement:

- a mandatory Beads/Dolt database,
- a mandatory Spec Kit installation,
- a second issue tracker mirroring GitHub,
- a generic policy-as-code language for every sentence in the documentation,
- line-by-line AI provenance as a prerequisite for contribution,
- automated claims that a capability is supported merely because a symbol/file exists,
- a security score gate that blocks normal development without an explicit remediation policy,
- a generic asset/shader compiler merely because E1 now packages an SDK,
- release-attestation ceremony for CI-only acceptance artifacts,
- per-frame project-manifest/doctor/provenance work.

The objective is a repository that can explain and check itself **just enough to make autonomous development safer and cheaper**, not a repository whose tooling becomes larger than the engine.
