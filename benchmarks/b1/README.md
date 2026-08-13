# Benchmark B1 — Sprite / animation / particle authoring

Status: **scored suite + verifier dispatch frozen; 27-slot scored policy and owner-local harness prepared; scored execution is the next gate.**  
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
 -> owner-local transport/isolation/verifier preflight   NEXT (automated before slot 1)
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

## Owner-local scored execution

The owner-local runner is [`../../scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py`](../../scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py). It performs all setup and non-scored checks **before slot 1**:

- revalidates the frozen suite, policy, qualification evidence and dependency lock,
- verifies Codex CLI `0.144.6` login,
- proves the real `CodexSandboxOffline` Windows ACL isolation boundary again,
- downloads/caches official Godot `4.7.1-stable` and verifies the release SHA-512,
- checks out `hi-godot/godot-ai` at exact commit `f3d99df...`, reproduces the qualification Python dependency freeze and starts its loopback streamable-HTTP MCP lifecycle,
- requires a real Codex → Godot MCP tool call on a non-scored fixture,
- builds Trace2D public tooling from detached frozen production commit `31712ca...`,
- builds the B1 native held-out verifier from the benchmark branch,
- locally re-proves every known-good/known-bad Godot and Trace2D verifier pair.

Optional preflight-only mode performs all of the above but starts zero scored slots:

```powershell
python scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py --prepare-only
```

The scored command re-runs those preflights and, only after they pass, executes the committed 27-slot schedule:

```powershell
python scripts/run_benchmark_b1_codex_windows_acl_scored_cohort.py
```

If setup/preflight fails before slot 1, fixing that environment-only problem and trying again does not reroll a score because no scored result has been observed. If execution fails after slot 1 begins, preserve and upload the generated evidence ZIP; **do not rerun or replace any scored slot**.

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

Run the owner-local preflight/scored cohort without mutating the frozen benchmark. Preserve the resulting evidence ZIP, then perform aggregate deterministic review and the separately reported presentation/multimodal review. Do not begin #69 until B1 has reviewable multi-run acceptance evidence.
