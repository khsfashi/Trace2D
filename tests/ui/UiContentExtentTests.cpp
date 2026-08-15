#include <trace2d/ui/UiLayout.hpp>

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace trace2d::ui
{
namespace
{
UiLayoutNodeSpec AbsoluteNode(
    std::string id,
    const UiRect bounds,
    std::string parentId = {})
{
    UiLayoutNodeSpec node{};
    node.id = std::move(id);
    node.parentId = std::move(parentId);
    node.localBounds = bounds;
    return node;
}
} // namespace

TEST(UiContentExtentTests, LegacyParentStillRejectsChildOutsideVisibleBounds)
{
    UiLayoutTree layout(320U, 240U);
    ASSERT_EQ(
        layout.AddNode(AbsoluteNode("viewport", UiRect{20U, 20U, 100U, 80U})),
        UiLayoutResult::Success);
    ASSERT_EQ(
        layout.AddNode(AbsoluteNode("lower", UiRect{10U, 100U, 40U, 20U}, "viewport")),
        UiLayoutResult::Success);

    EXPECT_EQ(layout.Finalize(), UiLayoutResult::ChildOutsideParent);
    EXPECT_FALSE(layout.IsFinalized());
}

TEST(UiContentExtentTests, ExplicitContentExtentAllowsOffViewportLogicalChild)
{
    UiLayoutTree layout(320U, 240U);
    UiLayoutNodeSpec viewport = AbsoluteNode("viewport", UiRect{20U, 20U, 100U, 80U});
    viewport.childContentWidth = 100U;
    viewport.childContentHeight = 180U;
    ASSERT_EQ(layout.AddNode(std::move(viewport)), UiLayoutResult::Success);
    ASSERT_EQ(
        layout.AddNode(AbsoluteNode("lower", UiRect{10U, 100U, 40U, 20U}, "viewport")),
        UiLayoutResult::Success);

    ASSERT_EQ(layout.Finalize(), UiLayoutResult::Success);
    const UiResolvedLayoutNode* resolvedViewport = layout.Find("viewport");
    const UiResolvedLayoutNode* lower = layout.Find("lower");
    ASSERT_NE(resolvedViewport, nullptr);
    ASSERT_NE(lower, nullptr);
    EXPECT_EQ(resolvedViewport->bounds, (UiRect{20U, 20U, 100U, 80U}));
    EXPECT_EQ(resolvedViewport->childContentWidth, 100U);
    EXPECT_EQ(resolvedViewport->childContentHeight, 180U);
    EXPECT_EQ(lower->resolvedLocalBounds, (UiRect{10U, 100U, 40U, 20U}));
    EXPECT_EQ(lower->bounds, (UiRect{30U, 120U, 40U, 20U}));
}

TEST(UiContentExtentTests, AnchoredChildUsesExplicitContentReferenceDimensions)
{
    UiLayoutTree layout(320U, 240U);
    UiLayoutNodeSpec viewport = AbsoluteNode("viewport", UiRect{20U, 20U, 100U, 80U});
    viewport.childContentWidth = 200U;
    viewport.childContentHeight = 160U;
    ASSERT_EQ(layout.AddNode(std::move(viewport)), UiLayoutResult::Success);

    UiLayoutNodeSpec child{};
    child.id = "anchored";
    child.parentId = "viewport";
    child.placementMode = UiLayoutPlacementMode::AnchoredFixed;
    child.anchored = UiAnchoredPlacement{
        .anchor = UiNormalizedPoint{UiNormalizedUnit, UiNormalizedUnit},
        .pivot = UiNormalizedPoint{UiNormalizedUnit, UiNormalizedUnit},
        .width = 20U,
        .height = 10U,
    };
    ASSERT_EQ(layout.AddNode(std::move(child)), UiLayoutResult::Success);

    ASSERT_EQ(layout.Finalize(), UiLayoutResult::Success);
    const UiResolvedLayoutNode* anchored = layout.Find("anchored");
    ASSERT_NE(anchored, nullptr);
    EXPECT_EQ(anchored->resolvedLocalBounds, (UiRect{180U, 150U, 20U, 10U}));
    EXPECT_EQ(anchored->bounds, (UiRect{200U, 170U, 20U, 10U}));
}

TEST(UiContentExtentTests, StackFlowUsesExplicitContentDimensions)
{
    UiLayoutTree layout(320U, 240U);
    UiLayoutNodeSpec viewport = AbsoluteNode("viewport", UiRect{20U, 20U, 100U, 50U});
    viewport.containerLayout = UiContainerLayoutMode::VerticalStack;
    viewport.padding = UiInsets{5U, 5U, 5U, 5U};
    viewport.spacing = 10U;
    viewport.childContentWidth = 100U;
    viewport.childContentHeight = 140U;
    ASSERT_EQ(layout.AddNode(std::move(viewport)), UiLayoutResult::Success);

    UiLayoutNodeSpec first{};
    first.id = "first";
    first.parentId = "viewport";
    first.placementMode = UiLayoutPlacementMode::StackFixed;
    first.stackFixed = UiStackFixedPlacement{.width = 40U, .height = 40U};
    ASSERT_EQ(layout.AddNode(std::move(first)), UiLayoutResult::Success);

    UiLayoutNodeSpec second{};
    second.id = "second";
    second.parentId = "viewport";
    second.placementMode = UiLayoutPlacementMode::StackFixed;
    second.stackFixed = UiStackFixedPlacement{.width = 40U, .height = 40U};
    ASSERT_EQ(layout.AddNode(std::move(second)), UiLayoutResult::Success);

    ASSERT_EQ(layout.Finalize(), UiLayoutResult::Success);
    const UiResolvedLayoutNode* firstResolved = layout.Find("first");
    const UiResolvedLayoutNode* secondResolved = layout.Find("second");
    ASSERT_NE(firstResolved, nullptr);
    ASSERT_NE(secondResolved, nullptr);
    EXPECT_EQ(firstResolved->resolvedLocalBounds, (UiRect{5U, 5U, 40U, 40U}));
    EXPECT_EQ(secondResolved->resolvedLocalBounds, (UiRect{5U, 55U, 40U, 40U}));
    EXPECT_EQ(secondResolved->bounds, (UiRect{25U, 75U, 40U, 40U}));
}

TEST(UiContentExtentTests, InvalidContentExtentsFailBeforePublication)
{
    {
        UiLayoutTree layout(320U, 240U);
        UiLayoutNodeSpec partial = AbsoluteNode("partial", UiRect{0U, 0U, 100U, 80U});
        partial.childContentWidth = 120U;
        EXPECT_EQ(layout.AddNode(std::move(partial)), UiLayoutResult::InvalidContentExtent);
    }

    {
        UiLayoutTree layout(320U, 240U);
        UiLayoutNodeSpec overLimit = AbsoluteNode("over_limit", UiRect{0U, 0U, 100U, 80U});
        overLimit.childContentWidth = MaxUiCanvasDimension + 1U;
        overLimit.childContentHeight = 100U;
        EXPECT_EQ(layout.AddNode(std::move(overLimit)), UiLayoutResult::InvalidContentExtent);
    }

    {
        UiLayoutTree layout(320U, 240U);
        UiLayoutNodeSpec undersized = AbsoluteNode("undersized", UiRect{20U, 20U, 100U, 80U});
        undersized.childContentWidth = 90U;
        undersized.childContentHeight = 80U;
        ASSERT_EQ(layout.AddNode(std::move(undersized)), UiLayoutResult::Success);
        EXPECT_EQ(layout.Finalize(), UiLayoutResult::InvalidContentExtent);
        EXPECT_FALSE(layout.IsFinalized());
    }

    {
        UiLayoutTree layout(320U, 240U);
        UiLayoutNodeSpec escaping = AbsoluteNode("escaping", UiRect{250U, 20U, 50U, 80U});
        escaping.childContentWidth = 100U;
        escaping.childContentHeight = 80U;
        ASSERT_EQ(layout.AddNode(std::move(escaping)), UiLayoutResult::Success);
        EXPECT_EQ(layout.Finalize(), UiLayoutResult::InvalidContentExtent);
        EXPECT_FALSE(layout.IsFinalized());
    }
}
} // namespace trace2d::ui
