#include <trace2d/input/Input.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace trace2d::input
{
namespace
{
[[nodiscard]] bool IsSchedulableControl(const InputControl control) noexcept
{
    const std::size_t index = static_cast<std::size_t>(control);
    return control != InputControl::Unknown && index < static_cast<std::size_t>(InputControl::Count);
}
} // namespace

void InputSystem::Reset() noexcept
{
    states_.fill(InputControlState{});
    scheduledEvents_.clear();
    nextScheduledEvent_ = 0;
    currentFrame_ = 0;
}

void InputSystem::ApplyEvent(const InputEvent& event) noexcept
{
    const std::size_t index = ControlIndex(event.control);
    if (index >= states_.size() || event.control == InputControl::Unknown)
    {
        return;
    }

    InputControlState& state = states_[index];
    switch (event.type)
    {
    case InputEventType::Press:
        if (!state.held)
        {
            state.held = true;
            state.pressed = true;
        }
        break;
    case InputEventType::Release:
        if (state.held)
        {
            state.held = false;
            state.released = true;
        }
        break;
    }
}

void InputSystem::Schedule(const std::uint64_t frame, const InputEvent& event)
{
    if (!IsSchedulableControl(event.control))
    {
        throw std::invalid_argument{"Scheduled input control must be a known engine input control."};
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

std::size_t InputSystem::PendingScheduledEventCount() const noexcept
{
    return scheduledEvents_.size() - nextScheduledEvent_;
}

std::size_t InputSystem::ControlIndex(const InputControl control) noexcept
{
    return static_cast<std::size_t>(control);
}

void InputSystem::ClearTransientState() noexcept
{
    for (InputControlState& state : states_)
    {
        state.pressed = false;
        state.released = false;
    }
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
} // namespace trace2d::input
