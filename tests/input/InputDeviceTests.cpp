#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace
{
using trace2d::input::ActionMap;
using trace2d::input::Axis1DActionId;
using trace2d::input::ButtonActionId;
using trace2d::input::InputAxis;
using trace2d::input::InputControl;
using trace2d::input::InputDeviceId;
using trace2d::input::InputEvent;
using trace2d::input::InputEventType;
using trace2d::input::InputSystem;
using trace2d::input::PointerState;
using trace2d::input::VirtualInputSource;

TEST(InputDeviceTests, PrimaryGamepadUsesConnectionOrderAndFailsOverWithRetainedState)
{
    InputSystem input{};
    VirtualInputSource source{input};
    constexpr InputDeviceId first = 11U;
    constexpr InputDeviceId second = 22U;

    source.ConnectGamepad(first);
    source.ConnectGamepad(second);

    ASSERT_EQ(input.ConnectedGamepadCount(), 2U);
    ASSERT_TRUE(input.PrimaryGamepad().has_value());
    EXPECT_EQ(*input.PrimaryGamepad(), first);

    source.PressGamepad(second, InputControl::GamepadSouth);
    source.SetGamepadAxis(second, InputAxis::GamepadLeftX, 0.75F);
    EXPECT_FALSE(input.Held(InputControl::GamepadSouth));
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadLeftX), 0.0F);

    source.PressGamepad(first, InputControl::GamepadEast);
    source.SetGamepadAxis(first, InputAxis::GamepadLeftX, -0.25F);
    EXPECT_TRUE(input.Held(InputControl::GamepadEast));
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadLeftX), -0.25F);

    source.DisconnectGamepad(first);

    ASSERT_TRUE(input.PrimaryGamepad().has_value());
    EXPECT_EQ(*input.PrimaryGamepad(), second);
    EXPECT_FALSE(input.IsGamepadConnected(first));
    EXPECT_TRUE(input.IsGamepadConnected(second));
    EXPECT_TRUE(input.Held(InputControl::GamepadSouth));
    EXPECT_FALSE(input.Held(InputControl::GamepadEast));
    EXPECT_TRUE(input.Released(InputControl::GamepadEast));
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadLeftX), 0.75F);
}

TEST(InputDeviceTests, ReconnectIsASeparateRuntimeIdentityAndDoesNotStealPrimary)
{
    InputSystem input{};
    VirtualInputSource source{input};

    source.ConnectGamepad(100U);
    source.ConnectGamepad(200U);
    source.DisconnectGamepad(100U);
    source.ConnectGamepad(300U);

    ASSERT_TRUE(input.PrimaryGamepad().has_value());
    EXPECT_EQ(*input.PrimaryGamepad(), 200U);
    EXPECT_EQ(input.ConnectedGamepadCount(), 2U);
    EXPECT_FALSE(input.IsGamepadConnected(100U));
    EXPECT_TRUE(input.IsGamepadConnected(200U));
    EXPECT_TRUE(input.IsGamepadConnected(300U));
}

TEST(InputDeviceTests, PointerPositionPersistsWhileDeltaAndWheelAreFixedFrameTransient)
{
    InputSystem input{};
    VirtualInputSource source{input};

    source.MovePointer(10.0F, 20.0F, 10.0F, 20.0F);
    source.MovePointer(13.0F, 18.0F, 3.0F, -2.0F);
    source.ScrollPointer(1.0F, 2.0F);
    source.ScrollPointer(-0.25F, 0.5F);

    EXPECT_EQ(
        input.Pointer(),
        (PointerState{
            .x = 13.0F,
            .y = 18.0F,
            .deltaX = 13.0F,
            .deltaY = 18.0F,
            .wheelX = 0.75F,
            .wheelY = 2.5F,
        }));

    input.AdvanceToFrame(1);

    EXPECT_EQ(
        input.Pointer(),
        (PointerState{
            .x = 13.0F,
            .y = 18.0F,
        }));
}

TEST(InputDeviceTests, ScheduledDeviceAndAxisEventsApplyInInsertionOrder)
{
    InputSystem input{};
    constexpr InputDeviceId gamepad = 77U;

    input.Schedule(
        2U,
        InputEvent{.type = InputEventType::DeviceConnected, .device = gamepad});
    input.Schedule(
        2U,
        InputEvent{
            .type = InputEventType::AxisMotion,
            .axis = InputAxis::GamepadRightX,
            .device = gamepad,
            .value = 0.5F,
        });

    input.AdvanceToFrame(2U);

    ASSERT_TRUE(input.PrimaryGamepad().has_value());
    EXPECT_EQ(*input.PrimaryGamepad(), gamepad);
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadRightX), 0.5F);
}

TEST(InputDeviceTests, LowLevelAxisDomainIsClampedByAxisKind)
{
    InputSystem input{};
    VirtualInputSource source{input};
    source.ConnectGamepad(1U);

    source.SetGamepadAxis(1U, InputAxis::GamepadLeftX, 2.0F);
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadLeftX), 1.0F);

    source.SetGamepadAxis(1U, InputAxis::GamepadLeftX, -2.0F);
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadLeftX), -1.0F);

    source.SetGamepadAxis(1U, InputAxis::GamepadLeftTrigger, -0.5F);
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadLeftTrigger), 0.0F);

    source.SetGamepadAxis(1U, InputAxis::GamepadLeftTrigger, 2.0F);
    EXPECT_FLOAT_EQ(input.Axis(InputAxis::GamepadLeftTrigger), 1.0F);
}

TEST(InputDeviceTests, SemanticGamepadButtonsUseTheSameActionStateAsOtherControls)
{
    InputSystem input{};
    VirtualInputSource source{input};
    ActionMap actions{};
    const ButtonActionId attack = actions.AddButtonAction("attack");
    actions.BindButton(attack, InputControl::GamepadSouth);
    actions.Finalize();

    source.ConnectGamepad(9U);
    source.PressGamepad(9U, InputControl::GamepadSouth);
    actions.Resolve(input);

    EXPECT_TRUE(actions.Held(attack));
    EXPECT_TRUE(actions.Pressed(attack));

    input.AdvanceToFrame(1U);
    source.ReleaseGamepad(9U, InputControl::GamepadSouth);
    actions.Resolve(input);

    EXPECT_FALSE(actions.Held(attack));
    EXPECT_TRUE(actions.Released(attack));
}

TEST(InputDeviceTests, AnalogAxisBindingAppliesFrozenDeadzoneAndContinuousRemap)
{
    InputSystem input{};
    VirtualInputSource source{input};
    ActionMap actions{};
    const Axis1DActionId moveX = actions.AddAxis1DAction("move_x");
    actions.BindAxis1DAnalog(moveX, InputAxis::GamepadLeftX, 0.2F);
    actions.Finalize();
    source.ConnectGamepad(4U);

    source.SetGamepadAxis(4U, InputAxis::GamepadLeftX, 0.2F);
    actions.Resolve(input);
    EXPECT_FLOAT_EQ(actions.Axis1D(moveX), 0.0F);

    source.SetGamepadAxis(4U, InputAxis::GamepadLeftX, 0.6F);
    actions.Resolve(input);
    EXPECT_NEAR(actions.Axis1D(moveX), 0.5F, 0.00001F);

    source.SetGamepadAxis(4U, InputAxis::GamepadLeftX, -0.6F);
    actions.Resolve(input);
    EXPECT_NEAR(actions.Axis1D(moveX), -0.5F, 0.00001F);
}

TEST(InputDeviceTests, DigitalAndAnalogAxisBindingsComposeThenClamp)
{
    InputSystem input{};
    VirtualInputSource source{input};
    ActionMap actions{};
    const Axis1DActionId moveX = actions.AddAxis1DAction("move_x", InputControl::KeyA, InputControl::KeyD);
    actions.BindAxis1DAnalog(moveX, InputAxis::GamepadLeftX);
    actions.Finalize();
    source.ConnectGamepad(5U);

    source.Press(InputControl::KeyD);
    source.SetGamepadAxis(5U, InputAxis::GamepadLeftX, -0.5F);
    actions.Resolve(input);
    EXPECT_FLOAT_EQ(actions.Axis1D(moveX), 0.5F);

    source.SetGamepadAxis(5U, InputAxis::GamepadLeftX, 0.75F);
    actions.Resolve(input);
    EXPECT_FLOAT_EQ(actions.Axis1D(moveX), 1.0F);
}

TEST(InputDeviceTests, AnalogBindingSetupRejectsInvalidOrAmbiguousDefinitions)
{
    ActionMap actions{};
    const Axis1DActionId axis = actions.AddAxis1DAction("look_x");

    EXPECT_THROW(actions.BindAxis1DAnalog(axis, InputAxis::Unknown), std::invalid_argument);
    EXPECT_THROW(actions.BindAxis1DAnalog(axis, InputAxis::GamepadRightX, -0.1F), std::invalid_argument);
    EXPECT_THROW(actions.BindAxis1DAnalog(axis, InputAxis::GamepadRightX, 1.0F), std::invalid_argument);
    EXPECT_THROW(actions.BindAxis1DAnalog(axis, InputAxis::GamepadRightX, 0.1F, 0.0F), std::invalid_argument);

    actions.BindAxis1DAnalog(axis, InputAxis::GamepadRightX, 0.1F, -1.0F);
    EXPECT_THROW(actions.BindAxis1DAnalog(axis, InputAxis::GamepadRightX), std::invalid_argument);

    ActionMap unbound{};
    (void)unbound.AddAxis1DAction("unbound");
    EXPECT_THROW(unbound.Finalize(), std::logic_error);
}
} // namespace
