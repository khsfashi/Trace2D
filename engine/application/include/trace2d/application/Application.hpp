#pragma once

#include <trace2d/input/Input.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/Scene.hpp>
#include <trace2d/ui/Ui.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace trace2d::agent
{
struct WorkResult;
struct WorkSpec;
}

namespace trace2d::application
{
enum class ApplicationLifecycle : std::uint8_t
{
    Created = 0,
    Running,
    Stopped,
};

[[nodiscard]] std::string_view ToString(ApplicationLifecycle lifecycle) noexcept;

struct FixedUpdate final
{
    std::uint64_t frame{0};
    std::chrono::nanoseconds simulationTime{0};
    std::chrono::nanoseconds fixedDelta{0};
};

class GameContext final
{
public:
    [[nodiscard]] const runtime::FixedStepRuntime& Runtime() const noexcept;

    [[nodiscard]] scene::Scene& Scene() noexcept;
    [[nodiscard]] const scene::Scene& Scene() const noexcept;

    [[nodiscard]] const input::InputSystem& Input() const noexcept;

    [[nodiscard]] ui::UiDocument& Ui() noexcept;
    [[nodiscard]] const ui::UiDocument& Ui() const noexcept;

    [[nodiscard]] const agent::WorkSpec* WorkSpec() const noexcept;
    [[nodiscard]] agent::WorkResult* WorkResult() noexcept;
    [[nodiscard]] const agent::WorkResult* WorkResult() const noexcept;

private:
    GameContext(
        runtime::FixedStepRuntime& runtime,
        scene::Scene& scene,
        input::InputSystem& input,
        ui::UiDocument& ui) noexcept;

    runtime::FixedStepRuntime& runtime_;
    scene::Scene& scene_;
    input::InputSystem& input_;
    ui::UiDocument& ui_;
    const agent::WorkSpec* workSpec_{nullptr};
    agent::WorkResult* workResult_{nullptr};

    friend class Application;
};

class Game
{
public:
    virtual ~Game() = default;

    virtual void OnStart(GameContext&) {}
    virtual void OnFixedUpdate(GameContext& context, const FixedUpdate& update) = 0;
    virtual void OnStop(GameContext&) {}
};

struct ApplicationConfig final
{
    runtime::RuntimeConfig runtime{};
    scene::SceneMetadata scene{};
    std::uint32_t uiWidth{1280};
    std::uint32_t uiHeight{720};
};

struct ApplicationSnapshot final
{
    ApplicationLifecycle lifecycle{ApplicationLifecycle::Created};
    std::uint64_t frame{0};
    std::uint64_t seed{0};
    std::chrono::nanoseconds simulationTime{0};
    std::string_view sceneSemanticId{};
    std::size_t entityCount{0};
    std::size_t uiElementCount{0};
    bool workSpecBound{false};
    bool workResultBound{false};
    bool presentationBound{false};
};

using PresentationCallback = void (*)(const GameContext& context, void* userData);

class Application final
{
public:
    explicit Application(Game& game, ApplicationConfig config = {});

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    ~Application() = default;

    void BindWorkContracts(const agent::WorkSpec* workSpec, agent::WorkResult* workResult) noexcept;
    void SetPresentationCallback(PresentationCallback callback, void* userData = nullptr) noexcept;

    void ApplyInput(const input::InputEvent& event) noexcept;
    void ScheduleInput(std::uint64_t frame, const input::InputEvent& event);

    void Start();
    void StepFrames(std::uint64_t count = 1);
    [[nodiscard]] std::uint64_t AdvanceElapsed(std::chrono::nanoseconds elapsed);
    [[nodiscard]] bool Present();
    void Stop();

    [[nodiscard]] ApplicationLifecycle Lifecycle() const noexcept;
    [[nodiscard]] ApplicationSnapshot Snapshot() const noexcept;

    [[nodiscard]] const runtime::FixedStepRuntime& Runtime() const noexcept;
    [[nodiscard]] const input::InputSystem& Input() const noexcept;

    [[nodiscard]] scene::Scene& Scene() noexcept;
    [[nodiscard]] const scene::Scene& Scene() const noexcept;

    [[nodiscard]] ui::UiDocument& Ui() noexcept;
    [[nodiscard]] const ui::UiDocument& Ui() const noexcept;

private:
    void RequireRunning() const;
    void ValidateStepCount(std::uint64_t count) const;

    Game& game_;
    runtime::FixedStepRuntime runtime_;
    input::InputSystem input_{};
    scene::Scene scene_;
    ui::UiDocument ui_;
    GameContext context_;
    ApplicationLifecycle lifecycle_{ApplicationLifecycle::Created};
    PresentationCallback presentationCallback_{nullptr};
    void* presentationUserData_{nullptr};
};
} // namespace trace2d::application
