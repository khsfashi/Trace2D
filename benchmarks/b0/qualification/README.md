# B0 qualification evidence

A **scored** B0 trial is blocked by `scripts/benchmark_b0.py` until `suite.json` and the task are marked `eligible` and the lane's configured evidence file exists with a positive result.

Do not commit placeholder `qualified: true` files.

## Current state — 2026-08-11

- `godot.generic` — **oracle-qualified** with committed hosted evidence in [`godot-generic.json`](godot-generic.json).
- `godot.agent` — **bridge- and oracle-qualified** with committed hosted evidence in [`godot-agent.json`](godot-agent.json), exact bridge `@satelliteoflove/godot-mcp@4.1.0`.
- `trace2d.agent` — **oracle-qualified** with committed Windows evidence in [`trace2d-agent.json`](trace2d-agent.json).
- coding Agent/model — frozen to `openai-codex-cli@0.144.6` + ChatGPT-managed `gpt-5.5`; owner-local model preflight is **proven callable**.
- isolation backend — **not qualified**. The attempted native-Windows Codex custom permission profile is rejected after a real owner-local canary leak. The committed Agent profile explicitly says `qualification_pending_external_isolation_backend` until a replacement boundary is proven and frozen before the first lane trial.

All three engine/adapter environments and the real coding model are ready. No matched lane trial has started because the held-out integrity boundary is still unresolved.

## Owner-local Codex qualification history

Every file below is pre-scoring evidence and is retained rather than rewritten as an engine result.

- [`codex-chatgpt-model-attempt-2026-08-11.json`](codex-chatgpt-model-attempt-2026-08-11.json) — dated API snapshot unavailable through ChatGPT-managed Codex.
- [`codex-chatgpt-recovery-attempt-2026-08-11.json`](codex-chatgpt-recovery-attempt-2026-08-11.json) — early recovery observability gap.
- [`codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json`](codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json) — `gpt-5.6` reached the provider but was unavailable to the owner account.
- [`codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json`](codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json) — frozen `gpt-5.5` model preflight passed; the old 90-second isolation process ceiling expired before verdict.
- [`codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](codex-chatgpt-native-windows-isolation-breach-2026-08-11.json) — **decisive integrity evidence**: `gpt-5.5` completed, workspace writes were blocked by Codex policy, and the exact random canary beside the held-out verifier was successfully read and exposed to the model. Classification: `integrity_isolation_breach_native_windows_profile`.

The last record rejects the attempted Codex-internal native-Windows read-deny backend. It is not an engine loss, and the old three-lane calibration must not be rerun until a replacement hard boundary is qualified and integrated.

## Replacement isolation mechanism probe

[`scripts/qualify_benchmark_b0_windows_acl_isolation.py`](../../scripts/qualify_benchmark_b0_windows_acl_isolation.py) is the current next gate. It performs **no model call and no engine trial**. In throwaway local directories it discovers the effective Codex Windows sandbox SID, applies a temporary NTFS deny ACE only to that SID, and requires:

1. sandbox SID differs from the host user SID,
2. Codex built-in `:workspace` permits a normal workspace write,
3. the ACL-protected external random canary cannot be read by the sandbox identity,
4. no canary content leaks,
5. host access remains intact and the temporary deny ACE is removed.

A passing result qualifies only the external ACL mechanism. The full B0 runner still needs a reviewed repo/harness quarantine lifecycle before any lane trial may start. That integration must then replace `qualification_pending_external_isolation_backend` with an exact frozen isolation setting and produce the profile hash shared across all three lanes.

Current owner-local command after updating PR #118:

```powershell
python .\scripts\qualify_benchmark_b0_windows_acl_isolation.py
```

Do not run `run_benchmark_b0_codex_chatgpt_calibration_safe.py`; it now fails closed by design.

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

Every lane evidence file is JSON format version 1 and records the exact environment used:

```json
{
  "schema_version": 1,
  "suite_id": "trace2d-b0",
  "lane_id": "trace2d.agent",
  "qualified": true,
  "engine": {
    "id": "trace2d",
    "version": "...",
    "source_commit": "..."
  },
  "environment": {
    "os": "...",
    "architecture": "..."
  },
  "fixture": {
    "task_id": "b0-semantic-scene-authoring",
    "known_good_passed": true,
    "known_bad_failed": true
  }
}
```

`godot.agent` additionally records the exact bridge package/integrity and positive authoring/runtime/input/deterministic-step checks.

## Fixture oracle command

Before a lane can be eligible, run the independent gold/known-bad check:

```powershell
python scripts/benchmark_b0.py qualify-fixtures `
  --task b0-semantic-scene-authoring `
  --lane trace2d.agent
```

or the corresponding Godot lane after setting the engine binary environment variable.

The generated stdout is evidence material, but committing a positive lane evidence file is a separate explicit review step. This separation prevents a script from silently blessing its own environment.
