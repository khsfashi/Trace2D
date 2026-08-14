#include <trace2d/input/Input.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace trace2d::input
{
namespace
{
[[nodiscard]] bool IsKnownControl(const InputControl control) noexcept
{
    const std::size_t index = static_cast<std::size_t>(control);
    return control != InputControl::Unknown && index < static_cast<std::size_t>(InputControl::Count);
}

[[nodiscard]] bool IsGamepadControl(const InputControl control) noexcept
{
    return control >= InputControl::GamepadSouth && control < InputControl::Count;
}

[[nodiscard]] bool IsKnownAxis(const InputAxis axis) noexcept
{
    const std::size_t index = static_cast<std::size_t>(axis);
    return axis != InputAxis::Unknown && index < static_cast<std::size_t>(InputAxis::Count);
}

[[nodiscard]] bool IsTriggerAxis(const InputAxis axis) noexcept
{
    return axis == InputAxis::GamepadLeftTrigger || axis == InputAxis::GamepadRightTrigger;
}

[[nodiscard]] bool IsFinitePointerEvent(const InputEvent& event) noexcept
{
    return std::isfinite(event.x) && std::isfinite(event.y) && std::isfinite(event.deltaX)
           && std::isfinite(event.deltaY) && std::isfinite(event.wheelX) && std::isfinite(event.wheelY);
}

[[nodiscard]] bool IsValidEvent(const InputEvent& event) noexcept
{
    switch (event.type)
    {
    case InputEventType::Press:
    case InputEventType::Release:
        return IsKnownControl(event.control)
               && (!IsGamepadControl(event.control) || event.device != InvalidInputDeviceId);
    case InputEventType::AxisMotion:
        return IsKnownAxis(event.axis) && event.device != InvalidInputDeviceId && std::isfinite(event.value);
    case InputEventType::PointerMotion:
    case InputEventType::PointerWheel:
        return IsFinitePointerEvent(event);
    case InputEventType::DeviceConnected:
    case InputEventType::DeviceDisconnected:
        return event.device != InvalidInputDeviceId && event.deviceType == InputDeviceType::Gamepad;
    }

    return false;
}

[[nodiscard]] float ClampAxisValue(const InputAxis axis, const float value) noexcept
{
    if (!std::isfinite(value))
    {
        return 0.0F;
    }

    if (IsTriggerAxis(axis))
    {
        return std::clamp(value, 0.0F, 1.0F);
    }

    return std::clamp(value, -1.0F, 1.0F);
}
} // namespace

void InputSystem::Reset() noexcept
{
    states_.fill(InputControlState{});
    axes_.fill(0.0F);
    pointer_ = {};
    gamepads_.clear();
    scheduledEvents_.clear();
    nextScheduledEvent_ = 0;
    currentFrame_ = 0;
}

void InputSystem::ApplyEvent(const InputEvent& event)
{
    if (!IsValidEvent(event))
    {
        return;
    }

    switch (event.type)
    {
    case InputEventType::Press:
    case InputEventType::Release:
        if (!IsGamepadControl(event.control))
        {
            ApplyButtonTransition(event.control, event.type);
            return;
        }

        if (GamepadState* const gamepad = FindGamepad(event.device); gamepad != nullptr)
        {
            const std::size_t controlIndex = ControlIndex(event.control);
            gamepad->held[controlIndex] = event.type == InputEventType::Press;

            if (!gamepads_.empty() && gamepads_.front().device == event.device)
            {
                ApplyButtonTransition(event.control, event.type);
            }
        }
        return;

    case InputEventType::AxisMotion:
        if (GamepadState* const gamepad = FindGamepad(event.device); gamepad != nullptr)
        {
            const std::size_t axisIndex = AxisIndex(event.axis);
            const float value = ClampAxisValue(event.axis, event.value);
            gamepad->axes[axisIndex] = value;

            if (!gamepads_.empty() && gamepads_.front().device == event.device)
            {
                axes_[axisIndex] = value;
            }
        }
        return;

    case InputEventType::PointerMotion:
        pointer_.x = event.x;
        pointer_.y = event.y;
        pointer_.deltaX += event.deltaX;
        pointer_.deltaY += event.deltaY;
        return;

    case InputEventType::PointerWheel:
        pointer_.x = event.x;
        pointer_.y = event.y;
        pointer_.wheelX += event.wheelX;
        pointer_.wheelY += event.wheelY;
        return;

    case InputEventType::DeviceConnected:
        if (FindGamepad(event.device) != nullptr)
        {
            return;
        }

        gamepads_.push_back(GamepadState{.device = event.device});
        if (gamepads_.size() == 1U)
        {
            SyncPrimaryGamepadState();
        }
        return;

    case InputEventType::DeviceDisconnected:
    {
        const auto gamepad = std::find_if(
            gamepads_.begin(),
            gamepads_.end(),
            [&event](const GamepadState& state) { return state.device == event.device; });
        if (gamepad == gamepads_.end())
        {
            return;
        }

        const bool wasPrimary = gamepad == gamepads_.begin();
        if (wasPrimary)
        {
            ClearCanonicalGamepadState();
        }

        gamepads_.erase(gamepad);
        if (wasPrimary)
        {
            SyncPrimaryGamepadState();
        }
        return;
    }
    }
}

void InputSystem::Schedule(const std::uint64_t frame, const InputEvent& event)
{
    if (!IsValidEvent(event))
    {
        throw std::invalid_argument{"Scheduled input event is not a valid engine input event."};
    }

    if (frame <= currentFrame_)
    {
        throw std::invalid_argument{"Scheduled input frame must be later than the current input frame."};
    }

    const auto firstPending = scheduledEvents_.begin() + static_cast<std::ptrdiff_t>(nextScheduledEvent_);
    const auto insertPosition = std::upper_bound(
        firstPending,
        scheduledEvents_.end(),
        frame,
        [](const std::uint64_t value, const ScheduledInputEvent& scheduled) { return value < scheduled.frame; });

    scheduledEvents_.insert(insertPosition, ScheduledInputEvent{.frame = frame, .event = event});
}

void InputSystem::AdvanceToFrame(const std::uint64_t frame)
{
    if (frame < currentFrame_)
    {
        throw std::invalid_argument{"Input frame cannot move backwards."};
    }

    while (currentFrame_ < frame)
    {
        ++currentFrame_;
        ClearTransientState();

        while (nextScheduledEvent_ < scheduledEvents_.size()
               && scheduledEvents_[nextScheduledEvent_].frame == currentFrame_)
        {
            ApplyEvent(scheduledEvents_[nextScheduledEvent_].event);
            ++nextScheduledEvent_;
        }
    }
}

std::uint64_t InputSystem::CurrentFrame() const noexcept
{
    return currentFrame_;
}

InputControlState InputSystem::State(const InputControl control) const noexcept
{
    const std::size_t index = ControlIndex(control);
    if (index >= states_.size() || control == InputControl::Unknown)
    {
        return {};
    }

    return states_[index];
}

bool InputSystem::Held(const InputControl control) const noexcept
{
    return State(control).held;
}

bool InputSystem::Pressed(const InputControl control) const noexcept
{
    return State(control).pressed;
}

bool InputSystem::Released(const InputControl control) const noexcept
{
    return State(control).released;
}

float InputSystem::Axis(const InputAxis axis) const noexcept
{
    const std::size_t index = AxisIndex(axis);
    if (index >= axes_.size() || axis == InputAxis::Unknown)
    {
        return 0.0F;
    }

    return axes_[index];
}

PointerState InputSystem::Pointer() const noexcept
{
    return pointer_;
}

std::optional<InputDeviceId> InputSystem::PrimaryGamepad() const noexcept
{
    if (gamepads_.empty())
    {
        return std::nullopt;
    }

    return gamepads_.front().device;
}

bool InputSystem::IsGamepadConnected(const InputDeviceId device) const noexcept
{
    return FindGamepad(device) != nullptr;
}

std::size_t InputSystem::ConnectedGamepadCount() const noexcept
{
    return gamepads_.size();
}

std::size_t InputSystem::PendingScheduledEventCount() const noexcept
{
    return scheduledEvents_.size() - nextScheduledEvent_;
}

std::size_t InputSystem::ControlIndex(const InputControl control) noexcept
{
    return static_cast<std::size_t>(control);
}

std::size_t InputSystem::AxisIndex(const InputAxis axis) noexcept
{
    return static_cast<std::size_t>(axis);
}

InputSystem::GamepadState* InputSystem::FindGamepad(const InputDeviceId device) noexcept
{
    const auto gamepad = std::find_if(
        gamepads_.begin(),
        gamepads_.end(),
        [device](const GamepadState& state) { return state.device == device; });
    return gamepad != gamepads_.end() ? &*gamepad : nullptr;
}

const InputSystem::GamepadState* InputSystem::FindGamepad(const InputDeviceId device) const noexcept
{
    const auto gamepad = std::find_if(
        gamepads_.begin(),
        gamepads_.end(),
        [device](const GamepadState& state) { return state.device == device; });
    return gamepad != gamepads_.end() ? &*gamepad : nullptr;
}

void InputSystem::ApplyButtonTransition(const InputControl control, const InputEventType type) noexcept
{
    const std::size_t index = ControlIndex(control);
    if (index >= states_.size() || control == InputControl::Unknown)
    {
        return;
    }

    InputControlState& state = states_[index];
    if (type == InputEventType::Press)
    {
        if (!state.held)
        {
            state.held = true;
            state.pressed = true;
        }
        return;
    }

    if (type == InputEventType::Release && state.held)
    {
        state.held = false;
        state.released = true;
    }
}

void InputSystem::ClearCanonicalGamepadState() noexcept
{
    for (std::size_t index = ControlIndex(InputControl::GamepadSouth);
         index < ControlIndex(InputControl::Count);
         ++index)
    {
        InputControlState& state = states_[index];
        if (state.held)
        {
            state.held = false;
            state.released = true;
        }
    }

    for (std::size_t index = AxisIndex(InputAxis::GamepadLeftX);
         index < AxisIndex(InputAxis::Count);
         ++index)
    {
        axes_[index] = 0.0F;
    }
}

void InputSystem::SyncPrimaryGamepadState() noexcept
{
    if (gamepads_.empty())
    {
        return;
    }

    const GamepadState& primary = gamepads_.front();
    for (std::size_t index = ControlIndex(InputControl::GamepadSouth);
         index < ControlIndex(InputControl::Count);
         ++index)
    {
        states_[index].held = primary.held[index];
    }

    for (std::size_t index = AxisIndex(InputAxis::GamepadLeftX);
         index < AxisIndex(InputAxis::Count);
         ++index)
    {
        axes_[index] = primary.axes[index];
    }
}

void InputSystem::ClearTransientState() noexcept
{
    for (InputControlState& state : states_)
    {
        state.pressed = false;
        state.released = false;
    }

    pointer_.deltaX = 0.0F;
    pointer_.deltaY = 0.0F;
    pointer_.wheelX = 0.0F;
    pointer_.wheelY = 0.0F;
}

VirtualInputSource::VirtualInputSource(InputSystem& input) noexcept
    : input_{input}
{
}

void VirtualInputSource::Reset() noexcept
{
    input_.Reset();
}

void VirtualInputSource::Press(const InputControl control) noexcept
{
    input_.ApplyEvent(InputEvent{.control = control, .type = InputEventType::Press});
}

void VirtualInputSource::Release(const InputControl control) noexcept
{
    input_.ApplyEvent(InputEvent{.control = control, .type = InputEventType::Release});
}

void VirtualInputSource::SchedulePress(const std::uint64_t frame, const InputControl control)
{
    input_.Schedule(frame, InputEvent{.control = control, .type = InputEventType::Press});
}

void VirtualInputSource::ScheduleRelease(const std::uint64_t frame, const InputControl control)
{
    input_.Schedule(frame, InputEvent{.control = control, .type = InputEventType::Release});
}

void VirtualInputSource::ConnectGamepad(const InputDeviceId device)
{
    input_.ApplyEvent(InputEvent{.type = InputEventType::DeviceConnected, .device = device});
}

void VirtualInputSource::DisconnectGamepad(const InputDeviceId device)
{
    input_.ApplyEvent(InputEvent{.type = InputEventType::DeviceDisconnected, .device = device});
}

void VirtualInputSource::PressGamepad(const InputDeviceId device, const InputControl control) noexcept
{
    input_.ApplyEvent(InputEvent{.control = control, .type = InputEventType::Press, .device = device});
}

void VirtualInputSource::ReleaseGamepad(const InputDeviceId device, const InputControl control) noexcept
{
    input_.ApplyEvent(InputEvent{.control = control, .type = InputEventType::Release, .device = device});
}

void VirtualInputSource::SetGamepadAxis(
    const InputDeviceId device,
    const InputAxis axis,
    const float value) noexcept
{
    input_.ApplyEvent(InputEvent{
        .type = InputEventType::AxisMotion,
        .axis = axis,
        .device = device,
        .value = value,
    });
}

void VirtualInputSource::MovePointer(
    const float x,
    const float y,
    const float deltaX,
    const float deltaY) noexcept
{
    input_.ApplyEvent(InputEvent{
        .type = InputEventType::PointerMotion,
        .x = x,
        .y = y,
        .deltaX = deltaX,
        .deltaY = deltaY,
    });
}

void VirtualInputSource::ScrollPointer(const float wheelX, const float wheelY) noexcept
{
    const PointerState pointer = input_.Pointer();
    input_.ApplyEvent(InputEvent{
        .type = InputEventType::PointerWheel,
        .x = pointer.x,
        .y = pointer.y,
        .wheelX = wheelX,
        .wheelY = wheelY,
    });
}
} // namespace trace2d::input
