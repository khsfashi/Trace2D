#pragma once

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/Scene.hpp>
#include <trace2d/scene/SceneText.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::testing
{
struct ScenarioInputEvent final
{
    std::uint64_t frame{0};
    input::InputEvent event{};
    bool scheduled{true};

    [[nodiscard]] bool operator==(const ScenarioInputEvent&) const noexcept = default;
};

struct InputStateSnapshot final
{
    input::InputControl control{input::InputControl::Unknown};
    input::InputControlState state{};

    [[nodiscard]] bool operator==(const InputStateSnapshot&) const noexcept = default;
};

struct GameplayFailureSnapshot final
{
    agent::RuntimeSnapshot runtime{};
    std::uint64_t inputFrame{0};
    std::vector<InputStateSnapshot> relevantInput{};
    std::optional<agent::EntitySnapshot> entity{};

    [[nodiscard]] bool operator==(const GameplayFailureSnapshot&) const noexcept = default;
};

enum class GameplayAssertionFailureCode
{
    SceneUnavailable,
    InvalidSelector,
    NoMatch,
    AmbiguousMatch,
    ComponentMissing,
    FieldMissing,
    ValueMismatch,
};

[[nodiscard]] std::string_view ToString(GameplayAssertionFailureCode code) noexcept;

struct GameplayAssertionFailure final
{
    GameplayAssertionFailureCode code{GameplayAssertionFailureCode::ValueMismatch};
    std::string selector{};
    std::string componentType{};
    std::string fieldName{};
    agent::FieldValue expected{};
    std::optional<agent::FieldValue> observed{};
    std::uint64_t frame{0};
    std::uint64_t seed{0};
    std::string detail{};
    GameplayFailureSnapshot snapshot{};

    [[nodiscard]] bool operator==(const GameplayAssertionFailure&) const noexcept = default;
};

struct GameplayScenarioReport final
{
    std::uint64_t frame{0};
    std::uint64_t seed{0};
    std::vector<ScenarioInputEvent> inputEvents{};
    std::vector<GameplayAssertionFailure> failures{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return failures.empty();
    }

    [[nodiscard]] bool operator==(const GameplayScenarioReport&) const noexcept = default;
};

struct GameplaySceneLoadResult final
{
    bool loaded{false};
    std::vector<scene::SceneTextDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return loaded && diagnostics.empty();
    }
};

struct GameplayFrameContext final
{
    std::uint64_t frame;
    scene::Scene& scene;
    const input::InputSystem& input;
    const runtime::FixedStepRuntime& runtime;
};

using GameplayFrameUpdate = std::function<void(GameplayFrameContext&)>;

class GameplayScenario final
{
public:
    explicit GameplayScenario(const runtime::RuntimeConfig& runtimeConfig = {});

    [[nodiscard]] GameplaySceneLoadResult LoadSceneToml(
        std::string_view text,
        std::string_view sourceName = {});
    void LoadScene(scene::Scene scene);
    void Reset(std::uint64_t seed);

    void InjectInput(const input::InputEvent& event) noexcept;
    void ScheduleInput(std::uint64_t frame, const input::InputEvent& event);
    void SchedulePress(std::uint64_t frame, input::InputControl control);
    void ScheduleRelease(std::uint64_t frame, input::InputControl control);

    void RunFrames(std::uint64_t count, const GameplayFrameUpdate& update = {});

    [[nodiscard]] bool AssertFloatFieldEquals(
        std::string_view selector,
        std::string_view componentType,
        std::string_view fieldName,
        float expected);

    [[nodiscard]] const GameplayScenarioReport& Report() const noexcept;
    [[nodiscard]] const runtime::FixedStepRuntime& Runtime() const noexcept;
    [[nodiscard]] const input::InputSystem& Input() const noexcept;
    [[nodiscard]] scene::Scene* ActiveScene() noexcept;
    [[nodiscard]] const scene::Scene* ActiveScene() const noexcept;

private:
    [[nodiscard]] GameplayFailureSnapshot MakeFailureSnapshot(
        const std::optional<agent::EntitySnapshot>& entity) const;
    void AppendFailure(
        GameplayAssertionFailureCode code,
        std::string_view selector,
        std::string_view componentType,
        std::string_view fieldName,
        const agent::FieldValue& expected,
        std::optional<agent::FieldValue> observed,
        std::string detail,
        std::optional<agent::EntitySnapshot> entity = std::nullopt);
    void SyncReportFrame() noexcept;

    std::optional<scene::Scene> baselineScene_{};
    std::optional<scene::Scene> activeScene_{};
    runtime::FixedStepRuntime runtime_{};
    input::InputSystem input_{};
    GameplayScenarioReport report_{};
};
} // namespace trace2d::testing
