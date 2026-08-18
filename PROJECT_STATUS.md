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

The minimum external-game sequence required before B2 is complete:

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
```

The completed production Sprite program remains frozen continuity. Later work must not weaken its renderer, animation, QA or performance contracts.

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

## Benchmark B2 — scored cohort complete, acceptance closure pending

B2's original scored benchmark is **complete and immutable**.

- task: `b2-topdown-combat-v1`,
- three lanes: Godot Generic / selected Godot Agent / Trace2D Agent,
- three trials per lane,
- **9/9 scored slots consumed**,
- automatic retries: 0,
- replacement trials: 0,
- no scored candidate reached deterministic acceptance, so the preregistered human-feedback phase could not begin.

Post-score work preserved that result and repaired only demonstrated general surfaces, including candidate handoff/public API discovery and later Agent execution/result classification/readiness infrastructure.

### Acceptance-v5 state

V5 is consumed and must not be rerun.

Candidate-free qualification passed on run `32111766482` against main `dcb877338f464478020129a4b3b6e0bba0751c1f`.

The actual V5 initial cohort ran once on run `32112146580` and consumed exactly two attempts. The Actions workflow completed normally, but both retained attempts were:

- status: `agent_result_failure`,
- approximately 887 seconds wall time each,
- recorded tool calls: 0,
- recorded input/output tokens: 0 / 0,
- `agent_identity_ok=false`,
- deterministic verifier authority: false,
- presentation gate: skipped because the Agent result was not authoritative,
- eligible perceptual-review trials: 0,
- review target: null,
- full feedback/revision loop: not proven.

This is **not evidence of a gameplay-verifier or presentation-quality failure**. The blocked layer is Agent execution -> valid Agent result.

### Exact B2 closure rule

The next B2 action is read-only diagnosis of the two already-consumed V5 records/Agent logs/trajectory/workspaces. V5 must never be restarted.

A V6 is permitted only when all of the following are true:

1. read-only V5 evidence identifies a concrete general execution/result-layer defect;
2. the remediation does not weaken or change the frozen task, deterministic verifier, presentation gate, rubric, budget or historical evidence;
3. the remediation is independently qualified without consuming a V5 candidate;
4. the owner explicitly decides that another append-only proof is worth the cost.

If the V5 evidence instead shows provider/capacity/transport instability, an external failure outside a correct Trace2D execution layer, or no actionable general product defect, **close #104 with the acceptance failure preserved**. Successful full-loop acceptance is not required to honestly close a benchmark whose result is failure.

Do not create V6 merely to obtain a passing screenshot or a successful anecdotal run.

## Product checkpoint after B2 — #315

After #104 closes, the exact next core-lane item is:

> **#315 — tiny external playable product proof**

#315 comes **before #89 Material2D/Shader2D**.

It intentionally uses current capabilities to prove the actual user loop:

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

Security/determinism separation and developer-Agent versus shipped-runtime capability separation remain architectural constraints. Renderer breadth remains evidence-gated; #89 should preserve only the bounded seams justified for later 2D work rather than introducing a generic render graph.

## Fixed continuation lane

The owner-fixed continuation is now:

```text
#104 B2 closure
 -> #315 tiny external playable product proof
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

- While #104 is open, continue only the bounded B2 closure path described above; do not jump to #315 or #89.
- If a V5 read-only diagnostic PR exists, finish that exact PR and inspect the retained V5 evidence.
- Do not rerun V5.
- Do not create V6 without the four-part evidence gate above.
- When #104 closes, #315 becomes the exact next product item.
- Only after #315 closes does the core lane advance to #89.
- If live GitHub state conflicts with this handoff, live issue/PR/CI state plus `config/trace2d.core-lane.json` wins.
