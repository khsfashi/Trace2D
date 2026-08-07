#include <trace2d/input/Input.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace
{
using trace2d::input::InputControl;
using trace2d::input::InputControlState;
using trace2d::input::InputEvent;
using trace2d::input::InputEventType;
using trace2d::input::InputSystem;
using trace2d::input::VirtualInputSource;

TEST(InputTests, ImmediateEventsProducePressHeldAndReleaseTransitions)
{
    InputSystem input{};
    VirtualInputSource source{input};

    source.Press(InputControl::KeyA);
    EXPECT_EQ(input.State(InputControl::KeyA), (InputControlState{.held = true, .pressed = true, .released = false}));

    source.Press(InputControl::KeyA);
    EXPECT_TRUE(input.Held(InputControl::KeyA));
    EXPECT_TRUE(input.Pressed(InputControl::KeyA));
    EXPECT_FALSE(input.Released(InputControl::KeyA));

    input.AdvanceToFrame(1);
    EXPECT_TRUE(input.Held(InputControl::KeyA));
    EXPECT_FALSE(input.Pressed(InputControl::KeyA));
    EXPECT_FALSE(input.Released(InputControl::KeyA));

    source.Release(InputControl::KeyA);
    EXPECT_FALSE(input.Held(InputControl::KeyA));
    EXPECT_FALSE(input.Pressed(InputControl::KeyA));
    EXPECT_TRUE(input.Released(InputControl::KeyA));
}

TEST(InputTests, ScheduledPressAndReleaseApplyOnExactFrames)
{
    InputSystem input{};
    VirtualInputSource source{input};
    source.SchedulePress(2, InputControl::KeyW);
    source.ScheduleRelease(5, InputControl::KeyW);

    input.AdvanceToFrame(1);
    EXPECT_FALSE(input.Held(InputControl::KeyW));

    input.AdvanceToFrame(2);
    EXPECT_TRUE(input.Held(InputControl::KeyW));
    EXPECT_TRUE(input.Pressed(InputControl::KeyW));
    EXPECT_FALSE(input.Released(InputControl::KeyW));

    input.AdvanceToFrame(4);
    EXPECT_TRUE(input.Held(InputControl::KeyW));
    EXPECT_FALSE(input.Pressed(InputControl::KeyW));
    EXPECT_FALSE(input.Released(InputControl::KeyW));

    input.AdvanceToFrame(5);
    EXPECT_FALSE(input.Held(InputControl::KeyW));
    EXPECT_FALSE(input.Pressed(InputControl::KeyW));
    EXPECT_TRUE(input.Released(InputControl::KeyW));
    EXPECT_EQ(input.PendingScheduledEventCount(), 0U);
}

TEST(InputTests, SameFrameEventsPreserveInsertionOrder)
{
    InputSystem input{};
    VirtualInputSource source{input};
    source.SchedulePress(3, InputControl::Space);
    source.ScheduleRelease(3, InputControl::Space);

    input.AdvanceToFrame(3);

    EXPECT_FALSE(input.Held(InputControl::Space));
    EXPECT_TRUE(input.Pressed(InputControl::Space));
    EXPECT_TRUE(input.Released(InputControl::Space));
}

TEST(InputTests, SchedulingOrderDoesNotNeedToMatchFrameOrder)
{
    InputSystem input{};
    VirtualInputSource source{input};
    source.ScheduleRelease(8, InputControl::KeyD);
    source.SchedulePress(2, InputControl::KeyD);
    source.ScheduleRelease(4, InputControl::KeyD);
    source.SchedulePress(6, InputControl::KeyD);

    input.AdvanceToFrame(2);
    EXPECT_TRUE(input.Pressed(InputControl::KeyD));
    EXPECT_TRUE(input.Held(InputControl::KeyD));

    input.AdvanceToFrame(4);
    EXPECT_TRUE(input.Released(InputControl::KeyD));
    EXPECT_FALSE(input.Held(InputControl::KeyD));

    input.AdvanceToFrame(6);
    EXPECT_TRUE(input.Pressed(InputControl::KeyD));
    EXPECT_TRUE(input.Held(InputControl::KeyD));

    input.AdvanceToFrame(8);
    EXPECT_TRUE(input.Released(InputControl::KeyD));
    EXPECT_FALSE(input.Held(InputControl::KeyD));
}

TEST(InputTests, ResetClearsStateFrameAndScheduledEvents)
{
    InputSystem input{};
    VirtualInputSource source{input};
    source.SchedulePress(2, InputControl::MouseLeft);
    source.ScheduleRelease(4, InputControl::MouseLeft);
    input.AdvanceToFrame(2);

    ASSERT_TRUE(input.Held(InputControl::MouseLeft));
    ASSERT_EQ(input.PendingScheduledEventCount(), 1U);

    source.Reset();

    EXPECT_EQ(input.CurrentFrame(), 0U);
    EXPECT_EQ(input.State(InputControl::MouseLeft), InputControlState{});
    EXPECT_EQ(input.PendingScheduledEventCount(), 0U);

    input.AdvanceToFrame(10);
    EXPECT_FALSE(input.Held(InputControl::MouseLeft));
}

TEST(InputTests, RejectsPastSchedulingBackwardFramesAndUnknownControls)
{
    InputSystem input{};
    input.AdvanceToFrame(3);

    EXPECT_THROW(
        input.Schedule(3, InputEvent{.control = InputControl::KeyA, .type = InputEventType::Press}),
        std::invalid_argument);
    EXPECT_THROW(
        input.Schedule(2, InputEvent{.control = InputControl::KeyA, .type = InputEventType::Press}),
        std::invalid_argument);
    EXPECT_THROW(
        input.Schedule(4, InputEvent{.control = InputControl::Unknown, .type = InputEventType::Press}),
        std::invalid_argument);
    EXPECT_THROW(input.AdvanceToFrame(2), std::invalid_argument);
}

TEST(InputTests, IdenticalSchedulesProduceIdenticalDeterministicState)
{
    InputSystem first{};
    InputSystem second{};
    VirtualInputSource firstSource{first};
    VirtualInputSource secondSource{second};

    for (VirtualInputSource* source : {&firstSource, &secondSource})
    {
        source->SchedulePress(1, InputControl::ArrowRight);
        source->SchedulePress(3, InputControl::Space);
        source->ScheduleRelease(4, InputControl::Space);
        source->ScheduleRelease(7, InputControl::ArrowRight);
    }

    for (std::uint64_t frame = 1; frame <= 8; ++frame)
    {
        first.AdvanceToFrame(frame);
        second.AdvanceToFrame(frame);

        EXPECT_EQ(first.State(InputControl::ArrowRight), second.State(InputControl::ArrowRight));
        EXPECT_EQ(first.State(InputControl::Space), second.State(InputControl::Space));
        EXPECT_EQ(first.PendingScheduledEventCount(), second.PendingScheduledEventCount());
    }
}

TEST(InputTests, InputFramesCanAdvanceInExactRuntimeLockstep)
{
    trace2d::runtime::FixedStepRuntime runtime{};
    InputSystem input{};
    VirtualInputSource source{input};
    source.SchedulePress(1, InputControl::KeyW);
    source.ScheduleRelease(4, InputControl::KeyW);

    for (std::uint64_t frame = 1; frame <= 4; ++frame)
    {
        input.AdvanceToFrame(frame);
        runtime.Step();

        EXPECT_EQ(input.CurrentFrame(), runtime.State().frame);
        if (frame == 1)
        {
            EXPECT_TRUE(input.Pressed(InputControl::KeyW));
        }
        if (frame > 1 && frame < 4)
        {
            EXPECT_TRUE(input.Held(InputControl::KeyW));
            EXPECT_FALSE(input.Pressed(InputControl::KeyW));
        }
        if (frame == 4)
        {
            EXPECT_TRUE(input.Released(InputControl::KeyW));
            EXPECT_FALSE(input.Held(InputControl::KeyW));
        }
    }
}
} // namespace
