# Benchmark B1 — Sprite / animation / particle authoring

Status: **scored suite + verifier dispatch frozen; 27-slot scored policy and owner-Windows Actions automation prepared; automatic zero-score preflight is the next gate.**  
Parent: #100  
Issue: #103  
Predecessors: #59 Complete Sprite and #102 Benchmark B0 are complete.

B1 extends the matched benchmark into content authoring that is expensive to judge from pixels alone:

- import or normalize Sprite content,
- repair trim / pivot / alignment defects,
- author deterministic animation with exact semantic events,
- author or repair particle content under structural/performance constraints,
- retain exact-frame/headless evidence separately from presentation review,
- diagnose and repair at least one seeded content defect.

## Frozen baseline decision

`baseline-qualification.json` is the machine-readable B1 baseline contract.

The preregistered non-scored qualification is complete. `hi-godot/godot-ai` is selected at exact pin `v3.0.6@f3d99dfbd38c9e095edf1467f85bee507ace2c3a`. The comparison evidence and rejection rationale for the other leading candidate are frozen in [`qualification/SELECTION.md`](qualification/SELECTION.md).

The selected bridge's Python dependency graph is also preserved byte-for-byte from qualification workflow `31622618958`, artifact `9151863240`, in [`godot-ai-python-freeze.txt`](godot-ai-python-freeze.txt). Scored execution must reproduce that graph rather than resolving the same source tag against newer dependencies.

## Frozen scored suite

`benchmarks/b1/suite.json` freezes the scored task membership **before any scored comparative run**. The three matched scenarios cover every #103 task class exactly once:

1. `b1-sprite-normalize-repair`
   - Sprite import/normalization,
   - trim/pivot/alignment repair.
2. `b1-animation-exact-event`
   - deterministic animation with exact semantic event timing,
   - exact-frame/headless evidence separated from presentation evidence.
3. `b1-particle-budget-repair`
   - particle structural/performance budget,
   - diagnosis and repair of an intentionally seeded content defect.

Each task keeps the frozen B0 Agent budget unchanged: 300 wall seconds, 80 tool calls, 100k input tokens, 20k output tokens and zero human interventions. `godot.generic` and `godot.agent` receive byte-for-byte identical starter/known-good/known-bad paths and verifier identity; only the interaction adapter differs. `trace2d.agent` receives a semantically matched native Trace2D fixture.

`benchmarks/b1/verifiers.json` freezes verifier identities, deterministic check sets and authority seams. Fixture qualification has now proven every known-good fixture is accepted and every known-bad fixture is rejected through the frozen dispatch; the result is recorded separately in [`fixture-qualification.json`](fixture-qualification.json), so the frozen suite and registry remain unchanged.

## Preregistered scored cohort

[`scored-cohort-v1.json`](scored-cohort-v1.json) commits the complete comparative schedule before any scored B1 result is observed:

- 3 frozen tasks,
- 3 lanes,
- 3 repetitions per task/lane pair,
- exactly **27 scheduled attempts**,
- B0's lane rotation reused exactly,
- an independent rotating task order to reduce temporal-position bias,
- zero automatic retries,
- zero infrastructure replacement trials,
- no early stopping,
- no best-of-N or weighted composite score.

Every scheduled attempt remains evidence whether it succeeds, exceeds budget, fails implementation, loses transport, hits infrastructure failure, or violates integrity. A failed scored slot is never rerolled.

The required ordering is now:

```text
current primary-source refresh                 complete
 -> non-scored Godot Agent qualification       complete
 -> select/pin strongest credible Godot lane   complete
 -> freeze matched B1 tasks + budgets + fixtures   complete
 -> qualify frozen known-good/known-bad verifier dispatch   complete
 -> preregister exact 27-slot scored schedule + environment lock   complete
 -> automatic owner-Windows zero-score preflight   NEXT
 -> one explicit owner approval on PR #174
 -> run exactly 27 scored attempts with same coding Agent
 -> independently reverify all preserved workspaces
 -> aggregate deterministic report
 -> presentation + multimodal/human review evidence
```

The B0 schema, trial isolation, append-only trace rules, retry/exclusion policy and raw metric vocabulary are reused rather than forked. B1 extends only the content task/verifier layer and environment bootstrap required by #103.

## Fixture qualification result

The frozen dispatch is qualified at source head `557c7edf9ee30fd9dca0cc33379731887e79f29a` without changing the frozen task fixtures or verifier registry.

- Godot: official `4.7.1.stable.official.a13da4feb`; all three known-good fixtures accepted and all three seeded known-bad fixtures rejected in workflow `31651157113` / job `94295573573`.
- Trace2D: the native Sprite parser, animation runtime contract and particle parser/compiler were exercised by six qualification CTests; all six passed in workflow `31651157103` / job `94295573606`.
- No scored B1 run has been observed. Fixture qualification proves verifier discrimination only; it is not a comparative benchmark result.

Machine-readable evidence: [`fixture-qualification.json`](fixture-qualification.json).

## Owner-Windows Actions automation

The owner runner remains [`../../scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py`](../../scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py), but routine execution is now wrapped by two GitHub Actions workflows so the owner does not need to drive the benchmark from an interactive PowerShell session.

[`../../.github/workflows/benchmark-b1-owner-preflight.yml`](../../.github/workflows/benchmark-b1-owner-preflight.yml) automatically runs on pushes to the active B1 branch on the existing owner Windows self-hosted runner. It starts **zero scored slots** and:

- purges the persistent self-hosted checkout before fetching the exact branch head,
- restores every frozen benchmark path from its canonical `HEAD` Git blob and verifies that blob still matches the preregistered SHA-256 before materializing it,
- revalidates the frozen suite,
- verifies Codex CLI `0.144.6` login,
- proves the real `CodexSandboxOffline` Windows ACL isolation boundary again,
- downloads/caches official Godot `4.7.1-stable` and verifies the release SHA-512,
- checks out `hi-godot/godot-ai` at exact commit `f3d99df...`, reproduces the qualification Python dependency freeze and starts its loopback streamable-HTTP MCP lifecycle,
- requires a real Codex → Godot MCP tool call on a non-scored fixture,
- builds Trace2D public tooling from detached frozen production commit `31712ca...`,
- builds the B1 native held-out verifier from the benchmark branch,
- locally re-proves every known-good/known-bad Godot and Trace2D verifier pair,
- uploads the scrubbed preflight evidence ZIP.

The canonical-byte materializer exists specifically for persistent Windows worktrees whose older checkout may still contain CRLF materialization. It never weakens the freeze: if the `HEAD` Git blob itself no longer matches the frozen digest, the workflow fails instead of rewriting around the drift.

[`../../.github/workflows/benchmark-b1-owner-scored.yml`](../../.github/workflows/benchmark-b1-owner-scored.yml) is deliberately **not** push-triggered. Scoring can start only when the repository owner adds the existing `github_actions` label to PR #174. The approval job additionally requires:

- PR #174,
- the same-repository `agent/benchmark-b1-content-authoring` head,
- actor `khsfashi`,
- first workflow attempt only,
- a successful `Benchmark B1 Owner Preflight` run for the **exact same head SHA**.

Only then does the owner Windows runner check out that exact approved SHA, repeat frozen-byte validation and the built-in preflights, execute the preregistered 27-slot cohort, independently reverify the preserved workspaces, aggregate the report and upload the scored evidence ZIP.

Once the scored workflow begins slot 1, do **not** rerun the workflow, remove/re-add the approval label, or replace any failed slot. Any scored failure remains evidence.

### Manual fallback only

If GitHub Actions is unavailable, the same runner still supports an interactive zero-score preflight:

```powershell
python scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py --prepare-only
```

After a positive preflight, the fallback scored command is:

```powershell
python scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py
```

These commands are fallback paths, not the normal owner workflow now that Actions automation exists.

The runner creates isolated workspaces, append-only hash-chained `raw.jsonl`, independent reverify records, aggregate deterministic output and a scrubbed evidence ZIP. Credentials, transient Codex homes and `.godot` caches are excluded from the package.

## Product-authority boundary

B1 consumes already-public Trace2D production contracts:

- canonical `.sprite.toml` CPU truth for Sprite metadata,
- `SpriteAnimationClip2D` / `SpriteAnimator2D` exact integer-nanosecond animation authority,
- `.trace2d.particle.toml` particle definitions and existing particle analysis/verification surfaces.

The Trace2D animation fixture uses the ordinary public C++ `SpriteAnimationClip2D::Prepare` surface rather than introducing a benchmark-only animation format. Verifier metadata remains benchmark evidence only and never becomes runtime/content authority.

For the `trace2d.agent` scored lane, the runner exposes the frozen public `trace2d`, `trace2d_sprite_process` and `trace2d_particle_analyze` binaries. It deliberately does **not** inject a benchmark-only scene merely to make the scene-bound `trace2d_mcp` executable fit B1's content fixtures. The held-out verifier is executed only after the Agent turn and is never exposed as an authoring shortcut.

The benchmark is not permission to add a benchmark-shaped production asset model, hidden answer API, benchmark-detection path, normal-frame report maintenance, or a second animation/particle authority.

## Next implementation step

Wait for the automatic exact-head owner-Windows preflight to pass. Then the owner explicitly adds `github_actions` to PR #174 once to authorize the scored cohort. Preserve the resulting evidence ZIP, perform aggregate deterministic review and the separately reported presentation/multimodal review, and do not begin #69 until B1 has reviewable multi-run acceptance evidence.
