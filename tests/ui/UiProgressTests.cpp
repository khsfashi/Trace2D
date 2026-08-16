#include <trace2d/agent/Inspection.hpp>
#include <trace2d/ui/Ui.hpp>
#include <trace2d/ui/UiRaster.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace trace2d::ui
{
namespace
{
void AddElement(
    UiDocument& document,
    const std::string_view id,
    const UiElementKind kind,
    const UiRect bounds,
    const std::string_view name = {})
{
    UiElement element{};
    element.id = id;
    element.kind = kind;
    element.bounds = bounds;
    element.name = name;
    ASSERT_EQ(document.AddElement(std::move(element)), UiActionResult::Success);
}

[[nodiscard]] std::array<std::uint8_t, 4U> PixelAt(
    const UiRasterImage& image,
    const std::uint32_t x,
    const std::uint32_t y)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4U;
    return {
        image.rgba8[offset],
        image.rgba8[offset + 1U],
        image.rgba8[offset + 2U],
        image.rgba8[offset + 3U],
    };
}
} // namespace

TEST(UiProgressTests, ConfigureAndSetPreserveRangeAndNoOpRevisionContracts)
{
    UiDocument document(160U, 80U);
    AddElement(document, "health", UiElementKind::Panel, UiRect{8U, 8U, 100U, 12U});
    AddElement(document, "label", UiElementKind::Label, UiRect{8U, 28U, 100U, 12U});

    ASSERT_EQ(document.ConfigureProgress("health", 25U, 100U), UiProgressResult::Success);
    const UiElement* health = document.Find("health");
    ASSERT_NE(health, nullptr);
    EXPECT_TRUE(health->progress.Active());
    EXPECT_EQ(health->progress.Value(), 25U);
    EXPECT_EQ(health->progress.Maximum(), 100U);
    EXPECT_EQ(health->progress.Revision(), 1U);

    EXPECT_EQ(document.SetProgress("health", 25U, 100U), UiProgressResult::Success);
    EXPECT_EQ(document.Find("health")->progress.Revision(), 1U);

    EXPECT_EQ(document.SetProgress("health", 50U, 100U), UiProgressResult::Success);
    health = document.Find("health");
    ASSERT_NE(health, nullptr);
    EXPECT_EQ(health->progress.Value(), 50U);
    EXPECT_EQ(health->progress.Maximum(), 100U);
    EXPECT_EQ(health->progress.Revision(), 2U);

    EXPECT_EQ(document.SetProgress("health", 101U, 100U), UiProgressResult::InvalidRange);
    EXPECT_EQ(document.SetProgress("health", 0U, 0U), UiProgressResult::InvalidRange);
    EXPECT_EQ(document.SetProgress("missing", 1U, 1U), UiProgressResult::NotFound);
    EXPECT_EQ(document.SetProgress("label", 1U, 1U), UiProgressResult::NotProgress);
    EXPECT_EQ(document.ConfigureProgress("label", 1U, 1U), UiProgressResult::InvalidTarget);
    EXPECT_EQ(document.ConfigureProgress("health", 1U, 1U), UiProgressResult::InvalidTarget);

    health = document.Find("health");
    ASSERT_NE(health, nullptr);
    EXPECT_EQ(health->progress.Value(), 50U);
    EXPECT_EQ(health->progress.Maximum(), 100U);
    EXPECT_EQ(health->progress.Revision(), 2U);
    EXPECT_EQ(ToString(UiProgressResult::InvalidRange), "invalid_range");
}

TEST(UiProgressTests, ScrollViewportCannotBeSpecializedAsProgress)
{
    UiDocument document(160U, 80U);
    AddElement(document, "viewport", UiElementKind::Panel, UiRect{8U, 8U, 100U, 40U});
    ASSERT_EQ(document.ConfigureScrollViewport("viewport", 100U, 80U), UiActionResult::Success);

    EXPECT_EQ(document.ConfigureProgress("viewport", 1U, 2U), UiProgressResult::InvalidTarget);
    const UiElement* viewport = document.Find("viewport");
    ASSERT_NE(viewport, nullptr);
    EXPECT_FALSE(viewport->progress.Active());
}

TEST(UiProgressTests, AgentReportsProgressRoleAndAssertableRetainedValues)
{
    UiDocument document(160U, 80U);
    AddElement(
        document,
        "health",
        UiElementKind::Panel,
        UiRect{8U, 8U, 100U, 12U},
        "Health");
    ASSERT_EQ(document.ConfigureProgress("health", 30U, 120U), UiProgressResult::Success);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiQueryOneResult query = facade.QueryOneUi(agent::UiSelector{
        .role = agent::UiRole::ProgressBar,
    });
    ASSERT_TRUE(query.Succeeded());
    ASSERT_TRUE(query.match.has_value());
    EXPECT_EQ(query.match->id, "health");
    EXPECT_EQ(query.match->role, agent::UiRole::ProgressBar);
    EXPECT_EQ(query.match->progressValue, 30U);
    EXPECT_EQ(query.match->progressMaximum, 120U);
    EXPECT_EQ(query.match->progressRevision, 1U);
    EXPECT_EQ(agent::ToString(query.match->role), "progressbar");

    const agent::UiAssertionResult assertion = facade.AssertUi(
        agent::UiSelector{.id = "health"},
        agent::UiExpectedState{
            .progressValue = 30U,
            .progressMaximum = 120U,
            .progressRevision = 1U,
        });
    EXPECT_TRUE(assertion.Succeeded());
}

TEST(UiProgressTests, RasterFillUsesDeterministicIntegerWidth)
{
    UiDocument document(16U, 8U);
    AddElement(document, "health", UiElementKind::Panel, UiRect{2U, 2U, 10U, 3U});
    ASSERT_EQ(document.ConfigureProgress("health", 5U, 10U), UiProgressResult::Success);

    UiRasterImage half{};
    UiRasterMetrics halfMetrics{};
    ASSERT_TRUE(RasterizeUi(document, half, &halfMetrics));
    EXPECT_EQ(halfMetrics.elementsRasterized, 1U);
    EXPECT_EQ(halfMetrics.glyphsRasterized, 0U);

    const std::array<std::uint8_t, 4U> fillPixel = PixelAt(half, 3U, 3U);
    const std::array<std::uint8_t, 4U> trackPixel = PixelAt(half, 8U, 3U);
    EXPECT_NE(fillPixel, trackPixel);

    ASSERT_EQ(document.SetProgress("health", 10U, 10U), UiProgressResult::Success);
    UiRasterImage full{};
    UiRasterMetrics fullMetrics{};
    ASSERT_TRUE(RasterizeUi(document, full, &fullMetrics));
    EXPECT_EQ(fullMetrics, halfMetrics);
    EXPECT_EQ(PixelAt(full, 8U, 3U), fillPixel);
}
} // namespace trace2d::ui
