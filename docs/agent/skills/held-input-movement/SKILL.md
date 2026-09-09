---
name: held-input-movement
description: Use when continuous player or entity movement must remain active while a semantic input action is held across fixed simulation frames.
---

# Held input movement

## Canonical workflow

1. Define semantic movement actions during setup (`Game::OnStart`) and retain their action IDs. Prefer an `Axis1D` action for opposing directions; use button `Held` state when the action is naturally digital.
2. Let the host, test, or Agent feed physical/virtual events through `Application::ApplyInput` or deterministic `Application::ScheduleInput`.
3. In every `Game::OnFixedUpdate`, read `GameContext::Actions()` and sample `Axis1D(...)` or `Held(...)`. Application resolves actions before the callback.
4. Apply movement to canonical gameplay/scene state from that per-fixed-frame value. Scale by the fixed-step policy used by the game; do not couple movement cadence to host key-repeat.
5. Stop movement naturally when the held/axis value returns to zero.

## Authority and ordering

`Application input -> ActionMap resolution -> OnFixedUpdate -> canonical game/scene state`

The ActionMap is semantic input authority for ordinary gameplay. Raw physical `Input()` is a lower-level escape hatch, not the default movement contract.

## Do not

- Do not move only on `Pressed(...)`; it is an edge, not continuous state.
- Do not depend on Win32/SDL/OS key-repeat events for held movement.
- Do not create/rebind actions every fixed frame.
- Do not poll a second platform input path outside `Application` and merge it into gameplay ad hoc.

## Discovery handoff

Load only these surfaces unless a missing signature requires more detail: `input`, `gameplay`, `scene`. Then use the exact public header/API index named by those surfaces.

## Deterministic verification

Schedule a press at a known frame, step multiple frames, and inspect/query canonical position. Schedule release, step again, and prove the position stops changing. For tool-driven verification, the relevant primitive set is `trace2d.input.schedule`, `trace2d.input.inspect`, `trace2d.runtime.step`, `trace2d.query`, and `trace2d.assert_float`.
