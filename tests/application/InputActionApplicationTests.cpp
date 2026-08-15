#include <trace2d/application/Application.hpp>
#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/input/InputMap.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using trace2d::application::Application;
using trace2d::application::FixedUpdate;
using trace2d::application::Game;
using trace2d::application::GameContext;
using trace2d::input::Axis1DActionId;
using trace2d::input::ButtonActionId;
using trace2d::input::InputAxis;
using trace2d::input::InputControl;
using trace2d::input::InputEvent;
using trace2d::input::InputEventType;
using trace2d::input::PointerState;

struct FrameObservation final
{
    std::uint64_t frame{0};
    float moveX{0.0F};
    bool jumpHeld{false};
    bool jumpPressed{false};
    bool jumpReleased{false};
};

class SemanticInputGame final : public Game
{
public:
    void OnStart(GameContext& context) override
    {
        jump_ = context.Actions().AddButtonAction("jump");
        context.Actions().BindButton(jump_, InputControl::Space);
        moveX_ = context.Actions().AddAxis1DAction("move_x", InputControl::KeyA, InputControl::KeyD);
    }

    void OnFixedUpdate(GameContext& context, const FixedUpdate& update) override
    {
        observations.push_back(FrameObservation{
            .frame = update.frame,
            .moveX = context.Actions().Axis1D(moveX_),
            .jumpHeld = context.Actions().Held(jump_),
            .jumpPressed = context.Actions().Pressed(jump_),
            .jumpReleased = context.Actions().Released(jump_),
        });
    }

    std::vector<FrameObservation> observations{};

private:
    ButtonActionId jump_{};
    Axis1DActionId moveX_{};
};

TEST(InputActionApplicationTests, ApplicationResolvesActionsBeforeEveryFixedUpdate)
{
    SemanticInputGame game{};
    Application application{game};

    application.ScheduleInput(1, InputEvent{.control = InputControl::KeyD, .type = InputEventType::Press});
    application.ScheduleInput(2, InputEvent{.control = InputControl::Space, .type = InputEventType::Press});
    application.ScheduleInput(3, InputEvent{.control = InputControl::Space, .type = InputEventType::Release});
    application.ScheduleInput(3, InputEvent{.control = InputControl::KeyD, .type = InputEventType::Release});

    application.Start();
    ASSERT_TRUE(application.Actions().IsFinalized());
    application.StepFrames(3);

    ASSERT_EQ(game.observations.size(), 3U);

    EXPECT_EQ(game.observations[0].frame, 1U);
    EXPECT_FLOAT_EQ(game.observations[0].moveX, 1.0F);
    EXPECT_FALSE(game.observations[0].jumpHeld);

    EXPECT_EQ(game.observations[1].frame, 2U);
    EXPECT_FLOAT_EQ(game.observations[1].moveX, 1.0F);
    EXPECT_TRUE(game.observations[1].jumpHeld);
    EXPECT_TRUE(game.observations[1].jumpPressed);
    EXPECT_FALSE(game.observations[1].jumpReleased);

    EXPECT_EQ(game.observations[2].frame, 3U);
    EXPECT_FLOAT_EQ(game.observations[2].moveX, 0.0F);
    EXPECT_FALSE(game.observations[2].jumpHeld);
    EXPECT_FALSE(game.observations[2].jumpPressed);
    EXPECT_TRUE(game.observations[2].jumpReleased);
}

struct DeviceFrameObservation final
{
    std::uint64_t frame{0};
    float moveX{0.0F};
    bool attackPressed{false};
    PointerState pointer{};
};

class DeviceInputGame final : public Game
{
public:
    void OnStart(GameContext& context) override
    {
        attack_ = context.Actions().AddButtonAction("attack");
        context.Actions().BindButton(attack_, InputControl::GamepadSouth);
        moveX_ = context.Actions().AddAxis1DAction("move_x");
        context.Actions().BindAxis1DAnalog(moveX_, InputAxis::GamepadLeftX, 0.2F);
    }

    void OnFixedUpdate(GameContext& context, const FixedUpdate& update) override
    {
        observations.push_back(DeviceFrameObservation{
            .frame = update.frame,
            .moveX = context.Actions().Axis1D(moveX_),
            .attackPressed = context.Actions().Pressed(attack_),
            .pointer = context.Input().Pointer(),
        });
    }

    std::vector<DeviceFrameObservation> observations{};

private:
    ButtonActionId attack_{};
    Axis1DActionId moveX_{};
};

TEST(InputActionApplicationTests, HostDeviceEventsConvergeBeforeTheSameFixedUpdate)
{
    DeviceInputGame game{};
    Application application{game};
    application.Start();

    application.ApplyInput(InputEvent{.type = InputEventType::DeviceConnected, .device = 42U});
    application.ApplyInput(InputEvent{
        .control = InputControl::GamepadSouth,
        .type = InputEventType::Press,
        .device = 42U,
    });
    application.ApplyInput(InputEvent{
        .type = InputEventType::AxisMotion,
        .axis = InputAxis::GamepadLeftX,
        .device = 42U,
        .value = 0.6F,
    });
    application.ApplyInput(InputEvent{
        .type = InputEventType::PointerMotion,
        .x = 120.0F,
        .y = 80.0F,
        .deltaX = 4.0F,
        .deltaY = -2.0F,
    });

    application.StepFrames(1U);

    ASSERT_EQ(game.observations.size(), 1U);
    EXPECT_EQ(game.observations[0].frame, 1U);
    EXPECT_NEAR(game.observations[0].moveX, 0.5F, 0.00001F);
    EXPECT_TRUE(game.observations[0].attackPressed);
    EXPECT_EQ(
        game.observations[0].pointer,
        (PointerState{
            .x = 120.0F,
            .y = 80.0F,
            .deltaX = 4.0F,
            .deltaY = -2.0F,
        }));
}

class InvalidSemanticInputGame final : public Game
{
public:
    void OnStart(GameContext& context) override
    {
        (void)context.Actions().AddButtonAction("unbound");
    }

    void OnFixedUpdate(GameContext&, const FixedUpdate&) override {}
};

TEST(InputActionApplicationTests, StartRejectsIncompleteActionDefinitionsBeforeGameplay)
{
    InvalidSemanticInputGame game{};
    Application application{game};

    EXPECT_THROW(application.Start(), std::logic_error);
    EXPECT_EQ(application.Lifecycle(), trace2d::application::ApplicationLifecycle::Stopped);
}

class ConfiguredInputGame final : public Game
{
public:
    void OnStart(GameContext& context) override
    {
        const auto jump = context.Actions().FindButtonAction("jump");
        if (!jump.has_value())
        {
            throw std::logic_error{"Configured input map must contain jump."};
        }
        jump_ = *jump;
    }

    void OnFixedUpdate(GameContext& context, const FixedUpdate& update) override
    {
        observations.push_back(FrameObservation{
            .frame = update.frame,
            .jumpHeld = context.Actions().Held(jump_),
            .jumpPressed = context.Actions().Pressed(jump_),
            .jumpReleased = context.Actions().Released(jump_),
        });
    }

    std::vector<FrameObservation> observations{};

private:
    ButtonActionId jump_{};
};

TEST(InputActionApplicationTests, RebindingToAnAlreadyHeldControlDoesNotSynthesizeAPressEdge)
{
    constexpr std::string_view initialText = R"toml(format_version = 1
[[buttons]]
id = "jump"
controls = ["Enter"]
)toml";

    auto load = trace2d::input::ParseInputMapToml(initialText);
    ASSERT_TRUE(load.Succeeded());
    auto initialBuild = trace2d::input::BuildActionMap(*load.document);
    ASSERT_TRUE(initialBuild.Succeeded());

    ConfiguredInputGame game{};
    Application application{game};
    application.CommitActions(std::move(*initialBuild.actionMap));
    application.Start();

    application.ApplyInput(InputEvent{.control = InputControl::Space, .type = InputEventType::Press});
    application.StepFrames(1U);
    ASSERT_EQ(game.observations.size(), 1U);
    EXPECT_FALSE(game.observations[0].jumpHeld);
    EXPECT_FALSE(game.observations[0].jumpPressed);

    auto document = *load.document;
    const auto rebind = trace2d::input::RebindControl(
        document,
        "jump",
        InputControl::Enter,
        InputControl::Space);
    ASSERT_TRUE(rebind.Succeeded());
    ASSERT_TRUE(rebind.changed);
    auto reboundBuild = trace2d::input::BuildActionMap(document);
    ASSERT_TRUE(reboundBuild.Succeeded());

    application.CommitActions(std::move(*reboundBuild.actionMap));
    application.StepFrames(1U);
    ASSERT_EQ(game.observations.size(), 2U);
    EXPECT_TRUE(game.observations[1].jumpHeld);
    EXPECT_FALSE(game.observations[1].jumpPressed);
    EXPECT_FALSE(game.observations[1].jumpReleased);

    application.ApplyInput(InputEvent{.control = InputControl::Space, .type = InputEventType::Release});
    application.StepFrames(1U);
    ASSERT_EQ(game.observations.size(), 3U);
    EXPECT_FALSE(game.observations[2].jumpHeld);
    EXPECT_FALSE(game.observations[2].jumpPressed);
    EXPECT_TRUE(game.observations[2].jumpReleased);
}

TEST(InputActionApplicationTests, CommitRejectsUnfinalizedMapsAndStoppedApplications)
{
    ConfiguredInputGame game{};
    Application application{game};

    trace2d::input::ActionMap unfinalized{};
    const auto jump = unfinalized.AddButtonAction("jump");
    unfinalized.BindButton(jump, InputControl::Space);
    EXPECT_THROW(application.CommitActions(std::move(unfinalized)), std::invalid_argument);

    trace2d::input::ActionMap finalized{};
    const auto configuredJump = finalized.AddButtonAction("jump");
    finalized.BindButton(configuredJump, InputControl::Space);
    finalized.Finalize();
    application.CommitActions(std::move(finalized));
    application.Stop();

    trace2d::input::ActionMap afterStop{};
    const auto afterStopJump = afterStop.AddButtonAction("jump");
    afterStop.BindButton(afterStopJump, InputControl::Enter);
    afterStop.Finalize();
    EXPECT_THROW(application.CommitActions(std::move(afterStop)), std::logic_error);
}
} // namespace
