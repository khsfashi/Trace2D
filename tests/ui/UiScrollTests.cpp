#include <trace2d/agent/Inspection.hpp>
#include <trace2d/ui/Ui.hpp>
#include <trace2d/ui/UiRaster.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace trace2d::ui
{
namespace
{
std::size_t AddScrollElement(
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
    element.parentIndex = parentIndex;
    element.parentId = parentId;
    element.bounds = bounds;
    element.name = id;
    element.text = id;
    EXPECT_EQ(document.AddElement(std::move(element)), UiActionResult::Success);
    return index;
}

[[nodiscard]] std::size_t PixelOffset(
    const UiRasterImage& image,
    const std::uint32_t x,
    const std::uint32_t y)
{
    return (static_cast<std::size_t>(y) * image.width + x) * 4U;
}
} // namespace

TEST(UiScrollTests, ScrollKeepsLogicalBoundsAndOnlyTranslatesOnChangedOffset)
{
    UiDocument document(320U, 240U);
    const std::size_t viewportIndex = AddScrollElement(
        document,
        "viewport",
        UiElementKind::Panel,
        UiRect{20U, 20U, 100U, 80U});
    AddScrollElement(
        document,
        "top",
        UiElementKind::Label,
        UiRect{30U, 20U, 40U, 20U},
        viewportIndex,
        "viewport");
    AddScrollElement(
        document,
        "lower",
        UiElementKind::Button,
        UiRect{30U, 120U, 40U, 20U},
        viewportIndex,
        "viewport");

    ASSERT_EQ(document.ConfigureScrollViewport("viewport", 100U, 180U), UiActionResult::Success);
    const UiElement* viewport = document.Find("viewport");
    const UiElement* top = document.Find("top");
    const UiElement* lower = document.Find("lower");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(top, nullptr);
    ASSERT_NE(lower, nullptr);
    EXPECT_TRUE(viewport->scroll.viewport);
    EXPECT_TRUE(viewport->clipChildren);
    EXPECT_EQ(top->scrollOwnerIndex, viewportIndex);
    EXPECT_EQ(lower->scrollOwnerIndex, viewportIndex);
    EXPECT_TRUE(lower->clipActive);
    EXPECT_EQ(lower->clipBounds, (UiRect{20U, 20U, 100U, 80U}));

    const UiRect logicalTop = top->bounds;
    const UiRect logicalLower = lower->bounds;
    ASSERT_EQ(document.ScrollTo("viewport", 0U, 1000U), UiActionResult::Success);

    viewport = document.Find("viewport");
    top = document.Find("top");
    lower = document.Find("lower");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(top, nullptr);
    ASSERT_NE(lower, nullptr);
    EXPECT_EQ(viewport->scroll.offsetY, 100U);
    EXPECT_EQ(viewport->scroll.revision, 1U);
    EXPECT_EQ(viewport->scroll.translationUpdates, 2U);
    EXPECT_EQ(top->bounds, logicalTop);
    EXPECT_EQ(lower->bounds, logicalLower);
    EXPECT_EQ(top->presentationBounds, (UiPresentationRect{30, -80, 40U, 20U}));
    EXPECT_EQ(lower->presentationBounds, (UiPresentationRect{30, 20, 40U, 20U}));

    ASSERT_EQ(document.ScrollTo("viewport", 0U, 100U), UiActionResult::Success);
    viewport = document.Find("viewport");
    ASSERT_NE(viewport, nullptr);
    EXPECT_EQ(viewport->scroll.revision, 1U);
    EXPECT_EQ(viewport->scroll.translationUpdates, 2U);
}

TEST(UiScrollTests, PointerAndWheelUseTheSameTranslatedScrollAuthority)
{
    UiDocument document(320U, 240U);
    const std::size_t viewportIndex = AddScrollElement(
        document,
        "viewport",
        UiElementKind::Panel,
        UiRect{20U, 20U, 100U, 80U});
    AddScrollElement(
        document,
        "button",
        UiElementKind::Button,
        UiRect{30U, 120U, 50U, 24U},
        viewportIndex,
        "viewport");
    ASSERT_EQ(document.ConfigureScrollViewport("viewport", 100U, 180U), UiActionResult::Success);

    input::PointerState wheel{};
    wheel.x = 25.0F;
    wheel.y = 25.0F;
    wheel.wheelY = -1.0F;
    const UiPointerRouteResult wheelResult = document.ApplyPointer(wheel, {});
    EXPECT_TRUE(wheelResult.consumed);
    ASSERT_NE(document.Find("viewport"), nullptr);
    EXPECT_EQ(document.Find("viewport")->scroll.offsetY, UiScrollWheelStep);

    ASSERT_EQ(document.ScrollTo("viewport", 0U, 80U), UiActionResult::Success);
    const UiElement* button = document.Find("button");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->presentationBounds, (UiPresentationRect{30, 40, 50U, 24U}));

    input::PointerState pointer{};
    pointer.x = 40.0F;
    pointer.y = 50.0F;
    const UiPointerRouteResult pressed = document.ApplyPointer(
        pointer,
        input::InputControlState{.held = true, .pressed = true});
    EXPECT_TRUE(pressed.consumed);
    ASSERT_NE(document.CapturedElement(), nullptr);
    EXPECT_EQ(document.CapturedElement()->id, "button");

    const UiPointerRouteResult released = document.ApplyPointer(
        pointer,
        input::InputControlState{.released = true});
    EXPECT_TRUE(released.activated);
    EXPECT_EQ(document.Find("button")->activationCount, 1U);
}

TEST(UiScrollTests, CpuRasterAndAgentInspectionObserveTranslatedPresentationBounds)
{
    UiDocument document(320U, 240U);
    const std::size_t viewportIndex = AddScrollElement(
        document,
        "viewport",
        UiElementKind::Panel,
        UiRect{20U, 20U, 100U, 80U});
    AddScrollElement(
        document,
        "button",
        UiElementKind::Button,
        UiRect{30U, 120U, 50U, 24U},
        viewportIndex,
        "viewport");
    ASSERT_EQ(document.ConfigureScrollViewport("viewport", 100U, 180U), UiActionResult::Success);

    UiRasterImage before{};
    ASSERT_TRUE(RasterizeUi(document, before));
    ASSERT_EQ(document.ScrollTo("viewport", 0U, 80U), UiActionResult::Success);
    UiRasterImage after{};
    ASSERT_TRUE(RasterizeUi(document, after));

    const std::size_t sample = PixelOffset(after, 40U, 50U);
    ASSERT_LT(sample + 3U, before.rgba8.size());
    ASSERT_LT(sample + 3U, after.rgba8.size());
    EXPECT_NE(before.rgba8[sample], after.rgba8[sample]);
    EXPECT_NE(before.rgba8, after.rgba8);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiTreeResult tree = facade.InspectUi();
    ASSERT_TRUE(tree.Succeeded());
    ASSERT_EQ(tree.tree->elements.size(), 2U);

    const agent::UiElementSnapshot& viewport = tree.tree->elements[0];
    const agent::UiElementSnapshot& button = tree.tree->elements[1];
    EXPECT_TRUE(viewport.scrollViewport);
    EXPECT_EQ(viewport.scrollContentHeight, 180U);
    EXPECT_EQ(viewport.scrollOffsetY, 80U);
    EXPECT_EQ(viewport.scrollRevision, 1U);
    EXPECT_EQ(viewport.scrollTranslationUpdates, 1U);
    ASSERT_TRUE(button.scrollOwnerId.has_value());
    EXPECT_EQ(*button.scrollOwnerId, "viewport");
    EXPECT_EQ(button.bounds, (agent::UiRectSnapshot{30U, 120U, 50U, 24U}));
    EXPECT_EQ(button.presentationBounds, (agent::UiPresentationRectSnapshot{30, 40, 50U, 24U}));
}

TEST(UiScrollTests, InvalidOrNestedConfigurationIsRejectedTransactionally)
{
    UiDocument document(320U, 240U);
    const std::size_t outerIndex = AddScrollElement(
        document,
        "outer",
        UiElementKind::Panel,
        UiRect{20U, 20U, 120U, 100U});
    const std::size_t innerIndex = AddScrollElement(
        document,
        "inner",
        UiElementKind::Panel,
        UiRect{30U, 30U, 80U, 60U},
        outerIndex,
        "outer");
    AddScrollElement(
        document,
        "item",
        UiElementKind::Button,
        UiRect{40U, 40U, 40U, 20U},
        innerIndex,
        "inner");

    EXPECT_EQ(
        document.ConfigureScrollViewport("outer", 100U, 100U),
        UiActionResult::InvalidScrollContent);
    EXPECT_FALSE(document.Find("outer")->scroll.viewport);

    ASSERT_EQ(document.ConfigureScrollViewport("inner", 100U, 100U), UiActionResult::Success);
    EXPECT_EQ(
        document.ConfigureScrollViewport("outer", 180U, 180U),
        UiActionResult::UnsupportedScrollHierarchy);
    EXPECT_FALSE(document.Find("outer")->scroll.viewport);
    EXPECT_TRUE(document.Find("inner")->scroll.viewport);
}
} // namespace trace2d::ui
