# B0 qualification evidence

Status: **complete for #102; live bridge qualification remains maintained in CI**.

A B0 result is trusted only when the environment/bridge oracle, coding-Agent profile, hard held-out isolation boundary, append-only trial evidence, and independent verifier/replay checks are preserved separately.

## Qualified lanes

- `godot.generic` — oracle-qualified in [`godot-generic.json`](godot-generic.json).
- `godot.agent` — bridge/oracle-qualified in [`godot-agent.json`](godot-agent.json), exact bridge `@satelliteoflove/godot-mcp@4.1.0`.
- `trace2d.agent` — oracle-qualified in [`trace2d-agent.json`](trace2d-agent.json).

The immutable `godot-agent.json` records the historical #102 accepted run, which used `step_until physics_ticks >= 12` and observed identical movement in both clean sessions. After #102 completed, always-on CI exposed a one-tick raw-key activation-phase variance: absolute tick 12 could contain 12 or 11 ticks in which Godot had actually observed newly injected `D` input. The maintained live qualifier now preserves absolute `physics_ticks` as evidence but uses `input_ticks >= 12`—a fixture counter incremented only on fixed callbacks where `Input.is_key_pressed(KEY_D)` is true—as the movement equality boundary. See [`GODOT_AGENT.md`](GODOT_AGENT.md). Historical scored evidence is not rewritten.

## Frozen coding Agent

```text
Agent                    openai-codex-cli@0.144.6
model                    gpt-5.5
reasoning                high
approval                 never
permission profile       :workspace
Windows sandbox          elevated
external isolation       windows_ntfs_acl_v1_elevated
network                  disabled
human interventions      0
budget                   300s / 80 tools / 100000 input / 20000 output
```

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

## Isolation history

The original custom native-Windows Codex permission profile is rejected after a real owner-local canary leak:

[`codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](codex-chatgpt-native-windows-isolation-breach-2026-08-11.json)

The replacement external NTFS ACL mechanism was qualified model-free and then proven with the real frozen model. Pre-scoring transport/backend/matcher defects remain preserved in their own evidence files rather than being rewritten as engine losses.

Final mechanism evidence:

[`codex-windows-acl-backend-qualified-2026-08-11.json`](codex-windows-acl-backend-qualified-2026-08-11.json)

Final profile freeze:

[`codex-windows-acl-final-profile-freeze-2026-08-11.json`](codex-windows-acl-final-profile-freeze-2026-08-11.json)

## Accepted unscored calibration

```text
codex-chatgpt-calibration-20260811-163459-3812f9f7.zip
SHA-256 31d1e70938a3e98716559073518bf1e1de5465316f85bafffab4d58880e097fd
```

Acceptance:

[`codex-windows-acl-unscored-calibration-accepted-2026-08-11.json`](codex-windows-acl-unscored-calibration-accepted-2026-08-11.json)

This established suite/task eligibility while preserving all calibration outcomes, including over-budget and verifier-fail cases.

## Accepted scored cohort

```text
codex-chatgpt-scored-20260811-180458-214dfeb0.zip
SHA-256 0625e084b6704258a537de7005a3f9a427d66147663abc7878d5880b2860ea52
```

Acceptance:

[`codex-windows-acl-scored-cohort-accepted-2026-08-11.json`](codex-windows-acl-scored-cohort-accepted-2026-08-11.json)

Results:

[`../RESULTS.md`](../RESULTS.md)

Verified final facts:

- exactly nine preregistered scored records;
- exactly nine independent replay/reverify records;
- zero automatic/replacement retries and no best-of-N;
- one common frozen Agent profile hash;
- zero human intervention in every slot;
- raw and replay SHA chains independently recompute;
- `9/9` workspaces and verifier verdicts independently reproduce;
- real held-out canary read was attempted and denied without leakage;
- ACL apply/cleanup succeeded for the isolation gate and all nine turns (`10/10`);
- package scrub review found no auth file, obvious API key, plaintext canary, raw Windows SID or bearer token.

All nine benchmark statuses are `budget_exceeded` because every trial exceeded the frozen `100000` input-token ceiling. Independent semantic-verifier outcomes remain visible separately: `godot.generic` 1/3 pass, `godot.agent` 3/3 pass, `trace2d.agent` 3/3 pass.

Six Godot `signal 11` crash events across five scored Godot trials are also preserved in the raw trajectories. They were not rerolled and did not prevent final verifier/reverify completion.

## Decision

#102's benchmark-harness acceptance is satisfied. The cohort proves repeatable matched execution, immutable evidence, independent objective verification, explicit failure domains, no favorable retry selection, and independent replay without manually reconstructing core metrics.

The later live-qualification maintenance correction changes no scored record, frozen Agent profile, task, budget or accepted cohort interpretation.

This one-task three-sample B0 result is not a broad engine-superiority claim. The active fixed-order project item is #59 Complete Sprite program, currently S0/#119.
