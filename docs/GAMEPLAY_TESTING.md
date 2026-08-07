# Deterministic Gameplay Testing

Trace2D P4 exposes a protocol-independent gameplay scenario runner through `Trace2D::Testing`.

The runner composes the existing scene, fixed-step runtime, deterministic input, and semantic query systems. It does not create a second simulation path and it does not depend on JSON, CLI, MCP, SDL event objects, or GoogleTest.

## Scenario lifecycle

A deterministic scenario follows this lifecycle:

```text
load authored scene
        |
        v
reset seed / scene / input / report
        |
        v
schedule or inject engine input
        |
        v
advance input one frame
        |
        v
advance runtime one frame
        |
        v
run gameplay update for that frame
        |
        v
assert authoritative state
        |
        v
read structured report
```

Example:

```cpp
trace2d::runtime::RuntimeConfig config{};
config.seed = 42;
trace2d::testing::GameplayScenario scenario{config};

scenario.LoadSceneToml(sceneText, "sample.trace2d.toml");
scenario.Reset(42);
scenario.SchedulePress(1, trace2d::input::InputControl::KeyD);
scenario.ScheduleRelease(4, trace2d::input::InputControl::KeyD);

scenario.RunFrames(
    4,
    [](trace2d::testing::GameplayFrameContext& context)
    {
        // Deterministic gameplay update using context.scene and context.input.
    });

const bool passed = scenario.AssertFloatFieldEquals(
    "#player",
    "Transform2D",
    "position.x",
    3.0F);

const trace2d::testing::GameplayScenarioReport& report = scenario.Report();
```

`Reset(seed)` restores the loaded baseline scene, resets runtime frame/time to zero with the requested seed, clears input state and scheduled input, and starts a fresh report.

## Frame semantics

`RunFrames` advances input and runtime exactly one frame at a time.

For frame `N`:

1. scheduled input for `N` is applied
2. the deterministic runtime advances to `N`
3. the optional gameplay update callback runs

The callback therefore observes the same frame number in input and runtime and can consume one-frame `pressed` / `released` transitions without skipping them.

The runner rejects execution if input and runtime frames have already diverged.

## Semantic assertions

Assertions reuse `AgentFacade::QueryOne` rather than maintaining a separate selector implementation.

That preserves existing selector behavior:

- exact, case-sensitive selector parsing
- no match is an explicit failure
- ambiguous matches are an explicit failure
- an arbitrary entity is never selected

The initial P4 assertion surface is deliberately narrow:

```text
AssertFloatFieldEquals(selector, componentType, fieldName, expected)
```

It operates on authoritative component fields already exposed by the agent snapshot contract. `Transform2D` is currently the authoritative component type, with float fields such as `position.x`, `position.y`, `rotation_radians`, `scale.x`, and `scale.y`.

Trace2D does not add reflection, guessed renderer bounds, tolerance rules, or unowned gameplay state just to make assertions look more generic. Additional assertion value kinds should be added when authoritative component state actually exists.

Float equality is exact because deterministic scenario reproduction is the contract. A future approximate assertion should be an explicit API with an authored tolerance rather than hidden behavior in the exact assertion.

## Structured failures

A failed assertion produces `GameplayAssertionFailure` with stable structured data:

- failure code
- semantic selector
- requested component and field
- expected value
- observed value when one exists
- exact runtime frame
- deterministic seed
- human-readable detail
- failure-time runtime snapshot
- input frame and relevant control states
- selected entity snapshot when a unique entity was resolved

Stable failure codes are:

```text
scene_unavailable
invalid_selector
no_match
ambiguous_match
component_missing
field_missing
value_mismatch
```

`GameplayScenarioReport` also records authored immediate/scheduled input events, including their target frame and event type. This makes seed, frame, and input setup available together when reproducing a failure.

Failure context is materialized only when an assertion fails. The runner does not build entity snapshots or serialize reports every simulation frame.

## Determinism

For the same baseline scene, seed, input schedule, frame count, and gameplay update logic, a repeated failing scenario must produce the same failure frame and observed state.

The automated test suite compares complete repeated failure reports to guard this contract.

The scenario runner intentionally performs setup-time copies and allocations when loading/resetting a baseline scene, authoring input schedules, recording report metadata, or materializing assertion results. Those operations are test/setup surfaces rather than the simulation hot path.

`RunFrames` itself advances the already-authored input schedule and fixed-step runtime one frame at a time and does not request inspection/query snapshots until an assertion is explicitly executed.

## CTest integration

The `trace2d_gameplay_tests` GoogleTest executable is discovered by CTest.

Coverage includes:

- authored scene load and reset
- scheduled input and exact frame lockstep
- deterministic gameplay mutation driven by input
- semantic component-field assertions
- expected/observed/frame/seed/selector failure metadata
- failure-time entity/runtime/input snapshots
- `QueryOne` ambiguity propagation
- baseline restoration on reset
- identical reports from repeated failing scenarios

This keeps gameplay behavior visible to the same CI test surface as the rest of Trace2D without making GoogleTest part of the engine-level testing contract.
