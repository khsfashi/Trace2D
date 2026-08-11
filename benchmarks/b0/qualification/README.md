# B0 qualification evidence

A **scored** B0 trial is blocked by `scripts/benchmark_b0.py` until `suite.json` and the task are marked `eligible` and the lane's configured evidence file exists with a positive result.

Do not commit placeholder `qualified: true` files.

## Current state — 2026-08-11

- `godot.generic` — **oracle-qualified** with committed hosted evidence in [`godot-generic.json`](godot-generic.json).
- `godot.agent` — **bridge- and oracle-qualified** with committed hosted evidence in [`godot-agent.json`](godot-agent.json), exact bridge `@satelliteoflove/godot-mcp@4.1.0`.
- `trace2d.agent` — **oracle-qualified** with committed Windows evidence in [`trace2d-agent.json`](trace2d-agent.json).
- coding Agent/model — frozen to `openai-codex-cli@0.144.6` + ChatGPT-managed `gpt-5.5`; owner-local model preflight is proven callable.
- Windows sandbox backend — frozen to `elevated` before any matched lane trial.
- isolation backend — model-free `windows_ntfs_acl_v1` mechanism is qualified; the final integrated backend is frozen as `windows_ntfs_acl_v1_elevated`.
- latest real-model isolation turn — **underlying boundary succeeded**, but the old command-path matcher produced a Windows escaping false negative, so the run failed closed and no matched lane started.

The suite/task remain `qualification_required` / `qualification_candidate`. No scored result exists.

## Frozen final Agent/isolation profile

See [`codex-windows-acl-final-profile-freeze-2026-08-11.json`](codex-windows-acl-final-profile-freeze-2026-08-11.json).

```text
Agent                    openai-codex-cli@0.144.6
model                    gpt-5.5
reasoning                high
permission_profile       :workspace
windows_sandbox_backend  elevated
isolation_backend        windows_ntfs_acl_v1_elevated
network_access           disabled
human_intervention       0
```

Canonical profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

The model, backend and budget must not be changed based on later lane outcomes.

## Owner-local Codex qualification history

Every file below is pre-scoring evidence and is retained rather than rewritten as an engine result.

- [`codex-chatgpt-model-attempt-2026-08-11.json`](codex-chatgpt-model-attempt-2026-08-11.json) — dated API snapshot unavailable through ChatGPT-managed Codex.
- [`codex-chatgpt-recovery-attempt-2026-08-11.json`](codex-chatgpt-recovery-attempt-2026-08-11.json) — early recovery observability gap.
- [`codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json`](codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json) — `gpt-5.6` reached the provider but was unavailable to the owner account.
- [`codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json`](codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json) — frozen `gpt-5.5` preflight passed; the old 90-second child ceiling expired before verdict.
- [`codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](codex-chatgpt-native-windows-isolation-breach-2026-08-11.json) — decisive rejection of the old custom Codex permission profile: external canary read succeeded and leaked while workspace writes were blocked.
- [`codex-windows-acl-probe-cli-shape-attempt-2026-08-11.json`](codex-windows-acl-probe-cli-shape-attempt-2026-08-11.json) — model-free probe stopped on obsolete Codex sandbox CLI shape before ACL qualification.
- [`codex-windows-acl-probe-cmd-transport-attempt-2026-08-11.json`](codex-windows-acl-probe-cmd-transport-attempt-2026-08-11.json) — model-free nested `cmd.exe` transport defect after SID/ACL setup.
- [`codex-windows-acl-probe-freeform-cmd-transport-attempt-2026-08-11.json`](codex-windows-acl-probe-freeform-cmd-transport-attempt-2026-08-11.json) — second model-free command-string transport defect; SID separation and ACL lifecycle remained intact.
- [`codex-windows-acl-backend-qualified-2026-08-11.json`](codex-windows-acl-backend-qualified-2026-08-11.json) — final model-free Windows ACL mechanism qualification passed: workspace write allowed, external read denied, no leak, cleanup succeeded.
- [`codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json`](codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json) — first integrated runner used a clean `CODEX_HOME` without an explicit Windows backend; SID discovery equaled the host SID and failed closed before ACL/model isolation/lane execution.
- [`codex-windows-acl-final-profile-freeze-2026-08-11.json`](codex-windows-acl-final-profile-freeze-2026-08-11.json) — final pre-lane refreeze pins `[windows] sandbox = "elevated"` and `windows_ntfs_acl_v1_elevated`.
- [`codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json`](codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json) — real elevated-Windows model isolation turn actually wrote the workspace, attempted the exact held-out read, received Windows access denied, wrote `DENIED`, leaked no canary, and completed; the run nevertheless failed because the old matcher did not normalize doubled Windows backslashes in Codex's command display.

None of these records is a scored engine result. The latest attempt started zero matched lanes.

## Current corrected isolation contract

The final wrapper is [`scripts/benchmark_b0_codex_windows_acl_wrapper.py`](../../scripts/benchmark_b0_codex_windows_acl_wrapper.py). It:

1. pins Codex built-in `:workspace`,
2. pins `[windows] sandbox = "elevated"`,
3. discovers host and sandbox SIDs and rejects equality,
4. clears stale deny ACEs from a prior abruptly terminated turn,
5. applies an NTFS deny ACE for the sandbox SID to the Trace2D repository,
6. runs the Codex model turn with shell network disabled,
7. removes the deny ACE in `finally`,
8. preserves scrubbed ACL lifecycle evidence.

For the isolation canary verifier only, doubled Windows backslashes in the Codex JSONL command **display** are canonicalized before checking whether the exact canary path was attempted. The raw provider trajectory is not rewritten.

The wrapper also exports `acl-isolation.json` outside package-excluded `.probe-artifacts` for the real isolation probe. This closes the observability gap seen in the latest archive.

## Current owner-local command

After updating the PR #118 checkout, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

Do not run `run_benchmark_b0_codex_chatgpt_calibration_safe.py`; it fails closed by design because its old native-Windows isolation boundary was rejected.

The current runner performs:

```text
gpt-5.5 model preflight
 -> real elevated-Windows ACL isolation canary
 -> only on positive isolation verdict:
    godot.generic   one unscored attempt
    godot.agent     one unscored attempt
    trace2d.agent   one unscored attempt
 -> aggregate report + scrubbed evidence ZIP
```

A lane failure is preserved and does not prevent the later lanes from running.

## Promotion rule

Before `eligible`, the corrected archive must prove:

- real isolation verdict positive,
- candidate workspace write proved,
- exact held-out read attempt observed,
- external read denied,
- canary secret not leaked,
- per-turn ACL apply and cleanup evidence packageable,
- exactly three unscored lane records preserved,
- one frozen Agent/profile/budget identity across all three,
- zero human intervention,
- provider trajectory/usage preserved where exposed,
- independent verifier result present for each lane.

Only after review of those facts may `suite.json` and the task become `eligible` and the predefined repeated scored cohort begin. No best-of-N selection is allowed.

## Determinism note for the Godot Agent lane

The qualification deliberately preserved and learned from two rejected measurement boundaries instead of hiding them:

1. A fixed render-frame count was rejected because uncapped hosted rendering can execute many render frames between fixed physics ticks.
2. A fixed 200 ms game-time window passed once, but a later clean rerun exposed scheduler-phase variance: the two sessions ended with 12 versus 13 physics ticks. Fixed milliseconds were therefore also rejected as the equality boundary for this fixed-physics fixture.

The accepted Q4 protocol uses the bridge's public `step_until` action with:

```text
until = tree.get_nodes_in_group("mcp_watch")[0].physics_ticks >= 12
```

The raw `D` key hold extends beyond that boundary and is force-released by the bridge when stepping stops, so input duration does not define the stop condition. Two clean launch-frozen runs both stopped at exactly 12 physics ticks with `Player.position_x == 2`. Their render-frame counts are retained as evidence but intentionally not compared.

## Required lane evidence shape

Every lane evidence file is JSON format version 1 and records the exact environment used. `godot.agent` additionally records the exact bridge package/integrity and positive authoring/runtime/input/deterministic-step checks.

The generated qualification outputs are evidence material, but committing a positive lane evidence file is a separate explicit review step. This separation prevents a script from silently blessing its own environment.
