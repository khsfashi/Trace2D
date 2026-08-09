#include <trace2d/agent/Inspection.hpp>

#include <trace2d/ui/Ui.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace trace2d::agent
{
namespace
{
[[nodiscard]] UiRole RoleForKind(const ui::UiElementKind kind) noexcept
{
    switch (kind)
    {
    case ui::UiElementKind::Panel:
        return UiRole::Panel;
    case ui::UiElementKind::Label:
        return UiRole::Label;
    case ui::UiElementKind::Button:
        return UiRole::Button;
    case ui::UiElementKind::TextInput:
        return UiRole::TextBox;
    }

    return UiRole::Panel;
}

[[nodiscard]] bool IsValidSelector(const UiSelector& selector) noexcept
{
    const bool hasId = selector.id.has_value();
    const bool hasRole = selector.role.has_value();
    const bool hasName = selector.name.has_value();
    if (!hasId && !hasRole && !hasName)
    {
        return false;
    }

    if (hasId && selector.id->empty())
    {
        return false;
    }

    if (hasName && selector.name->empty())
    {
        return false;
    }

    return true;
}

[[nodiscard]] bool MatchesSelector(
    const ui::UiElement& element,
    const UiSelector& selector) noexcept
{
    if (selector.id.has_value() && element.id != *selector.id)
    {
        return false;
    }

    if (selector.role.has_value() && RoleForKind(element.kind) != *selector.role)
    {
        return false;
    }

    if (selector.name.has_value() && element.name != *selector.name)
    {
        return false;
    }

    return true;
}

[[nodiscard]] UiElementSnapshot MakeSnapshot(
    const ui::UiDocument& document,
    const ui::UiElement& element)
{
    return UiElementSnapshot{
        .id = element.id,
        .role = RoleForKind(element.kind),
        .name = element.name,
        .bounds = UiRectSnapshot{
            .x = element.bounds.x,
            .y = element.bounds.y,
            .width = element.bounds.width,
            .height = element.bounds.height,
        },
        .visible = element.visible,
        .enabled = element.enabled,
        .focused = document.IsFocused(element.id),
        .text = element.text,
        .activationCount = element.activationCount,
    };
}

[[nodiscard]] UiAutomationError MakeError(
    const UiAutomationErrorCode code,
    std::string message)
{
    return UiAutomationError{
        .code = code,
        .message = std::move(message),
    };
}

[[nodiscard]] UiAutomationError ErrorForAction(const ui::UiActionResult result)
{
    switch (result)
    {
    case ui::UiActionResult::NotFound:
        return MakeError(UiAutomationErrorCode::NoMatch, "UI target no longer exists.");
    case ui::UiActionResult::NotVisible:
        return MakeError(UiAutomationErrorCode::NotVisible, "UI target is not visible.");
    case ui::UiActionResult::Disabled:
        return MakeError(UiAutomationErrorCode::Disabled, "UI target is disabled.");
    case ui::UiActionResult::NotFocusable:
        return MakeError(UiAutomationErrorCode::NotFocusable, "UI target is not focusable.");
    case ui::UiActionResult::NotActivatable:
        return MakeError(UiAutomationErrorCode::NotActivatable, "UI target is not activatable.");
    case ui::UiActionResult::NotTextInput:
        return MakeError(UiAutomationErrorCode::NotTextInput, "UI target is not a text box.");
    case ui::UiActionResult::NotFocused:
        return MakeError(UiAutomationErrorCode::NotFocused, "UI text box must be focused before text input.");
    case ui::UiActionResult::Success:
        break;
    case ui::UiActionResult::InvalidDocumentSize:
    case ui::UiActionResult::InvalidId:
    case ui::UiActionResult::DuplicateId:
    case ui::UiActionResult::InvalidBounds:
        break;
    }

    return MakeError(
        UiAutomationErrorCode::ActionRejected,
        "UI action was rejected by engine-owned UI state: " + std::string{ui::ToString(result)} + ".");
}

[[nodiscard]] UiActionResponse MakeActionQueryFailure(const UiQueryOneResult& query)
{
    UiActionResponse response{};
    response.selector = query.selector;
    response.error = query.error;
    return response;
}

[[nodiscard]] UiActionResponse MakeActionSuccess(
    const UiSelector& selector,
    const ui::UiDocument& document,
    const ui::UiElement& element)
{
    UiActionResponse response{};
    response.selector = selector;
    response.element = MakeSnapshot(document, element);
    return response;
}

[[nodiscard]] std::string BoolText(const bool value)
{
    return value ? "true" : "false";
}

[[nodiscard]] UiAutomationError StateMismatch(
    const std::string_view field,
    std::string expected,
    std::string observed)
{
    return MakeError(
        UiAutomationErrorCode::StateMismatch,
        "UI assertion failed for " + std::string{field} + ": expected " +
            std::move(expected) + ", observed " + std::move(observed) + ".");
}
} // namespace

std::string_view ToString(const UiRole role) noexcept
{
    switch (role)
    {
    case UiRole::Panel:
        return "panel";
    case UiRole::Label:
        return "label";
    case UiRole::Button:
        return "button";
    case UiRole::TextBox:
        return "textbox";
    }

    return "unknown";
}

std::string_view ToString(const UiAutomationErrorCode code) noexcept
{
    switch (code)
    {
    case UiAutomationErrorCode::UiUnavailable:
        return "ui_unavailable";
    case UiAutomationErrorCode::InvalidSelector:
        return "invalid_selector";
    case UiAutomationErrorCode::NoMatch:
        return "no_match";
    case UiAutomationErrorCode::AmbiguousMatch:
        return "ambiguous_match";
    case UiAutomationErrorCode::NotVisible:
        return "not_visible";
    case UiAutomationErrorCode::Disabled:
        return "disabled";
    case UiAutomationErrorCode::NotFocusable:
        return "not_focusable";
    case UiAutomationErrorCode::NotActivatable:
        return "not_activatable";
    case UiAutomationErrorCode::NotTextInput:
        return "not_text_input";
    case UiAutomationErrorCode::NotFocused:
        return "not_focused";
    case UiAutomationErrorCode::StateMismatch:
        return "state_mismatch";
    case UiAutomationErrorCode::ActionRejected:
        return "action_rejected";
    }

    return "unknown_ui_automation_error";
}

UiTreeResult AgentFacade::InspectUi() const
{
    UiTreeResult result{};
    if (ui_ == nullptr)
    {
        result.error = MakeError(
            UiAutomationErrorCode::UiUnavailable,
            "No active UI document is bound to the agent facade.");
        return result;
    }

    UiTreeSnapshot tree{};
    tree.width = ui_->Width();
    tree.height = ui_->Height();
    tree.elements.reserve(ui_->Elements().size());
    for (const ui::UiElement& element : ui_->Elements())
    {
        tree.elements.push_back(MakeSnapshot(*ui_, element));
    }

    result.tree = std::move(tree);
    return result;
}

UiQueryResult AgentFacade::QueryUi(const UiSelector& selector) const
{
    UiQueryResult result{};
    if (ui_ == nullptr)
    {
        result.error = MakeError(
            UiAutomationErrorCode::UiUnavailable,
            "No active UI document is bound to the agent facade.");
        return result;
    }

    if (!IsValidSelector(selector))
    {
        result.error = MakeError(
            UiAutomationErrorCode::InvalidSelector,
            "UI selector must contain a non-empty id, role, or non-empty name.");
        return result;
    }

    result.selector = selector;
    for (const ui::UiElement& element : ui_->Elements())
    {
        if (MatchesSelector(element, selector))
        {
            result.matches.push_back(MakeSnapshot(*ui_, element));
        }
    }

    return result;
}

UiQueryOneResult AgentFacade::QueryOneUi(const UiSelector& selector) const
{
    UiQueryOneResult result{};
    if (ui_ == nullptr)
    {
        result.error = MakeError(
            UiAutomationErrorCode::UiUnavailable,
            "No active UI document is bound to the agent facade.");
        return result;
    }

    if (!IsValidSelector(selector))
    {
        result.error = MakeError(
            UiAutomationErrorCode::InvalidSelector,
            "UI selector must contain a non-empty id, role, or non-empty name.");
        return result;
    }

    result.selector = selector;
    std::size_t matchCount = 0U;
    for (const ui::UiElement& element : ui_->Elements())
    {
        if (!MatchesSelector(element, selector))
        {
            continue;
        }

        ++matchCount;
        if (matchCount == 1U)
        {
            result.match = MakeSnapshot(*ui_, element);
        }
    }

    if (matchCount == 0U)
    {
        result.match.reset();
        result.error = MakeError(
            UiAutomationErrorCode::NoMatch,
            "UI selector matched no elements.");
        return result;
    }

    if (matchCount != 1U)
    {
        result.match.reset();
        result.error = MakeError(
            UiAutomationErrorCode::AmbiguousMatch,
            "UI selector matched " + std::to_string(matchCount) +
                " elements; single-result query requires exactly one match.");
    }

    return result;
}

UiActionResponse AgentFacade::FocusUi(const UiSelector& selector)
{
    const UiQueryOneResult query = QueryOneUi(selector);
    if (!query.Succeeded())
    {
        return MakeActionQueryFailure(query);
    }

    const ui::UiActionResult action = ui_->Focus(query.match->id);
    if (action != ui::UiActionResult::Success)
    {
        UiActionResponse response{};
        response.selector = selector;
        response.error = ErrorForAction(action);
        return response;
    }

    const ui::UiElement* element = ui_->Find(query.match->id);
    if (element == nullptr)
    {
        UiActionResponse response{};
        response.selector = selector;
        response.error = MakeError(UiAutomationErrorCode::NoMatch, "UI target no longer exists.");
        return response;
    }

    return MakeActionSuccess(selector, *ui_, *element);
}

UiActionResponse AgentFacade::ActivateUi(const UiSelector& selector)
{
    const UiQueryOneResult query = QueryOneUi(selector);
    if (!query.Succeeded())
    {
        return MakeActionQueryFailure(query);
    }

    const ui::UiActionResult action = ui_->Activate(query.match->id);
    if (action != ui::UiActionResult::Success)
    {
        UiActionResponse response{};
        response.selector = selector;
        response.error = ErrorForAction(action);
        return response;
    }

    const ui::UiElement* element = ui_->Find(query.match->id);
    if (element == nullptr)
    {
        UiActionResponse response{};
        response.selector = selector;
        response.error = MakeError(UiAutomationErrorCode::NoMatch, "UI target no longer exists.");
        return response;
    }

    return MakeActionSuccess(selector, *ui_, *element);
}

UiActionResponse AgentFacade::InputUiText(
    const UiSelector& selector,
    const std::string_view text)
{
    const UiQueryOneResult query = QueryOneUi(selector);
    if (!query.Succeeded())
    {
        return MakeActionQueryFailure(query);
    }

    const ui::UiActionResult action = ui_->InputText(query.match->id, text);
    if (action != ui::UiActionResult::Success)
    {
        UiActionResponse response{};
        response.selector = selector;
        response.error = ErrorForAction(action);
        return response;
    }

    const ui::UiElement* element = ui_->Find(query.match->id);
    if (element == nullptr)
    {
        UiActionResponse response{};
        response.selector = selector;
        response.error = MakeError(UiAutomationErrorCode::NoMatch, "UI target no longer exists.");
        return response;
    }

    return MakeActionSuccess(selector, *ui_, *element);
}

UiAssertionResult AgentFacade::AssertUi(
    const UiSelector& selector,
    const UiExpectedState& expected) const
{
    UiAssertionResult result{};
    const UiQueryOneResult query = QueryOneUi(selector);
    result.selector = query.selector;
    if (!query.Succeeded())
    {
        result.error = query.error;
        return result;
    }

    result.observed = query.match;
    const UiElementSnapshot& observed = *result.observed;

    if (expected.visible.has_value() && observed.visible != *expected.visible)
    {
        result.error = StateMismatch(
            "visible",
            BoolText(*expected.visible),
            BoolText(observed.visible));
        return result;
    }

    if (expected.enabled.has_value() && observed.enabled != *expected.enabled)
    {
        result.error = StateMismatch(
            "enabled",
            BoolText(*expected.enabled),
            BoolText(observed.enabled));
        return result;
    }

    if (expected.focused.has_value() && observed.focused != *expected.focused)
    {
        result.error = StateMismatch(
            "focused",
            BoolText(*expected.focused),
            BoolText(observed.focused));
        return result;
    }

    if (expected.text.has_value() && observed.text != *expected.text)
    {
        result.error = StateMismatch(
            "text",
            "'" + *expected.text + "'",
            "'" + observed.text + "'");
        return result;
    }

    if (expected.activationCount.has_value() &&
        observed.activationCount != *expected.activationCount)
    {
        result.error = StateMismatch(
            "activation_count",
            std::to_string(*expected.activationCount),
            std::to_string(observed.activationCount));
    }

    return result;
}
} // namespace trace2d::agent
