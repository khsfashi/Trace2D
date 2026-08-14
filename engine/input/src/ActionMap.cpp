#include <trace2d/input/ActionMap.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace trace2d::input
{
namespace
{
[[nodiscard]] bool IsKnownControl(const InputControl control) noexcept
{
    const std::size_t index = static_cast<std::size_t>(control);
    return control != InputControl::Unknown && index < static_cast<std::size_t>(InputControl::Count);
}

template <typename Id>
[[nodiscard]] std::size_t IdIndex(const Id id) noexcept
{
    return static_cast<std::size_t>(id.value);
}
} // namespace

ButtonActionId ActionMap::AddButtonAction(std::string semanticId)
{
    RequireMutable();
    if (semanticId.empty())
    {
        throw std::invalid_argument{"Button action semantic ID must not be empty."};
    }
    RequireUniqueSemanticId(semanticId);

    if (buttonActions_.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::length_error{"Button action ID space exhausted."};
    }

    const ButtonActionId id{.value = static_cast<std::uint32_t>(buttonActions_.size())};
    buttonActions_.push_back(ButtonActionRecord{.semanticId = std::move(semanticId)});
    return id;
}

void ActionMap::BindButton(const ButtonActionId action, const InputControl control)
{
    RequireMutable();
    if (!IsKnownControl(control))
    {
        throw std::invalid_argument{"Button action binding requires a known engine input control."};
    }

    ButtonActionRecord& record = ButtonRecord(action);
    if (std::find(record.controls.begin(), record.controls.end(), control) != record.controls.end())
    {
        throw std::invalid_argument{"Button action binding is already present."};
    }

    record.controls.push_back(control);
}

Axis1DActionId ActionMap::AddAxis1DAction(
    std::string semanticId,
    const InputControl negative,
    const InputControl positive)
{
    RequireMutable();
    if (semanticId.empty())
    {
        throw std::invalid_argument{"Axis action semantic ID must not be empty."};
    }
    if (!IsKnownControl(negative) || !IsKnownControl(positive))
    {
        throw std::invalid_argument{"Axis action bindings require known engine input controls."};
    }
    if (negative == positive)
    {
        throw std::invalid_argument{"Axis action negative and positive controls must differ."};
    }
    RequireUniqueSemanticId(semanticId);

    if (axis1DActions_.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::length_error{"Axis action ID space exhausted."};
    }

    const Axis1DActionId id{.value = static_cast<std::uint32_t>(axis1DActions_.size())};
    axis1DActions_.push_back(Axis1DActionRecord{
        .semanticId = std::move(semanticId),
        .negative = negative,
        .positive = positive,
    });
    return id;
}

void ActionMap::Finalize()
{
    if (finalized_)
    {
        return;
    }

    for (const ButtonActionRecord& action : buttonActions_)
    {
        if (action.controls.empty())
        {
            throw std::logic_error{"Every button action must have at least one binding before finalization."};
        }
    }

    finalized_ = true;
}

bool ActionMap::IsFinalized() const noexcept
{
    return finalized_;
}

void ActionMap::Resolve(const InputSystem& input)
{
    if (!finalized_)
    {
        throw std::logic_error{"ActionMap must be finalized before semantic state is resolved."};
    }

    for (ButtonActionRecord& action : buttonActions_)
    {
        const bool previouslyHeld = action.state.held;
        bool held = false;
        bool anyPressed = false;
        bool anyReleased = false;

        for (const InputControl control : action.controls)
        {
            const InputControlState controlState = input.State(control);
            held = held || controlState.held;
            anyPressed = anyPressed || controlState.pressed;
            anyReleased = anyReleased || controlState.released;
        }

        const bool sameFrameTap = !previouslyHeld && !held && anyPressed && anyReleased;
        action.state = ButtonActionState{
            .held = held,
            .pressed = (!previouslyHeld && held) || sameFrameTap,
            .released = (previouslyHeld && !held) || sameFrameTap,
        };
    }

    for (Axis1DActionRecord& action : axis1DActions_)
    {
        float value = 0.0F;
        if (input.Held(action.negative))
        {
            value -= 1.0F;
        }
        if (input.Held(action.positive))
        {
            value += 1.0F;
        }
        action.value = std::clamp(value, -1.0F, 1.0F);
    }
}

void ActionMap::ResetState() noexcept
{
    for (ButtonActionRecord& action : buttonActions_)
    {
        action.state = {};
    }
    for (Axis1DActionRecord& action : axis1DActions_)
    {
        action.value = 0.0F;
    }
}

std::optional<ButtonActionId> ActionMap::FindButtonAction(const std::string_view semanticId) const noexcept
{
    for (std::size_t index = 0; index < buttonActions_.size(); ++index)
    {
        if (buttonActions_[index].semanticId == semanticId)
        {
            return ButtonActionId{.value = static_cast<std::uint32_t>(index)};
        }
    }
    return std::nullopt;
}

std::optional<Axis1DActionId> ActionMap::FindAxis1DAction(const std::string_view semanticId) const noexcept
{
    for (std::size_t index = 0; index < axis1DActions_.size(); ++index)
    {
        if (axis1DActions_[index].semanticId == semanticId)
        {
            return Axis1DActionId{.value = static_cast<std::uint32_t>(index)};
        }
    }
    return std::nullopt;
}

ButtonActionState ActionMap::ButtonState(const ButtonActionId action) const
{
    return ButtonRecord(action).state;
}

bool ActionMap::Held(const ButtonActionId action) const
{
    return ButtonState(action).held;
}

bool ActionMap::Pressed(const ButtonActionId action) const
{
    return ButtonState(action).pressed;
}

bool ActionMap::Released(const ButtonActionId action) const
{
    return ButtonState(action).released;
}

float ActionMap::Axis1D(const Axis1DActionId action) const
{
    return AxisRecord(action).value;
}

std::size_t ActionMap::ButtonActionCount() const noexcept
{
    return buttonActions_.size();
}

std::size_t ActionMap::Axis1DActionCount() const noexcept
{
    return axis1DActions_.size();
}

void ActionMap::RequireMutable() const
{
    if (finalized_)
    {
        throw std::logic_error{"ActionMap bindings are frozen after finalization."};
    }
}

void ActionMap::RequireUniqueSemanticId(const std::string_view semanticId) const
{
    if (FindButtonAction(semanticId).has_value() || FindAxis1DAction(semanticId).has_value())
    {
        throw std::invalid_argument{"Input action semantic IDs must be unique across button and axis actions."};
    }
}

ActionMap::ButtonActionRecord& ActionMap::ButtonRecord(const ButtonActionId action)
{
    const std::size_t index = IdIndex(action);
    if (index >= buttonActions_.size())
    {
        throw std::out_of_range{"Button action ID is invalid for this ActionMap."};
    }
    return buttonActions_[index];
}

const ActionMap::ButtonActionRecord& ActionMap::ButtonRecord(const ButtonActionId action) const
{
    const std::size_t index = IdIndex(action);
    if (index >= buttonActions_.size())
    {
        throw std::out_of_range{"Button action ID is invalid for this ActionMap."};
    }
    return buttonActions_[index];
}

const ActionMap::Axis1DActionRecord& ActionMap::AxisRecord(const Axis1DActionId action) const
{
    const std::size_t index = IdIndex(action);
    if (index >= axis1DActions_.size())
    {
        throw std::out_of_range{"Axis action ID is invalid for this ActionMap."};
    }
    return axis1DActions_[index];
}
} // namespace trace2d::input
