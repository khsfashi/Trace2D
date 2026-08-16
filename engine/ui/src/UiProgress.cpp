#include <trace2d/ui/Ui.hpp>

#include <limits>

namespace trace2d::ui
{
std::string_view ToString(const UiProgressResult result) noexcept
{
    switch (result)
    {
    case UiProgressResult::Success:
        return "success";
    case UiProgressResult::NotFound:
        return "not_found";
    case UiProgressResult::InvalidTarget:
        return "invalid_target";
    case UiProgressResult::NotProgress:
        return "not_progress";
    case UiProgressResult::InvalidRange:
        return "invalid_range";
    }

    return "unknown";
}

UiProgressResult UiDocument::ConfigureProgress(
    const std::string_view id,
    const std::uint32_t value,
    const std::uint32_t maximum) noexcept
{
    UiElement* const element = FindMutable(id);
    if (element == nullptr)
    {
        return UiProgressResult::NotFound;
    }
    if (element->kind != UiElementKind::Panel || element->scroll.viewport ||
        element->image.Active() || element->progress.active_)
    {
        return UiProgressResult::InvalidTarget;
    }
    if (maximum == 0U || value > maximum)
    {
        return UiProgressResult::InvalidRange;
    }

    element->progress.active_ = true;
    element->progress.value_ = value;
    element->progress.maximum_ = maximum;
    element->progress.revision_ = 1U;
    return UiProgressResult::Success;
}

UiProgressResult UiDocument::SetProgress(
    const std::string_view id,
    const std::uint32_t value,
    const std::uint32_t maximum) noexcept
{
    UiElement* const element = FindMutable(id);
    if (element == nullptr)
    {
        return UiProgressResult::NotFound;
    }
    if (!element->progress.active_)
    {
        return UiProgressResult::NotProgress;
    }
    if (maximum == 0U || value > maximum)
    {
        return UiProgressResult::InvalidRange;
    }
    if (element->progress.value_ == value && element->progress.maximum_ == maximum)
    {
        return UiProgressResult::Success;
    }

    element->progress.value_ = value;
    element->progress.maximum_ = maximum;
    element->progress.revision_ = element->progress.revision_ == std::numeric_limits<std::uint64_t>::max()
        ? 1U
        : element->progress.revision_ + 1U;
    return UiProgressResult::Success;
}
} // namespace trace2d::ui
