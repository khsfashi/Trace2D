# Benchmark B0 frozen Codex cohort

Status: **real elevated-Windows ACL isolation passed; one complete corrected three-lane unscored calibration remains before eligibility**.

This document freezes the real coding-Agent/model/isolation candidate for #102. It does not make B0 scored-eligible and does not claim a comparative result.

## Frozen Agent/model/isolation selection

The committed profile is [`agent-profile.codex-0.144.6.json`](agent-profile.codex-0.144.6.json):

- Agent: OpenAI Codex CLI `0.144.6`,
- ChatGPT Codex CLI selector: `gpt-5.5`,
- provider revision policy: `chatgpt_codex_cli_selector_no_dated_snapshot`,
- reasoning effort: `high`,
- approval policy: `never`,
- built-in permission profile: `:workspace`,
- Windows sandbox backend: `elevated`,
- external isolation backend: `windows_ntfs_acl_v1_elevated`,
- protected-root policy: repository-root NTFS deny for the effective Codex sandbox SID,
- shell network access: disabled,
- session persistence: ephemeral,
- task budget: exactly the committed B0 task budget,
- human interventions: zero.

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

Freeze evidence is [`qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json`](qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json). The same model/backend/task/budget remains frozen; the 100000 input-token budget is **not** raised after observing unscored results.

## Isolation history

The original native-Windows Codex custom permission profile is permanently rejected after a real canary leak. See [`qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json).

The replacement external NTFS ACL mechanism is qualified. It proved a distinct sandbox SID, writable candidate workspace, denied external canary read, no leak, preserved host access and successful ACL cleanup. See [`qualification/codex-windows-acl-backend-qualified-2026-08-11.json`](qualification/codex-windows-acl-backend-qualified-2026-08-11.json).

A first integrated attempt then exposed an unspecified Windows backend and failed before any lane. The final profile was therefore frozen to `[windows] sandbox = "elevated"` while matched lane count was still zero. See [`qualification/codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json`](qualification/codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json).

The next real-model isolation turn behaved correctly but the attempt matcher produced a doubled-backslash Windows-path false negative. Raw JSONL proved the held-out read was attempted and denied without leakage. See [`qualification/codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json`](qualification/codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json).

The matcher now canonicalizes only the display form used for path matching; raw trajectory remains unchanged. Packageable `acl-isolation.json` is also exported outside `.probe-artifacts`.

## First real three-lane calibration attempt

Archive `codex-chatgpt-calibration-20260811-150451-625123c9.zip` is preserved as [`qualification/codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json`](qualification/codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json).

The important result is that the final real-model isolation gate itself **passed**:

- elevated sandbox SID differed from host SID,
- workspace write passed,
- held-out canary read attempt was observed,
- Windows denied it,
- secret leakage was false,
- ACL apply/cleanup both succeeded,
- Codex completed normally.

The lane attempt then exposed three pre-scoring harness details:

- `godot.generic`: independent verifier **passed** at `player / Player / (4, 1)`, but provider input usage was `149255`, above the frozen `100000` input-token budget. The old wrapper incorrectly called this `tool_transport_failure`.
- `trace2d.agent`: independent verifier **passed** at the same semantic result, but provider input usage was `279614`, again above the same frozen budget and again misclassified as transport failure.
- `godot.agent`: the per-turn sandbox SID command timed out after 60 seconds while the Godot editor/MCP stack was already running. The outer harness then hit a `FileNotFoundError` while recursively hashing a disappearing `.godot/shader_cache` directory, so no raw lane record was appended.

These two verifier passes are **not** promoted or cherry-picked into the final calibration cohort. The attempt remains pre-scoring infrastructure evidence.

## Corrections before the final unscored rerun

The frozen model/task/backend/budget are unchanged. Only harness semantics were corrected:

1. sandbox identity discovery now occurs during `CODEX_HOME` setup, before the Godot editor is launched, and the exact in-process identity is reused for that guarded turn;
2. the owner-local stable harness hashes authored workspace files but excludes engine-owned `.godot` cache (`authored_files_excluding_godot_cache_v1`);
3. a completed provider turn over the frozen budget is `budget_exceeded` in the implementation domain, not `tool_transport_failure` in infrastructure;
4. the 100000 input-token limit remains frozen and visible in every record.

These changes are test-covered by hosted `B0 Codex Wrapper` CI and do not alter prompt, verifier, lane selection, model, backend or budget.

## Lane exposure

The benchmark task and adapter lanes remain unchanged:

- `godot.generic`: pinned Godot 4.7.1 through ordinary shell/project files, no Godot MCP.
- `godot.agent`: same Godot plus qualified `@satelliteoflove/godot-mcp@4.1.0`.
- `trace2d.agent`: frozen public `trace2d` CLI + `trace2d_mcp`, no benchmark-only engine API.

## Current owner-local action

From an updated PR #118 checkout on native Windows, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

The retired `run_benchmark_b0_codex_chatgpt_calibration_safe.py` must not be used.

The runner first re-proves real-model isolation, then preserves exactly one new unscored attempt in each lane. A lane result may legitimately be `budget_exceeded`; that is a valid benchmark outcome as long as the record, provider usage, verifier and common frozen profile are preserved. Infrastructure failures remain distinct and are not converted into engine losses.

## Promotion rule

Already proven:

- all three engine/adapter environments have independent qualification evidence,
- Codex `0.144.6` and `gpt-5.5` are callable through the owner ChatGPT session,
- external Windows ACL isolation is qualified and has passed a real model canary turn,
- final elevated backend/profile was frozen before lane zero,
- the first real lane attempt exposed and preserved harness defects rather than hiding them.

Still required before `eligible`:

- one corrected archive containing exactly three unscored lane records,
- positive real-model isolation and packageable per-turn ACL cleanup evidence,
- one common frozen Agent/profile/budget identity across all lanes,
- provider trajectory/usage where exposed,
- zero human intervention,
- independent verifier result for every preserved lane,
- no silent retry or best-of-N selection.

After review, suite/task may become `eligible` and the predefined repeated scored cohort can begin. Every scored attempt, including budget losses and infrastructure outcomes, remains part of the cohort.
