#include <trace2d/ui/UiLayout.hpp>
#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace trace2d::ui
{
namespace
{
TEST(UiStackLayoutTests, ResolvesHorizontalStackInAuthoredOrderAndSkipsOverlayChildren)
{
    UiLayoutTree tree{320U, 180U};
    tree.ReserveNodes(4U);

    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "first",
            .parentId = "panel",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{
                .width = 40U,
                .height = 20U,
                .margin = UiInsets{2U, 1U, 3U, 2U},
            },
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "overlay",
            .parentId = "panel",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .anchor = UiNormalizedPoint{1024U, 0U},
                .pivot = UiNormalizedPoint{1024U, 0U},
                .offsetX = -5,
                .offsetY = 5,
                .width = 10U,
                .height = 10U,
            },
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "second",
            .parentId = "panel",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{
                .width = 30U,
                .height = 25U,
                .margin = UiInsets{1U, 2U, 4U, 1U},
            },
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "panel",
            .localBounds = UiRect{10U, 10U, 200U, 100U},
            .containerLayout = UiContainerLayoutMode::HorizontalStack,
            .padding = UiInsets{10U, 8U, 12U, 8U},
            .spacing = 5U,
        }),
        UiLayoutResult::Success);

    ASSERT_EQ(tree.Finalize(), UiLayoutResult::Success);

    const UiResolvedLayoutNode* const first = tree.Find("first");
    const UiResolvedLayoutNode* const overlay = tree.Find("overlay");
    const UiResolvedLayoutNode* const second = tree.Find("second");
    const UiResolvedLayoutNode* const panel = tree.Find("panel");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(overlay, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(panel, nullptr);

    EXPECT_EQ(panel->containerLayout, UiContainerLayoutMode::HorizontalStack);
    EXPECT_EQ(panel->padding, (UiInsets{10U, 8U, 12U, 8U}));
    EXPECT_EQ(panel->spacing, 5U);

    EXPECT_EQ(first->resolvedLocalBounds, (UiRect{12U, 9U, 40U, 20U}));
    EXPECT_EQ(first->bounds, (UiRect{22U, 19U, 40U, 20U}));
    EXPECT_EQ(overlay->resolvedLocalBounds, (UiRect{185U, 5U, 10U, 10U}));
    EXPECT_EQ(overlay->bounds, (UiRect{195U, 15U, 10U, 10U}));
    EXPECT_EQ(second->resolvedLocalBounds, (UiRect{61U, 10U, 30U, 25U}));
    EXPECT_EQ(second->bounds, (UiRect{71U, 20U, 30U, 25U}));

    EXPECT_EQ(ToString(UiLayoutPlacementMode::StackFixed), "stack_fixed");
    EXPECT_EQ(ToString(UiContainerLayoutMode::HorizontalStack), "horizontal_stack");
}

TEST(UiStackLayoutTests, ResolvesNestedVerticalAndHorizontalStacks)
{
    UiLayoutTree tree{160U, 140U};
    tree.ReserveNodes(5U);

    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "nested-a",
            .parentId = "row",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{.width = 20U, .height = 20U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "row",
            .parentId = "root",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{.width = 100U, .height = 40U},
            .containerLayout = UiContainerLayoutMode::HorizontalStack,
            .padding = UiInsets{2U, 2U, 2U, 2U},
            .spacing = 3U,
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "nested-b",
            .parentId = "row",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{
                .width = 30U,
                .height = 18U,
                .margin = UiInsets{1U, 1U, 0U, 0U},
            },
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "tail",
            .parentId = "root",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{.width = 60U, .height = 20U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "root",
            .localBounds = UiRect{10U, 10U, 120U, 100U},
            .containerLayout = UiContainerLayoutMode::VerticalStack,
            .padding = UiInsets{5U, 5U, 5U, 5U},
            .spacing = 4U,
        }),
        UiLayoutResult::Success);

    ASSERT_EQ(tree.Finalize(), UiLayoutResult::Success);

    const UiResolvedLayoutNode* const row = tree.Find("row");
    const UiResolvedLayoutNode* const nestedA = tree.Find("nested-a");
    const UiResolvedLayoutNode* const nestedB = tree.Find("nested-b");
    const UiResolvedLayoutNode* const tail = tree.Find("tail");
    ASSERT_NE(row, nullptr);
    ASSERT_NE(nestedA, nullptr);
    ASSERT_NE(nestedB, nullptr);
    ASSERT_NE(tail, nullptr);

    EXPECT_EQ(row->resolvedLocalBounds, (UiRect{5U, 5U, 100U, 40U}));
    EXPECT_EQ(row->bounds, (UiRect{15U, 15U, 100U, 40U}));
    EXPECT_EQ(tail->resolvedLocalBounds, (UiRect{5U, 49U, 60U, 20U}));
    EXPECT_EQ(tail->bounds, (UiRect{15U, 59U, 60U, 20U}));

    EXPECT_EQ(nestedA->resolvedLocalBounds, (UiRect{2U, 2U, 20U, 20U}));
    EXPECT_EQ(nestedA->bounds, (UiRect{17U, 17U, 20U, 20U}));
    EXPECT_EQ(nestedB->resolvedLocalBounds, (UiRect{26U, 3U, 30U, 18U}));
    EXPECT_EQ(nestedB->bounds, (UiRect{41U, 18U, 30U, 18U}));
}

TEST(UiStackLayoutTests, RejectsMissingStackParentInvalidContainerStateAndOverflow)
{
    UiLayoutTree rootItem{64U, 64U};
    EXPECT_EQ(
        rootItem.AddNode(UiLayoutNodeSpec{
            .id = "orphan",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{.width = 8U, .height = 8U},
        }),
        UiLayoutResult::StackParentRequired);

    UiLayoutTree invalidContainer{64U, 64U};
    EXPECT_EQ(
        invalidContainer.AddNode(UiLayoutNodeSpec{
            .id = "panel",
            .localBounds = UiRect{0U, 0U, 32U, 32U},
            .padding = UiInsets{1U, 0U, 0U, 0U},
        }),
        UiLayoutResult::InvalidContainerLayout);

    UiLayoutTree wrongParent{64U, 64U};
    ASSERT_EQ(
        wrongParent.AddNode(UiLayoutNodeSpec{
            .id = "parent",
            .localBounds = UiRect{0U, 0U, 32U, 32U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        wrongParent.AddNode(UiLayoutNodeSpec{
            .id = "child",
            .parentId = "parent",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{.width = 8U, .height = 8U},
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(wrongParent.Finalize(), UiLayoutResult::StackParentRequired);
    EXPECT_FALSE(wrongParent.IsFinalized());

    UiLayoutTree overflow{64U, 64U};
    ASSERT_EQ(
        overflow.AddNode(UiLayoutNodeSpec{
            .id = "parent",
            .localBounds = UiRect{0U, 0U, 30U, 20U},
            .containerLayout = UiContainerLayoutMode::HorizontalStack,
            .padding = UiInsets{5U, 2U, 5U, 2U},
            .spacing = 4U,
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        overflow.AddNode(UiLayoutNodeSpec{
            .id = "a",
            .parentId = "parent",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{.width = 10U, .height = 8U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        overflow.AddNode(UiLayoutNodeSpec{
            .id = "b",
            .parentId = "parent",
            .placementMode = UiLayoutPlacementMode::StackFixed,
            .stackFixed = UiStackFixedPlacement{.width = 10U, .height = 8U},
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(overflow.Finalize(), UiLayoutResult::StackOverflow);
    EXPECT_FALSE(overflow.IsFinalized());
}

TEST(UiStackLayoutTests, LoadsAuthoredStackLayoutAndPublishesResolvedRuntimeBounds)
{
    constexpr std::string_view source = R"toml(
format_version = 1

elements = [
  { id = "first", kind = "button", parent = "menu", placement = "stack_fixed", size = [80, 20], margin = [2, 1, 3, 2], text = "First" },
  { id = "overlay", kind = "label", parent = "menu", bounds = [90, 10, 20, 20], text = "!" },
  { id = "second", kind = "button", parent = "menu", placement = "stack_fixed", size = [80, 18], text = "Second" },
  { id = "menu", kind = "panel", bounds = [20, 20, 120, 80], layout = "vertical_stack", padding = [10, 5, 10, 5], spacing = 4 },
]

[canvas]
width = 240
height = 160
)toml";

    UiLoadResult loaded = LoadUiToml(source, "stack-ui.toml");
    ASSERT_TRUE(loaded.Succeeded());
    ASSERT_TRUE(loaded.document.has_value());

    const UiElement* const first = loaded.document->Find("first");
    const UiElement* const overlay = loaded.document->Find("overlay");
    const UiElement* const second = loaded.document->Find("second");
    const UiElement* const menu = loaded.document->Find("menu");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(overlay, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(menu, nullptr);

    EXPECT_EQ(menu->bounds, (UiRect{20U, 20U, 120U, 80U}));
    EXPECT_EQ(first->localBounds, (UiRect{12U, 6U, 80U, 20U}));
    EXPECT_EQ(first->bounds, (UiRect{32U, 26U, 80U, 20U}));
    EXPECT_EQ(overlay->localBounds, (UiRect{90U, 10U, 20U, 20U}));
    EXPECT_EQ(overlay->bounds, (UiRect{110U, 30U, 20U, 20U}));
    EXPECT_EQ(second->localBounds, (UiRect{10U, 32U, 80U, 18U}));
    EXPECT_EQ(second->bounds, (UiRect{30U, 52U, 80U, 18U}));

    EXPECT_EQ(first->parentId, "menu");
    EXPECT_EQ(first->depth, 1U);
    ASSERT_NE(first->parentIndex, InvalidUiElementIndex);
    EXPECT_EQ(loaded.document->Elements()[first->parentIndex].id, "menu");
}

TEST(UiStackLayoutTests, RejectsInvalidAuthoredStackTransactionsWithoutPublishingDocument)
{
    constexpr std::string_view wrongParent = R"toml(
format_version = 1
elements = [
  { id = "parent", kind = "panel", bounds = [0, 0, 32, 32] },
  { id = "child", kind = "button", parent = "parent", placement = "stack_fixed", size = [8, 8], text = "x" },
]
[canvas]
width = 64
height = 64
)toml";

    UiLoadResult wrong = LoadUiToml(wrongParent, "wrong-parent.toml");
    EXPECT_FALSE(wrong.Succeeded());
    EXPECT_FALSE(wrong.document.has_value());
    ASSERT_FALSE(wrong.diagnostics.empty());
    EXPECT_EQ(wrong.diagnostics.back().path, "layout");
    EXPECT_NE(wrong.diagnostics.back().message.find("stack_parent_required"), std::string::npos);

    constexpr std::string_view overflowSource = R"toml(
format_version = 1
elements = [
  { id = "parent", kind = "panel", bounds = [0, 0, 20, 20], layout = "horizontal_stack", padding = [8, 1, 8, 1] },
  { id = "child", kind = "button", parent = "parent", placement = "stack_fixed", size = [8, 8], text = "x" },
]
[canvas]
width = 64
height = 64
)toml";

    UiLoadResult overflow = LoadUiToml(overflowSource, "overflow.toml");
    EXPECT_FALSE(overflow.Succeeded());
    EXPECT_FALSE(overflow.document.has_value());
    ASSERT_FALSE(overflow.diagnostics.empty());
    EXPECT_NE(overflow.diagnostics.back().message.find("stack_overflow"), std::string::npos);
}

TEST(UiStackLayoutTests, KeepsLegacyFormatOneAbsoluteDocumentsCompatible)
{
    constexpr std::string_view legacy = R"toml(
format_version = 1
elements = [
  { id = "legacy", kind = "label", bounds = [3, 4, 20, 10], text = "ok" },
]
[canvas]
width = 100
height = 60
)toml";

    UiLoadResult loaded = LoadUiToml(legacy, "legacy.toml");
    ASSERT_TRUE(loaded.Succeeded());
    const UiElement* const element = loaded.document->Find("legacy");
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->localBounds, (UiRect{3U, 4U, 20U, 10U}));
    EXPECT_EQ(element->bounds, element->localBounds);
}
} // namespace
} // namespace trace2d::ui
