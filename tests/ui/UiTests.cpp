#include <trace2d/ui/Ui.hpp>
#include <trace2d/ui/UiRaster.hpp>
#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace trace2d::ui
{
namespace
{
constexpr std::string_view BasicUi = R"(format_version = 1

[canvas]
width = 160
height = 96

[[elements]]
id = "root"
kind = "panel"
bounds = [0, 0, 160, 96]

[[elements]]
id = "title"
kind = "label"
bounds = [8, 8, 100, 16]
text = "Trace2D UI"

[[elements]]
id = "start"
kind = "button"
bounds = [8, 32, 96, 24]
text = "Start Game"

[[elements]]
id = "player_name"
kind = "text_input"
bounds = [8, 64, 120, 24]
text = "Player"
)";

[[nodiscard]] bool HasDiagnosticPath(
    const UiLoadResult& result,
    const std::string_view path)
{
    for (const UiTextDiagnostic& diagnostic : result.diagnostics)
    {
        if (diagnostic.path == path)
        {
            return true;
        }
    }
    return false;
}

TEST(UiTextTests, AuthoredUiLoadsWithStableOrderAndDeterministicBounds)
{
    UiLoadResult result = LoadUiToml(BasicUi, "basic_ui.trace2d.toml");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());

    const UiDocument& document = *result.document;
    EXPECT_EQ(document.Width(), 160U);
    EXPECT_EQ(document.Height(), 96U);

    const std::span<const UiElement> elements = document.Elements();
    ASSERT_EQ(elements.size(), 4U);
    EXPECT_EQ(elements[0].id, "root");
    EXPECT_EQ(elements[1].id, "title");
    EXPECT_EQ(elements[2].id, "start");
    EXPECT_EQ(elements[3].id, "player_name");
    EXPECT_EQ(elements[2].bounds, (UiRect{8U, 32U, 96U, 24U}));
    EXPECT_EQ(elements[3].kind, UiElementKind::TextInput);
}

TEST(UiStateTests, FocusAndActivationAreInspectableWithoutRendering)
{
    UiLoadResult result = LoadUiToml(BasicUi);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());
    UiDocument& document = *result.document;

    EXPECT_EQ(document.Focus("title"), UiActionResult::NotFocusable);
    EXPECT_EQ(document.Focus("player_name"), UiActionResult::Success);
    ASSERT_NE(document.FocusedElement(), nullptr);
    EXPECT_EQ(document.FocusedElement()->id, "player_name");
    EXPECT_TRUE(document.IsFocused("player_name"));

    EXPECT_EQ(document.Activate("title"), UiActionResult::NotActivatable);
    EXPECT_EQ(document.Activate("start"), UiActionResult::Success);
    EXPECT_EQ(document.Activate("start"), UiActionResult::Success);

    const UiElement* start = document.Find("start");
    ASSERT_NE(start, nullptr);
    EXPECT_EQ(start->activationCount, 2U);

    document.ClearFocus();
    EXPECT_EQ(document.FocusedElement(), nullptr);
}

TEST(UiRasterTests, SameDocumentProducesSamePixelsAndReusesOutputStorage)
{
    UiLoadResult result = LoadUiToml(BasicUi);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());
    UiDocument& document = *result.document;
    ASSERT_EQ(document.Focus("start"), UiActionResult::Success);

    UiRasterImage image{};
    UiRasterMetrics firstMetrics{};
    ASSERT_TRUE(RasterizeUi(document, image, &firstMetrics));
    ASSERT_EQ(image.width, 160U);
    ASSERT_EQ(image.height, 96U);
    ASSERT_EQ(image.rgba8.size(), 160U * 96U * 4U);
    EXPECT_EQ(firstMetrics.elementsRasterized, 4U);
    EXPECT_EQ(firstMetrics.glyphsRasterized, 24U);

    const std::uint8_t* const firstStorage = image.rgba8.data();
    const std::vector<std::uint8_t> firstPixels = image.rgba8;

    UiRasterMetrics secondMetrics{};
    ASSERT_TRUE(RasterizeUi(document, image, &secondMetrics));
    EXPECT_EQ(image.rgba8.data(), firstStorage);
    EXPECT_EQ(image.rgba8, firstPixels);
    EXPECT_EQ(secondMetrics, firstMetrics);
}

TEST(UiTextTests, InvalidAuthoredUiReturnsStructuredPaths)
{
    constexpr std::string_view InvalidUi = R"(format_version = 1
unknown_root = true

[canvas]
width = 64
height = 64

[[elements]]
id = "duplicate"
kind = "button"
bounds = [0, 0, 16, 16]

[[elements]]
id = "duplicate"
kind = "label"
bounds = [20, 0, 16, 16]

[[elements]]
id = "outside"
kind = "label"
bounds = [60, 60, 8, 8]
mystery = 1
)";

    const UiLoadResult result = LoadUiToml(InvalidUi, "invalid_ui.trace2d.toml");
    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(HasDiagnosticPath(result, "unknown_root"));
    EXPECT_TRUE(HasDiagnosticPath(result, "elements[1].id"));
    EXPECT_TRUE(HasDiagnosticPath(result, "elements[2].bounds"));
    EXPECT_TRUE(HasDiagnosticPath(result, "elements[2].mystery"));
}

TEST(UiStateTests, DisabledAndMissingTargetsFailDeterministically)
{
    UiDocument document{64U, 64U};
    UiElement disabled{};
    disabled.id = "disabled";
    disabled.kind = UiElementKind::Button;
    disabled.bounds = UiRect{4U, 4U, 24U, 16U};
    disabled.enabled = false;
    ASSERT_EQ(document.AddElement(disabled), UiActionResult::Success);

    EXPECT_EQ(document.Focus("disabled"), UiActionResult::Disabled);
    EXPECT_EQ(document.Activate("disabled"), UiActionResult::Disabled);
    EXPECT_EQ(document.Focus("missing"), UiActionResult::NotFound);
    EXPECT_EQ(document.Activate("missing"), UiActionResult::NotFound);
    EXPECT_EQ(ToString(UiActionResult::NotFound), "not_found");
}
} // namespace
} // namespace trace2d::ui
