#include <trace2d/application/Application.hpp>
#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace
{
using trace2d::application::Application;
using trace2d::application::FixedUpdate;
using trace2d::application::Game;
using trace2d::application::GameContext;
using trace2d::input::Axis1DActionId;
using trace2d::input::ButtonActionId;
using trace2d::input::InputControl;
using trace2d::input::InputEvent;
using trace2d::input::InputEventType;

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
} // namespace
