#include <trace2d/testing/GameplayScenario.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace trace2d::testing
{
namespace
{
agent::FieldValue MakeFloatValue(const float value) noexcept
{
    agent::FieldValue result{};
    result.kind = agent::FieldValueKind::Float;
    result.floatValue = value;
    return result;
}

GameplayAssertionFailureCode MapQueryFailure(const agent::QueryErrorCode code) noexcept
{
    switch (code)
    {
    case agent::QueryErrorCode::SceneUnavailable:
        return GameplayAssertionFailureCode::SceneUnavailable;
    case agent::QueryErrorCode::InvalidSelector:
        return GameplayAssertionFailureCode::InvalidSelector;
    case agent::QueryErrorCode::NoMatch:
        return GameplayAssertionFailureCode::NoMatch;
    case agent::QueryErrorCode::AmbiguousMatch:
        return GameplayAssertionFailureCode::AmbiguousMatch;
    }

    return GameplayAssertionFailureCode::InvalidSelector;
}
} // namespace

std::string_view ToString(const GameplayAssertionFailureCode code) noexcept
{
    switch (code)
    {
    case GameplayAssertionFailureCode::SceneUnavailable:
        return "scene_unavailable";
    case GameplayAssertionFailureCode::InvalidSelector:
        return "invalid_selector";
    case GameplayAssertionFailureCode::NoMatch:
        return "no_match";
    case GameplayAssertionFailureCode::AmbiguousMatch:
        return "ambiguous_match";
    case GameplayAssertionFailureCode::ComponentMissing:
        return "component_missing";
    case GameplayAssertionFailureCode::FieldMissing:
        return "field_missing";
    case GameplayAssertionFailureCode::ValueMismatch:
        return "value_mismatch";
    }

    return "unknown_gameplay_assertion_failure";
}

GameplayScenario::GameplayScenario(const runtime::RuntimeConfig& runtimeConfig)
    : runtime_{runtimeConfig}
{
    SyncReportFrame();
}

GameplaySceneLoadResult GameplayScenario::LoadSceneToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    scene::SceneLoadResult loadResult = scene::LoadSceneToml(text, sourceName);
    if (!loadResult.Succeeded())
    {
        const std::uint64_t seed = runtime_.State().seed;
        baselineScene_.reset();
        activeScene_.reset();
        runtime_.Reset(seed);
        input_.Reset();
        report_ = GameplayScenarioReport{.frame = 0, .seed = seed};

        GameplaySceneLoadResult result{};
        result.diagnostics = std::move(loadResult.diagnostics);
        return result;
    }

    LoadScene(std::move(*loadResult.scene));
    return GameplaySceneLoadResult{.loaded = true};
}

void GameplayScenario::LoadScene(scene::Scene scene)
{
    const std::uint64_t seed = runtime_.State().seed;
    baselineScene_ = std::move(scene);
    Reset(seed);
}

void GameplayScenario::Reset(const std::uint64_t seed)
{
    if (baselineScene_.has_value())
    {
        activeScene_ = *baselineScene_;
    }
    else
    {
        activeScene_.reset();
    }

    runtime_.Reset(seed);
    input_.Reset();
    report_ = GameplayScenarioReport{.frame = 0, .seed = seed};
}

void GameplayScenario::InjectInput(const input::InputEvent& event)
{
    input_.ApplyEvent(event);
    report_.inputEvents.push_back(ScenarioInputEvent{
        .frame = input_.CurrentFrame(),
        .event = event,
        .scheduled = false,
    });
}

void GameplayScenario::ScheduleInput(const std::uint64_t frame, const input::InputEvent& event)
{
    input_.Schedule(frame, event);
    report_.inputEvents.push_back(ScenarioInputEvent{
        .frame = frame,
        .event = event,
        .scheduled = true,
    });
}

void GameplayScenario::SchedulePress(const std::uint64_t frame, const input::InputControl control)
{
    ScheduleInput(
        frame,
        input::InputEvent{
            .control = control,
            .type = input::InputEventType::Press,
        });
}

void GameplayScenario::ScheduleRelease(const std::uint64_t frame, const input::InputControl control)
{
    ScheduleInput(
        frame,
        input::InputEvent{
            .control = control,
            .type = input::InputEventType::Release,
        });
}

void GameplayScenario::RunFrames(const std::uint64_t count, const GameplayFrameUpdate& update)
{
    if (!activeScene_.has_value())
    {
        throw std::logic_error{"Gameplay scenario requires a loaded scene before RunFrames."};
    }

    if (input_.CurrentFrame() != runtime_.State().frame)
    {
        throw std::logic_error{"Gameplay scenario input and runtime frames must remain in lockstep."};
    }

    for (std::uint64_t index = 0; index < count; ++index)
    {
        const std::uint64_t currentFrame = runtime_.State().frame;
        if (currentFrame == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error{"Gameplay scenario frame counter overflow."};
        }

        const std::uint64_t nextFrame = currentFrame + 1U;
        input_.AdvanceToFrame(nextFrame);
        runtime_.Step();

        if (update)
        {
            GameplayFrameContext context{
                .frame = nextFrame,
                .scene = *activeScene_,
                .input = input_,
                .runtime = runtime_,
            };
            update(context);
        }
    }

    SyncReportFrame();
}

bool GameplayScenario::AssertFloatFieldEquals(
    const std::string_view selector,
    const std::string_view componentType,
    const std::string_view fieldName,
    const float expected)
{
    const agent::FieldValue expectedValue = MakeFloatValue(expected);

    if (!activeScene_.has_value())
    {
        AppendFailure(
            GameplayAssertionFailureCode::SceneUnavailable,
            selector,
            componentType,
            fieldName,
            expectedValue,
            std::nullopt,
            "No scene is loaded in the gameplay scenario.");
        return false;
    }

    const agent::AgentFacade facade{&runtime_, &*activeScene_};
    agent::QueryOneResult query = facade.QueryOne(selector);
    if (!query.Succeeded())
    {
        const agent::QueryError error = query.error.value_or(
            agent::QueryError{
                .code = agent::QueryErrorCode::InvalidSelector,
                .message = "Semantic query failed without a structured error.",
            });
        AppendFailure(
            MapQueryFailure(error.code),
            selector,
            componentType,
            fieldName,
            expectedValue,
            std::nullopt,
            error.message);
        return false;
    }

    agent::EntitySnapshot entity = std::move(*query.match);
    const auto component = std::find_if(
        entity.components.begin(),
        entity.components.end(),
        [componentType](const agent::ComponentSnapshot& candidate)
        {
            return candidate.type == componentType;
        });

    if (component == entity.components.end())
    {
        AppendFailure(
            GameplayAssertionFailureCode::ComponentMissing,
            selector,
            componentType,
            fieldName,
            expectedValue,
            std::nullopt,
            "Selected entity does not expose the requested authoritative component.",
            std::move(entity));
        return false;
    }

    const auto field = std::find_if(
        component->fields.begin(),
        component->fields.end(),
        [fieldName](const agent::ComponentFieldSnapshot& candidate)
        {
            return candidate.name == fieldName;
        });

    if (field == component->fields.end())
    {
        AppendFailure(
            GameplayAssertionFailureCode::FieldMissing,
            selector,
            componentType,
            fieldName,
            expectedValue,
            std::nullopt,
            "Requested component field is not exposed by the authoritative snapshot.",
            std::move(entity));
        return false;
    }

    const agent::FieldValue observedValue = field->value;
    if (observedValue == expectedValue)
    {
        return true;
    }

    AppendFailure(
        GameplayAssertionFailureCode::ValueMismatch,
        selector,
        componentType,
        fieldName,
        expectedValue,
        observedValue,
        "Observed component field did not equal the expected deterministic value.",
        std::move(entity));
    return false;
}

const GameplayScenarioReport& GameplayScenario::Report() const noexcept
{
    return report_;
}

const runtime::FixedStepRuntime& GameplayScenario::Runtime() const noexcept
{
    return runtime_;
}

const input::InputSystem& GameplayScenario::Input() const noexcept
{
    return input_;
}

scene::Scene* GameplayScenario::ActiveScene() noexcept
{
    return activeScene_.has_value() ? &*activeScene_ : nullptr;
}

const scene::Scene* GameplayScenario::ActiveScene() const noexcept
{
    return activeScene_.has_value() ? &*activeScene_ : nullptr;
}

GameplayFailureSnapshot GameplayScenario::MakeFailureSnapshot(
    const std::optional<agent::EntitySnapshot>& entity) const
{
    const runtime::RuntimeState state = runtime_.State();

    GameplayFailureSnapshot snapshot{};
    snapshot.runtime = agent::RuntimeSnapshot{
        .frame = state.frame,
        .seed = state.seed,
        .fixedStepNanoseconds = runtime_.Config().fixedTimestep.count(),
        .simulationTimeNanoseconds = state.simulationTime.count(),
    };
    snapshot.inputFrame = input_.CurrentFrame();
    snapshot.entity = entity;

    for (const ScenarioInputEvent& inputEvent : report_.inputEvents)
    {
        if (inputEvent.frame > snapshot.inputFrame)
        {
            continue;
        }

        const input::InputControl control = inputEvent.event.control;
        const auto existing = std::find_if(
            snapshot.relevantInput.begin(),
            snapshot.relevantInput.end(),
            [control](const InputStateSnapshot& candidate)
            {
                return candidate.control == control;
            });
        if (existing != snapshot.relevantInput.end())
        {
            continue;
        }

        snapshot.relevantInput.push_back(InputStateSnapshot{
            .control = control,
            .state = input_.State(control),
        });
    }

    return snapshot;
}

void GameplayScenario::AppendFailure(
    const GameplayAssertionFailureCode code,
    const std::string_view selector,
    const std::string_view componentType,
    const std::string_view fieldName,
    const agent::FieldValue& expected,
    std::optional<agent::FieldValue> observed,
    std::string detail,
    std::optional<agent::EntitySnapshot> entity)
{
    const runtime::RuntimeState state = runtime_.State();
    GameplayFailureSnapshot snapshot = MakeFailureSnapshot(entity);

    report_.failures.push_back(GameplayAssertionFailure{
        .code = code,
        .selector = std::string{selector},
        .componentType = std::string{componentType},
        .fieldName = std::string{fieldName},
        .expected = expected,
        .observed = std::move(observed),
        .frame = state.frame,
        .seed = state.seed,
        .detail = std::move(detail),
        .snapshot = std::move(snapshot),
    });
    SyncReportFrame();
}

void GameplayScenario::SyncReportFrame() noexcept
{
    const runtime::RuntimeState state = runtime_.State();
    report_.frame = state.frame;
    report_.seed = state.seed;
}
} // namespace trace2d::testing
