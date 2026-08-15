#include <trace2d/agent/Inspection.hpp>
#include <trace2d/ui/Ui.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>
#include <utility>

namespace trace2d::ui
{
namespace
{
std::size_t AddAgentElement(
    UiDocument& document,
    const std::string_view id,
    const UiElementKind kind,
    const UiRect bounds,
    const std::size_t parentIndex = InvalidUiElementIndex,
    const std::string_view parentId = {})
{
    const std::size_t index = document.Elements().size();
    UiElement element{};
    element.id = id;
    element.kind = kind;
    element.parentId = parentId;
    element.parentIndex = parentIndex;
    element.bounds = bounds;
    element.name = id;
    EXPECT_EQ(document.AddElement(std::move(element)), UiActionResult::Success);
    return index;
}
} // namespace

TEST(UiModalAgentTests, InspectionReportsScopeAndSemanticActionReturnsStructuredRejection)
{
    UiDocument document(320U, 180U);
    AddAgentElement(document, "background", UiElementKind::Button, UiRect{8U, 8U, 64U, 24U});
    const std::size_t modalIndex =
        AddAgentElement(document, "modal", UiElementKind::Panel, UiRect{96U, 40U, 160U, 100U});
    AddAgentElement(
        document,
        "inside",
        UiElementKind::Button,
        UiRect{112U, 64U, 64U, 24U},
        modalIndex,
        "modal");
    ASSERT_EQ(document.SetModalScope("modal"), UiActionResult::Success);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiTreeResult tree = facade.InspectUi();
    ASSERT_TRUE(tree.Succeeded());
    ASSERT_TRUE(tree.tree->modalScopeId.has_value());
    EXPECT_EQ(*tree.tree->modalScopeId, "modal");

    const agent::UiActionResponse blocked =
        facade.ActivateUi(agent::UiSelector{.id = "background"});
    ASSERT_FALSE(blocked.Succeeded());
    ASSERT_TRUE(blocked.error.has_value());
    EXPECT_EQ(blocked.error->code, agent::UiAutomationErrorCode::OutsideModalScope);
    EXPECT_EQ(agent::ToString(blocked.error->code), "outside_modal_scope");
    EXPECT_EQ(document.Find("background")->activationCount, 0U);

    const agent::UiActionResponse inside =
        facade.ActivateUi(agent::UiSelector{.id = "inside"});
    ASSERT_TRUE(inside.Succeeded());
    EXPECT_EQ(inside.element->activationCount, 1U);
    EXPECT_EQ(document.Find("inside")->activationCount, 1U);

    document.ClearModalScope();
    const agent::UiTreeResult cleared = facade.InspectUi();
    ASSERT_TRUE(cleared.Succeeded());
    EXPECT_FALSE(cleared.tree->modalScopeId.has_value());
}
} // namespace trace2d::ui
