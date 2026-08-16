#pragma once

#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/World.hpp>
#include <trace2d/ui/Ui.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

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

    // Optional W0 structural lifecycle bound by the host. Structural requests made by Game during
    // OnFixedUpdate are committed by Application immediately after that callback returns.
    [[nodiscard]] scene::WorldLifecycle* Worlds() noexcept;
    [[nodiscard]] const scene::WorldLifecycle* Worlds() const noexcept;

    [[nodiscard]] const input::InputSystem& Input() const noexcept;

    // Action definitions may be authored during OnStart. Application freezes them immediately
    // after OnStart returns, resolves them before every fixed update, and rejects later mutation.
    [[nodiscard]] input::ActionMap& Actions() noexcept;
    [[nodiscard]] const input::ActionMap& Actions() const noexcept;

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
        input::ActionMap& actions,
        ui::UiDocument& ui) noexcept;

    runtime::FixedStepRuntime& runtime_;
    scene::Scene& scene_;
    input::InputSystem& input_;
    input::ActionMap& actions_;
    ui::UiDocument& ui_;
    scene::WorldLifecycle* worlds_{nullptr};
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
    bool worldLifecycleBound{false};
};

using PresentationCallback = void (*)(const GameContext& context, void* userData);

class Application final
{
public:
    explicit Application(Game& game, ApplicationConfig config = {});

    // Use a caller-owned frozen registry when the root application scene needs custom typed
    // components to remain available to normal Scene/Agent inspection. The non-const lvalue
    // reference prevents temporary registries because Scene retains a non-owning pointer to it.
    Application(
        Game& game,
        scene::ComponentRegistry& componentRegistry,
        ApplicationConfig config = {});

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    ~Application() = default;

    void BindWorkContracts(const agent::WorkSpec* workSpec, agent::WorkResult* workResult) noexcept;
    void BindWorldLifecycle(scene::WorldLifecycle* worlds) noexcept;
    void SetPresentationCallback(PresentationCallback callback, void* userData = nullptr) noexcept;

    // Commit a fully built/finalized semantic map at an explicit host safe boundary. This may be
    // used before Start for project-authored input or between StepFrames calls for rebinding. The
    // replacement synchronizes current held/axis state without synthesizing Pressed/Released edges.
    void CommitActions(input::ActionMap actions);

    // Physical/host input is retained until the next fixed frame so Pressed/Released edges remain
    // visible after InputSystem clears transient state for that frame.
    void ApplyInput(const input::InputEvent& event);
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
    [[nodiscard]] input::ActionMap& Actions() noexcept;
    [[nodiscard]] const input::ActionMap& Actions() const noexcept;

    [[nodiscard]] scene::Scene& Scene() noexcept;
    [[nodiscard]] const scene::Scene& Scene() const noexcept;

    [[nodiscard]] ui::UiDocument& Ui() noexcept;
    [[nodiscard]] const ui::UiDocument& Ui() const noexcept;

private:
    static constexpr std::size_t InitialPendingInputCapacity = 16U;

    void RequireRunning() const;
    void ValidateStepCount(std::uint64_t count) const;

    Game& game_;
    runtime::FixedStepRuntime runtime_;
    input::InputSystem input_{};
    input::ActionMap actions_{};
    scene::Scene scene_;
    ui::UiDocument ui_;
    GameContext context_;
    std::vector<input::InputEvent> pendingInputEvents_{};
    ApplicationLifecycle lifecycle_{ApplicationLifecycle::Created};
    PresentationCallback presentationCallback_{nullptr};
    void* presentationUserData_{nullptr};
};
} // namespace trace2d::application
