#include <trace2d/agent/Inspection.hpp>
#include <trace2d/ui/Ui.hpp>
#include <trace2d/ui/UiRaster.hpp>
#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] input::PointerState Pointer(const float x, const float y) noexcept
{
    return input::PointerState{.x = x, .y = y};
}

[[nodiscard]] input::InputControlState PressedButton() noexcept
{
    return input::InputControlState{.held = true, .pressed = true};
}

[[nodiscard]] input::InputControlState ReleasedButton() noexcept
{
    return input::InputControlState{.released = true};
}

[[nodiscard]] std::uint8_t PixelChannel(
    const UiRasterImage& image,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::size_t channel)
{
    const std::size_t pixel =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
        static_cast<std::size_t>(x);
    return image.rgba8[pixel * 4U + channel];
}
} // namespace

TEST(UiClippingTests, AuthoredClipChildrenResolvesAncestorClipForChildBeforeParent)
{
    constexpr std::string_view Source = R"toml(
format_version = 1

[canvas]
width = 120
height = 80

[[elements]]
id = "action"
kind = "button"
parent = "viewport"
bounds = [5, 5, 30, 20]
text = "GO"

[[elements]]
id = "viewport"
kind = "panel"
bounds = [10, 10, 60, 40]
clip_children = true
)toml";

    const UiLoadResult loaded = LoadUiToml(Source, "clip.toml");
    ASSERT_TRUE(loaded.Succeeded());
    const UiElement* const viewport = loaded.document->Find("viewport");
    const UiElement* const action = loaded.document->Find("action");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);

    EXPECT_TRUE(viewport->clipChildren);
    EXPECT_FALSE(viewport->clipActive);
    EXPECT_TRUE(action->clipActive);
    EXPECT_EQ(action->clipBounds, (UiRect{10U, 10U, 60U, 40U}));
    EXPECT_EQ(action->bounds, (UiRect{15U, 15U, 30U, 20U}));
}

TEST(UiClippingTests, PointerHitAndCapturedReleaseUseSameEffectiveClip)
{
    UiDocument document(100U, 60U);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "clipped",
            .kind = UiElementKind::Button,
            .bounds = UiRect{10U, 10U, 40U, 20U},
            .clipActive = true,
            .clipBounds = UiRect{10U, 10U, 15U, 20U},
            .name = "clipped",
        }),
        UiActionResult::Success);

    EXPECT_EQ(document.ApplyPointer(Pointer(30.0F, 15.0F), {}).hoveredIndex, InvalidUiElementIndex);
    EXPECT_EQ(document.HoveredElement(), nullptr);

    const UiPointerRouteResult press =
        document.ApplyPointer(Pointer(20.0F, 15.0F), PressedButton());
    ASSERT_TRUE(press.consumed);
    ASSERT_NE(document.CapturedElement(), nullptr);

    const UiPointerRouteResult release =
        document.ApplyPointer(Pointer(30.0F, 15.0F), ReleasedButton());
    EXPECT_TRUE(release.consumed);
    EXPECT_FALSE(release.activated);
    EXPECT_EQ(document.CapturedElement(), nullptr);
    EXPECT_EQ(document.Find("clipped")->activationCount, 0U);
}

TEST(UiClippingTests, AgentInspectionPublishesAuthoredAndEffectiveClipState)
{
    UiDocument document(100U, 60U);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "child",
            .kind = UiElementKind::Button,
            .bounds = UiRect{10U, 10U, 40U, 20U},
            .clipChildren = true,
            .clipActive = true,
            .clipBounds = UiRect{12U, 12U, 20U, 12U},
            .name = "child",
        }),
        UiActionResult::Success);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiQueryOneResult query =
        facade.QueryUiOne(agent::UiSelector{.id = "child"});
    ASSERT_TRUE(query.Succeeded());
    ASSERT_TRUE(query.match.has_value());
    EXPECT_TRUE(query.match->clipChildren);
    EXPECT_TRUE(query.match->clipActive);
    EXPECT_EQ(query.match->clipBounds.x, 12U);
    EXPECT_EQ(query.match->clipBounds.y, 12U);
    EXPECT_EQ(query.match->clipBounds.width, 20U);
    EXPECT_EQ(query.match->clipBounds.height, 12U);
}

TEST(UiClippingTests, DeterministicRasterDoesNotPaintOutsideEffectiveClip)
{
    UiDocument document(64U, 40U);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "button",
            .kind = UiElementKind::Button,
            .bounds = UiRect{8U, 8U, 32U, 16U},
            .clipActive = true,
            .clipBounds = UiRect{8U, 8U, 12U, 16U},
            .name = "button",
            .text = "OK",
        }),
        UiActionResult::Success);

    UiRasterImage image{};
    ASSERT_TRUE(RasterizeUi(document, image));

    // Canvas background is (16,18,24). The point inside the Button bounds but beyond the effective
    // clip must remain untouched, while a point inside the retained clip must differ from canvas.
    EXPECT_EQ(PixelChannel(image, 30U, 12U, 0U), 16U);
    EXPECT_EQ(PixelChannel(image, 30U, 12U, 1U), 18U);
    EXPECT_EQ(PixelChannel(image, 30U, 12U, 2U), 24U);
    EXPECT_NE(PixelChannel(image, 12U, 12U, 0U), 16U);
}
} // namespace trace2d::ui
