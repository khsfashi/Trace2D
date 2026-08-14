#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
using trace2d::input::ActionMap;
using trace2d::input::Axis1DActionId;
using trace2d::input::ButtonActionId;
using trace2d::input::ButtonActionState;
using trace2d::input::InputControl;
using trace2d::input::InputEvent;
using trace2d::input::InputEventType;
using trace2d::input::InputSystem;
using trace2d::input::VirtualInputSource;

TEST(ActionMapTests, MultipleBindingsAggregateIntoOneDeterministicButton)
{
    InputSystem input{};
    VirtualInputSource source{input};
    ActionMap actions{};

    const ButtonActionId jump = actions.AddButtonAction("jump");
    actions.BindButton(jump, InputControl::Space);
    actions.BindButton(jump, InputControl::Enter);
    actions.Finalize();

    source.Press(InputControl::Space);
    actions.Resolve(input);
    EXPECT_EQ(actions.ButtonState(jump), (ButtonActionState{.held = true, .pressed = true, .released = false}));

    input.AdvanceToFrame(1);
    source.Press(InputControl::Enter);
    source.Release(InputControl::Space);
    actions.Resolve(input);
    EXPECT_EQ(actions.ButtonState(jump), (ButtonActionState{.held = true, .pressed = false, .released = false}));

    input.AdvanceToFrame(2);
    source.Release(InputControl::Enter);
    actions.Resolve(input);
    EXPECT_EQ(actions.ButtonState(jump), (ButtonActionState{.held = false, .pressed = false, .released = true}));
}

TEST(ActionMapTests, SameFrameTapPreservesBothSemanticEdges)
{
    InputSystem input{};
    ActionMap actions{};
    const ButtonActionId attack = actions.AddButtonAction("attack");
    actions.BindButton(attack, InputControl::MouseLeft);
    actions.Finalize();

    input.ApplyEvent(InputEvent{.control = InputControl::MouseLeft, .type = InputEventType::Press});
    input.ApplyEvent(InputEvent{.control = InputControl::MouseLeft, .type = InputEventType::Release});
    actions.Resolve(input);

    EXPECT_EQ(actions.ButtonState(attack), (ButtonActionState{.held = false, .pressed = true, .released = true}));
}

TEST(ActionMapTests, DigitalAxisComposesAndCancelsDeterministically)
{
    InputSystem input{};
    VirtualInputSource source{input};
    ActionMap actions{};
    const Axis1DActionId moveX = actions.AddAxis1DAction("move_x", InputControl::KeyA, InputControl::KeyD);
    actions.Finalize();

    actions.Resolve(input);
    EXPECT_FLOAT_EQ(actions.Axis1D(moveX), 0.0F);

    source.Press(InputControl::KeyA);
    actions.Resolve(input);
    EXPECT_FLOAT_EQ(actions.Axis1D(moveX), -1.0F);

    source.Press(InputControl::KeyD);
    actions.Resolve(input);
    EXPECT_FLOAT_EQ(actions.Axis1D(moveX), 0.0F);

    source.Release(InputControl::KeyA);
    actions.Resolve(input);
    EXPECT_FLOAT_EQ(actions.Axis1D(moveX), 1.0F);
}

TEST(ActionMapTests, PhysicalStyleAndVirtualEventsProduceIdenticalSemanticState)
{
    InputSystem physical{};
    InputSystem virtualInput{};
    VirtualInputSource virtualSource{virtualInput};

    ActionMap physicalActions{};
    ActionMap virtualActions{};
    const ButtonActionId physicalAttack = physicalActions.AddButtonAction("attack");
    const ButtonActionId virtualAttack = virtualActions.AddButtonAction("attack");
    physicalActions.BindButton(physicalAttack, InputControl::MouseLeft);
    virtualActions.BindButton(virtualAttack, InputControl::MouseLeft);
    physicalActions.Finalize();
    virtualActions.Finalize();

    physical.ApplyEvent(InputEvent{.control = InputControl::MouseLeft, .type = InputEventType::Press});
    virtualSource.Press(InputControl::MouseLeft);
    physicalActions.Resolve(physical);
    virtualActions.Resolve(virtualInput);

    EXPECT_EQ(physicalActions.ButtonState(physicalAttack), virtualActions.ButtonState(virtualAttack));

    physical.AdvanceToFrame(1);
    virtualInput.AdvanceToFrame(1);
    physical.ApplyEvent(InputEvent{.control = InputControl::MouseLeft, .type = InputEventType::Release});
    virtualSource.Release(InputControl::MouseLeft);
    physicalActions.Resolve(physical);
    virtualActions.Resolve(virtualInput);

    EXPECT_EQ(physicalActions.ButtonState(physicalAttack), virtualActions.ButtonState(virtualAttack));
}

TEST(ActionMapTests, SetupRejectsAmbiguousOrInvalidDefinitions)
{
    ActionMap actions{};
    const ButtonActionId jump = actions.AddButtonAction("jump");

    EXPECT_THROW((void)actions.AddButtonAction("jump"), std::invalid_argument);
    EXPECT_THROW(
        (void)actions.AddAxis1DAction("jump", InputControl::KeyA, InputControl::KeyD),
        std::invalid_argument);
    EXPECT_THROW(actions.BindButton(jump, InputControl::Unknown), std::invalid_argument);

    actions.BindButton(jump, InputControl::Space);
    EXPECT_THROW(actions.BindButton(jump, InputControl::Space), std::invalid_argument);
    EXPECT_THROW(
        actions.BindButton(ButtonActionId{.value = 99U}, InputControl::Enter),
        std::out_of_range);
    EXPECT_THROW(
        (void)actions.AddAxis1DAction("move", InputControl::KeyA, InputControl::KeyA),
        std::invalid_argument);
}

TEST(ActionMapTests, FinalizationFreezesBindingsAndRequiresButtonBindings)
{
    ActionMap invalid{};
    (void)invalid.AddButtonAction("unbound");
    EXPECT_THROW(invalid.Finalize(), std::logic_error);

    ActionMap actions{};
    const ButtonActionId jump = actions.AddButtonAction("jump");
    actions.BindButton(jump, InputControl::Space);
    actions.Finalize();
    EXPECT_TRUE(actions.IsFinalized());

    EXPECT_THROW((void)actions.AddButtonAction("attack"), std::logic_error);
    EXPECT_THROW(actions.BindButton(jump, InputControl::Enter), std::logic_error);
    EXPECT_THROW(
        (void)actions.AddAxis1DAction("move", InputControl::KeyA, InputControl::KeyD),
        std::logic_error);
}

TEST(ActionMapTests, SemanticLookupReturnsStableResolvedIds)
{
    ActionMap actions{};
    const ButtonActionId jump = actions.AddButtonAction("jump");
    actions.BindButton(jump, InputControl::Space);
    const Axis1DActionId moveX = actions.AddAxis1DAction("move_x", InputControl::KeyA, InputControl::KeyD);

    ASSERT_TRUE(actions.FindButtonAction("jump").has_value());
    EXPECT_EQ(*actions.FindButtonAction("jump"), jump);
    ASSERT_TRUE(actions.FindAxis1DAction("move_x").has_value());
    EXPECT_EQ(*actions.FindAxis1DAction("move_x"), moveX);
    EXPECT_FALSE(actions.FindButtonAction("missing").has_value());
    EXPECT_FALSE(actions.FindAxis1DAction("missing").has_value());
}

TEST(ActionMapTests, ResetAndReplayProduceIdenticalSemanticState)
{
    InputSystem input{};
    VirtualInputSource source{input};
    ActionMap actions{};
    const ButtonActionId jump = actions.AddButtonAction("jump");
    actions.BindButton(jump, InputControl::Space);
    actions.Finalize();

    source.Press(InputControl::Space);
    actions.Resolve(input);
    const ButtonActionState first = actions.ButtonState(jump);

    source.Reset();
    actions.ResetState();
    source.Press(InputControl::Space);
    actions.Resolve(input);

    EXPECT_EQ(actions.ButtonState(jump), first);
}

TEST(ActionMapTests, InvalidResolvedIdsAreRejected)
{
    InputSystem input{};
    ActionMap actions{};
    const ButtonActionId jump = actions.AddButtonAction("jump");
    actions.BindButton(jump, InputControl::Space);
    (void)actions.AddAxis1DAction("move_x", InputControl::KeyA, InputControl::KeyD);
    actions.Finalize();
    actions.Resolve(input);

    EXPECT_THROW((void)actions.ButtonState(ButtonActionId{.value = 99U}), std::out_of_range);
    EXPECT_THROW((void)actions.Axis1D(Axis1DActionId{.value = 99U}), std::out_of_range);
}
} // namespace
