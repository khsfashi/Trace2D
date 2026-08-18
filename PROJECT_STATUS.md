# Trace2D Project Status

Last explanatory handoff update: **2026-08-18**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state. When they disagree, live repository state and the committed lane win.

## Product rule

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Product optimization target:

> **Author -> Run -> Observe -> Verify -> Revise through a compact semantic surface.**

Trace2D does not compete on MCP tool count or mature-engine feature count. The product moat to prove is low Agent discovery/context/revision cost plus deterministic engine-owned verification and a real human-playable review loop. See `docs/PRODUCT_DIRECTION.md`.

## Completed production foundation relevant to B2

The minimum external-game sequence through B2 is complete:

```text
#69 Game/Application
 -> #70 external project / SDK package
 -> #71 Scene hierarchy + typed authored components
 -> #86 unified typed resource lifecycle
 -> #87 scene templates + world lifecycle
 -> #88 Camera2D + Viewport2D
 -> #72 Input Actions / device input / text-IME
 -> #73 deterministic TileSet / TileMap / TileLayer
 -> #74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI
 -> #178 Sprite transactional Agent authoring
 -> #179 Particle transactional Agent authoring
 -> #104 Benchmark B2
```

## Frozen Sprite continuity

The completed production Sprite program remains frozen continuity for later work. In particular:

- #144 / SA0 deterministic Sprite animation timing/frame/event contract remains complete and frozen,
- SR8 renderer conformance and SA4 animation conformance remain trusted,
- SPERF performance evidence remains the production Sprite baseline.

Later work must not weaken these contracts.

## Benchmark B1 — immutable baseline

B1 remains immutable pre-improvement evidence:

- exact scored head: `6d6904e99ad7060341861cb3823e04591a579bf7`,
- owner scored workflow: `31763107941`,
- scored artifact id: `9206626314`,
- artifact SHA-256: `74ab53220927f557621c96ee7b8df7395010e60c191d3959705ab7ba09f8d4d6`,
- Godot Generic: 5/9,
- Trace2D Agent: 3/9,
- selected Godot Agent: 0/9.

The #175 postmortem remains the important interpretation: all six scored-unsuccessful Trace2D slots exceeded the 100k input-token budget while their final workspaces independently verified successfully. This is evidence that runtime capability and Agent usability are separate product properties.

Do not rerun, replace, repair or reinterpret B1.

## Agent Complexity Budget

For demonstrated deterministic authoring/repair classes, preserve the direction:

- one discoverable public Trace2D authoring root,
- no mandatory Git metadata,
- typed semantic mutation instead of mandatory raw representation editing where a production authority exists,
- bounded validation and output,
- low authored revision count,
- zero visual-feedback calls for facts the engine can verify directly,
- no normal-frame repository parsing or Agent tooling work.

Measure discovery time, files/rereads, tokens, tool calls, raw edits versus semantic transactions, revisions, validation calls, visual calls, evidence bytes and human interventions. Do not solve context pressure by merely increasing prompts/budgets or proliferating benchmark-shaped tools.

## Benchmark B2 — complete with full-loop acceptance not proven

B2's original scored benchmark is complete and immutable:

- task: `b2-topdown-combat-v1`,
- three lanes: Godot Generic / selected Godot Agent / Trace2D Agent,
- three trials per lane,
- **9/9 scored slots consumed**,
- automatic retries: 0,
- replacement trials: 0,
- no scored candidate reached deterministic acceptance, so the preregistered human-feedback phase could not begin.

Post-score acceptance v1-v5 preserved scored evidence and repaired only demonstrated general surfaces.

### Acceptance-v5 final diagnosis

V5 is consumed and must not be rerun.

Candidate-free qualification passed on run `32111766482`. The actual initial cohort ran once on `32112146580` and consumed exactly two attempts.

Read-only diagnostic run `32123441180` established the common V5 failure:

- both retained attempts are `agent_result_failure`, failure domain `infrastructure`,
- both `codex exec` subprocesses timed out after exactly 885 seconds,
- `agent-result.json` and `codex-events.jsonl` were absent,
- no valid Agent identity/result completed,
- deterministic verifier authority was false and verifier execution was correctly skipped,
- presentation review was correctly skipped,
- both workspaces nevertheless contained partial authoring side effects.

The last point is important: V5 was not a clean "no authoring happened" failure. The Agent/CLI mutated each workspace but did not complete a valid authoritative turn. Those partial workspaces remain diagnostic evidence only; they must not be retroactively promoted, verified as accepted V5 candidates, or used to reinterpret the consumed cohort.

No Trace2D gameplay-verifier or presentation-quality defect was demonstrated by V5. The common blocker is Agent/CLI completion-result delivery after side effects. The read-only evidence does not justify an automatic V6.

B2 therefore closes with:

- scored benchmark complete and immutable,
- post-score general remediations preserved,
- final full-loop acceptance **not proven**,
- V5 Agent execution/result timeout retained as the final acceptance outcome.

Do not create V6 merely to obtain a passing run. A future successor requires a separately demonstrated general defect, independent qualification and explicit owner promotion.

## Current core lane — #315 playable product proof

After #104 closes, the exact next core-lane item is:

> **#315 — tiny external playable product proof**

#315 comes **before #89 Material2D/Shader2D** and intentionally uses current capabilities.

Required owner loop:

```text
human intent
 -> Agent authors an external game
 -> build/run
 -> deterministic verification
 -> presentation evidence
 -> owner actually plays/reviews it
 -> one concrete owner feedback request
 -> Agent revision
 -> deterministic re-verification
 -> owner approval or preserved rejection
```

This is not another scored benchmark. New engine breadth is blocked during this checkpoint unless the playable proof demonstrates a concrete blocker and the owner explicitly promotes the minimum general fix.

The later #12 flagship game remains the broad production proof after more subsystems mature.

## Evidence-gated follow-ups

#312 Semantic Project Graph / Project Index remains `BLOCKED / RESEARCH-GATED` until the frozen TraceResearch R01 pilot and postmortem justify a retrieval experiment. Do not insert it into the product lane merely because a code graph appears useful.

Security/determinism separation and developer-Agent versus shipped-runtime capability separation remain architectural constraints. Renderer breadth remains evidence-gated; #89 should preserve only bounded seams justified for later 2D work rather than introducing a generic render graph.

## Fixed continuation lane

The owner-fixed continuation is now:

```text
#315 tiny external playable product proof
 -> #89 Material2D / Shader2D
 -> #90 deterministic tween
 -> #76 Physics2D
 -> #77 Audio
 -> #91 profiler
 -> #78 Linux / non-MSVC toolchain
 -> #92 tiered GPU QA
 -> #79 save / migration
 -> #12 broad flagship external game
 -> #60 Mesh2D
 -> #177 Asset Intelligence / Asset IR
 -> #176 native deterministic skeleton
 -> #61 Spine license gate
```

## Continuation rule

The next `@GitHub Trace2D 다음 진행해줘` must resolve live state first.

- If #104 is still open only for administrative closure, preserve all B2 evidence and close it; do not create V6 automatically.
- Once #104 is closed, #315 is the exact next product item.
- During #315, do not jump to #89 or add new broad subsystems unless #315 demonstrates a concrete blocker and the owner explicitly changes the lane.
- Only after #315 closes does the core lane advance to #89.
- If live GitHub state conflicts with this handoff, live issue/PR/CI state plus `config/trace2d.core-lane.json` wins.
