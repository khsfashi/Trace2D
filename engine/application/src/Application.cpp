#include <trace2d/application/Application.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace trace2d::application
{
std::string_view ToString(const ApplicationLifecycle lifecycle) noexcept
{
    switch (lifecycle)
    {
    case ApplicationLifecycle::Created:
        return "created";
    case ApplicationLifecycle::Running:
        return "running";
    case ApplicationLifecycle::Stopped:
        return "stopped";
    }

    return "unknown_application_lifecycle";
}

GameContext::GameContext(
    runtime::FixedStepRuntime& runtime,
    scene::Scene& scene,
    input::InputSystem& input,
    ui::UiDocument& ui) noexcept
    : runtime_{runtime}
    , scene_{scene}
    , input_{input}
    , ui_{ui}
{
}

const runtime::FixedStepRuntime& GameContext::Runtime() const noexcept
{
    return runtime_;
}

scene::Scene& GameContext::Scene() noexcept
{
    return scene_;
}

const scene::Scene& GameContext::Scene() const noexcept
{
    return scene_;
}

scene::WorldLifecycle* GameContext::Worlds() noexcept
{
    return worlds_;
}

const scene::WorldLifecycle* GameContext::Worlds() const noexcept
{
    return worlds_;
}

const input::InputSystem& GameContext::Input() const noexcept
{
    return input_;
}

ui::UiDocument& GameContext::Ui() noexcept
{
    return ui_;
}

const ui::UiDocument& GameContext::Ui() const noexcept
{
    return ui_;
}

const agent::WorkSpec* GameContext::WorkSpec() const noexcept
{
    return workSpec_;
}

agent::WorkResult* GameContext::WorkResult() noexcept
{
    return workResult_;
}

const agent::WorkResult* GameContext::WorkResult() const noexcept
{
    return workResult_;
}

Application::Application(Game& game, ApplicationConfig config)
    : game_{game}
    , runtime_{config.runtime}
    , scene_{std::move(config.scene)}
    , ui_{config.uiWidth, config.uiHeight}
    , context_{runtime_, scene_, input_, ui_}
{
    if (!ui_.HasValidSize())
    {
        throw std::invalid_argument{"Application UI dimensions must be within the supported UiDocument range."};
    }

    pendingInputEvents_.reserve(InitialPendingInputCapacity);
}

void Application::BindWorkContracts(
    const agent::WorkSpec* workSpec,
    agent::WorkResult* workResult) noexcept
{
    context_.workSpec_ = workSpec;
    context_.workResult_ = workResult;
}

void Application::BindWorldLifecycle(scene::WorldLifecycle* const worlds) noexcept
{
    context_.worlds_ = worlds;
}

void Application::SetPresentationCallback(
    const PresentationCallback callback,
    void* const userData) noexcept
{
    presentationCallback_ = callback;
    presentationUserData_ = userData;
}

void Application::ApplyInput(const input::InputEvent& event)
{
    pendingInputEvents_.push_back(event);
}

void Application::ScheduleInput(
    const std::uint64_t frame,
    const input::InputEvent& event)
{
    input_.Schedule(frame, event);
}

void Application::Start()
{
    if (lifecycle_ != ApplicationLifecycle::Created)
    {
        throw std::logic_error{"Application can only start from the created state."};
    }

    lifecycle_ = ApplicationLifecycle::Running;
    try
    {
        game_.OnStart(context_);
    }
    catch (...)
    {
        lifecycle_ = ApplicationLifecycle::Stopped;
        throw;
    }
}

void Application::StepFrames(const std::uint64_t count)
{
    RequireRunning();
    ValidateStepCount(count);

    if (input_.CurrentFrame() != runtime_.State().frame)
    {
        throw std::logic_error{"Application input and runtime frames must remain in lockstep."};
    }

    for (std::uint64_t index = 0; index < count; ++index)
    {
        const std::uint64_t nextFrame = runtime_.State().frame + 1U;
        input_.AdvanceToFrame(nextFrame);
        for (const input::InputEvent& event : pendingInputEvents_)
        {
            input_.ApplyEvent(event);
        }
        pendingInputEvents_.clear();

        runtime_.Step();

        const runtime::RuntimeState state = runtime_.State();
        game_.OnFixedUpdate(
            context_,
            FixedUpdate{
                .frame = state.frame,
                .simulationTime = state.simulationTime,
                .fixedDelta = runtime_.Config().fixedTimestep,
            });

        // W0 freezes the only engine-owned structural safe point immediately after the gameplay
        // callback. Headless and windowed hosts both advance through this same StepFrames path.
        if (context_.worlds_ != nullptr)
        {
            (void)context_.worlds_->CommitStructuralChanges();
        }
    }
}

std::uint64_t Application::AdvanceElapsed(const std::chrono::nanoseconds elapsed)
{
    RequireRunning();
    const std::uint64_t availableFrames = runtime_.ConsumeElapsed(elapsed);
    StepFrames(availableFrames);
    return availableFrames;
}

bool Application::Present()
{
    RequireRunning();
    if (presentationCallback_ == nullptr)
    {
        return false;
    }

    presentationCallback_(context_, presentationUserData_);
    return true;
}

void Application::Stop()
{
    if (lifecycle_ == ApplicationLifecycle::Stopped)
    {
        return;
    }

    if (lifecycle_ == ApplicationLifecycle::Created)
    {
        lifecycle_ = ApplicationLifecycle::Stopped;
        return;
    }

    try
    {
        game_.OnStop(context_);
    }
    catch (...)
    {
        lifecycle_ = ApplicationLifecycle::Stopped;
        throw;
    }

    lifecycle_ = ApplicationLifecycle::Stopped;
}

ApplicationLifecycle Application::Lifecycle() const noexcept
{
    return lifecycle_;
}

ApplicationSnapshot Application::Snapshot() const noexcept
{
    const runtime::RuntimeState state = runtime_.State();
    return ApplicationSnapshot{
        .lifecycle = lifecycle_,
        .frame = state.frame,
        .seed = state.seed,
        .simulationTime = state.simulationTime,
        .sceneSemanticId = scene_.Metadata().semanticId,
        .entityCount = scene_.EntityCount(),
        .uiElementCount = ui_.Elements().size(),
        .workSpecBound = context_.workSpec_ != nullptr,
        .workResultBound = context_.workResult_ != nullptr,
        .presentationBound = presentationCallback_ != nullptr,
        .worldLifecycleBound = context_.worlds_ != nullptr,
    };
}

const runtime::FixedStepRuntime& Application::Runtime() const noexcept
{
    return runtime_;
}

const input::InputSystem& Application::Input() const noexcept
{
    return input_;
}

scene::Scene& Application::Scene() noexcept
{
    return scene_;
}

const scene::Scene& Application::Scene() const noexcept
{
    return scene_;
}

ui::UiDocument& Application::Ui() noexcept
{
    return ui_;
}

const ui::UiDocument& Application::Ui() const noexcept
{
    return ui_;
}

void Application::RequireRunning() const
{
    if (lifecycle_ != ApplicationLifecycle::Running)
    {
        throw std::logic_error{"Application operation requires the running state."};
    }
}

void Application::ValidateStepCount(const std::uint64_t count) const
{
    const runtime::RuntimeState state = runtime_.State();
    if (count > std::numeric_limits<std::uint64_t>::max() - state.frame)
    {
        throw std::overflow_error{"Application frame counter overflow."};
    }

    const std::chrono::nanoseconds::rep timestepNanoseconds = runtime_.Config().fixedTimestep.count();
    const std::chrono::nanoseconds::rep remainingNanoseconds =
        std::numeric_limits<std::chrono::nanoseconds::rep>::max() - state.simulationTime.count();
    const auto maxAdditionalFrames = static_cast<std::uint64_t>(remainingNanoseconds / timestepNanoseconds);
    if (count > maxAdditionalFrames)
    {
        throw std::overflow_error{"Application simulation time overflow."};
    }
}
} // namespace trace2d::application
