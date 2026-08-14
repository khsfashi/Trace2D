# E0 — External Game / Application Boundary

Issue: #69

## Product boundary

Trace2D now has one source-level external game boundary:

```text
host executable
  -> trace2d::application::Application
       -> external Game
            -> GameContext
                 Runtime (read-only authoritative time)
                 Scene (canonical world state)
                 Input (read-only gameplay input)
                 UI (canonical game UI)
                 optional WorkSpec / WorkResult bindings
```

External game code lives outside `engine/`. The committed proof is `examples/e0_external_game/ExampleGame.*`.

This is a **source-level C++ contract**, not a binary plugin ABI. E0 does not add a service locator, reflection system, ECS, callback/event framework or editor-owned game database.

## Lifecycle and ownership

`Application` owns the canonical runtime/input/scene/UI instances used by a game session. The external `Game` receives three lifecycle callbacks:

1. `OnStart(GameContext&)`
2. `OnFixedUpdate(GameContext&, FixedUpdate)` exactly once per authoritative fixed step
3. `OnStop(GameContext&)`

Game code cannot step `FixedStepRuntime` through `GameContext`, and it receives `InputSystem` read-only. The host does not receive mutable `InputSystem` frame control either: physical/virtual events enter only through `Application::ApplyInput()` or deterministic `Application::ScheduleInput()`.

`ApplyInput()` retains host events in a reusable pending buffer until the next fixed frame. This is required because `InputSystem::AdvanceToFrame()` clears prior transient state. Applying pending host events **after** that clear makes `Pressed`/`Released` edges visible to the game on exactly the next authoritative frame instead of silently losing them.

The fixed order is:

```text
resolve next fixed frame
 -> InputSystem::AdvanceToFrame(nextFrame)
    (clears old transient state and applies deterministic scheduled events)
 -> apply pending host InputEvent values in arrival order
 -> FixedStepRuntime::Step()
 -> Game::OnFixedUpdate(...)
```

This deliberately matches the established headless `GameplayScenario` frame ownership while adding the host-event handoff needed by a real Application boundary.

`FixedStepRuntime::ConsumeElapsed()` separates wall-clock scheduling from authoritative stepping. It returns the number of complete fixed steps while preserving the sub-step remainder, but does not advance simulation. `Application::AdvanceElapsed()` then executes those steps one by one so game logic cannot be skipped when a windowed host catches up multiple frames.

## Headless and windowed are the same game

The external game module contains no SDL, renderer, MCP or backend types.

- `trace2d_e0_external_headless` drives the game without a window/GPU and is a deterministic CTest proof.
- `trace2d_e0_external_windowed` uses `Platform` and `Renderer` only in the host executable. It installs an explicit presentation callback, reads the canonical `game.player` Transform from `GameContext::Scene()`, and submits a visible Sprite at that position while running the same `ExampleGame` class.

Presentation is therefore a host concern over canonical current game state. It does not mutate or replace authoritative fixed-step state. The E0 CI gate requires the windowed host to compile; it does not invent a new real-GPU acceptance authority because Renderer semantics remain owned by the existing renderer/GPU evidence gates.

## Existing Agent / work authority is reused

E0 does not introduce `AgentGame`, a hidden game database, or a second result format.

`GameContext` may receive pointers to the existing #97 `WorkSpec` and #98 `WorkResult` contracts. The external proof consumes an acceptance ID from `WorkSpec`, emits a revision into `WorkResult`, and appends a deterministic `VerificationRecord` at shutdown after checking its canonical Scene/UI state.

For deterministic inspection, existing `AgentFacade` binds directly to the `Application`'s canonical Runtime/Scene/UI objects. The E0 headless proof queries `#game.player` from the same `Scene` that game logic mutates. `Application::Snapshot()` is a bounded derived view for lifecycle/session discovery; it stores no parallel authoritative state.

## Minimum Agent-facing projection

Representative common workflow:

```text
Application + external Game
 -> BindWorkContracts(...) when work intent/result is present
 -> Start()
 -> ApplyInput(...) and/or ScheduleInput(...) when input is needed
 -> StepFrames(...) or AdvanceElapsed(...)
 -> Snapshot() / existing Agent inspection over canonical services
 -> Stop()
```

The authoring/lifecycle core is intentionally bounded to four new concepts: `Application`, `Game`, `GameContext`, and `FixedUpdate`. The representative inspection path adds one derived value type, `ApplicationSnapshot`, for **five exposed E0 concepts total**. It is counted as an exposed concept even though it is not a semantic authority.

The representative external game does not need to know SDL, GPU backend state, MCP transport, internal service registration, renderer pipeline objects, Git metadata or engine source layout.

Structural Agent-complexity evidence for the committed proof:

- new primary authoring/runtime root: `trace2d/application/Application.hpp` — 1,
- exposed E0 concepts used by the representative lifecycle + discovery path — 5,
- external game implementation resources: `ExampleGame.hpp` + `ExampleGame.cpp` — 2,
- required lifecycle semantic operations: `Start`, one stepping operation, `Stop` — 3,
- optional host input operations: `ApplyInput` / `ScheduleInput` — 2 bounded operations, no raw input-frame mutation,
- optional existing work-contract binding operation — 1,
- deterministic session discovery operation: `Snapshot` — 1,
- raw engine-internal/backend/MCP edits required — 0,
- visual-feedback calls required for headless deterministic acceptance — 0,
- parallel Agent semantic authorities introduced — 0.

Token/tool-call measurements remain benchmark evidence rather than a universal E0 constant.

## Performance contract

E0 adds no normal-frame filesystem access, parsing, JSON/string report construction, GPU readback or Agent snapshot generation.

For `F` fixed frames, application orchestration is `O(F + pending/scheduled input work + game work)`. The fixed-step loop reuses retained `Application`, `GameContext`, Runtime, Input, Scene and UI objects. It performs one virtual `Game::OnFixedUpdate` dispatch per fixed step and no required heap allocation of its own per fixed update.

The pending host-input vector reserves reusable capacity during Application construction and is cleared without releasing that capacity after each fixed frame. A host input burst may grow the vector only when it exceeds retained capacity; subsequent frames reuse the high-water allocation. Deterministic scheduled input retains the existing `InputSystem` ownership/complexity.

`Application::Snapshot()` is explicit inspection work and returns scalar counts plus a borrowed `string_view` into canonical Scene metadata. Presentation callbacks are explicit host calls and are not part of authoritative simulation stepping.

## Scope deliberately deferred

E0 does not freeze the external project manifest, install/export/package target, clean external CMake consumer flow, environment doctor or public API version/deprecation policy. Those belong to #70.

Scene hierarchy and external authored gameplay components remain #71. Resources, templates/world lifecycle, Camera/Viewport and later systems retain their existing owners.

## Acceptance mapping

The committed tests/proofs cover:

- external game source outside `engine/`,
- explicit engine-owned lifecycle and fixed-step/input-frame ownership,
- host `Pressed`/`Released` edge preservation on the next authoritative fixed frame,
- identical game logic under explicit/headless and elapsed/windowed-style stepping,
- scene/input/UI access without backend types in `GameContext`,
- existing WorkSpec/WorkResult/VerificationRecord participation,
- existing Agent inspection over the canonical Scene,
- windowed host compilation that renders a Sprite from the same canonical external-game state,
- no binary plugin ABI or generic reflection/ECS/event framework.

After #69 merges green, the live core lane advances to #70 — project manifest and external consumer build/install/package flow.
