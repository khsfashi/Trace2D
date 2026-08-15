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
name = "Main Menu"

[[elements]]
id = "title"
kind = "label"
bounds = [8, 8, 100, 16]
name = "Title"
text = "Trace2D UI"

[[elements]]
id = "start"
kind = "button"
bounds = [8, 32, 96, 24]
name = "Start Game"
text = "Start Game"

[[elements]]
id = "player_name"
kind = "text_input"
bounds = [8, 64, 120, 24]
name = "Player Name"
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
    EXPECT_EQ(elements[2].name, "Start Game");
    EXPECT_EQ(elements[3].name, "Player Name");
    EXPECT_EQ(elements[2].bounds, (UiRect{8U, 32U, 96U, 24U}));
    EXPECT_EQ(elements[2].localBounds, elements[2].bounds);
    EXPECT_TRUE(elements[2].parentId.empty());
    EXPECT_EQ(elements[2].parentIndex, InvalidUiElementIndex);
    EXPECT_EQ(elements[2].depth, 0U);
    EXPECT_EQ(elements[3].kind, UiElementKind::TextInput);
    EXPECT_TRUE(elements[3].visible);
}

TEST(UiTextTests, AuthoredHierarchyAndAnchorsCompileIntoRuntimeDocument)
{
    constexpr std::string_view HierarchicalUi = R"(format_version = 1

[canvas]
width = 320
height = 180

[[elements]]
id = "confirm"
kind = "button"
parent = "panel"
placement = "anchored_fixed"
anchor = [1024, 1024]
pivot = [1024, 1024]
offset = [-8, -6]
size = [64, 24]
name = "Confirm"
text = "OK"

[[elements]]
id = "label"
kind = "label"
parent = "panel"
bounds = [4, 5, 40, 12]
text = "Ready"

[[elements]]
id = "panel"
kind = "panel"
bounds = [40, 20, 200, 100]
name = "Panel"
)";

    UiLoadResult result = LoadUiToml(HierarchicalUi, "hierarchical_ui.trace2d.toml");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());

    const std::span<const UiElement> elements = result.document->Elements();
    ASSERT_EQ(elements.size(), 3U);

    // Authored order is stable even though both children are declared before their parent.
    EXPECT_EQ(elements[0].id, "confirm");
    EXPECT_EQ(elements[1].id, "label");
    EXPECT_EQ(elements[2].id, "panel");

    EXPECT_EQ(elements[0].parentId, "panel");
    EXPECT_EQ(elements[0].parentIndex, 2U);
    EXPECT_EQ(elements[0].depth, 1U);
    EXPECT_EQ(elements[0].localBounds, (UiRect{128U, 70U, 64U, 24U}));
    EXPECT_EQ(elements[0].bounds, (UiRect{168U, 90U, 64U, 24U}));

    EXPECT_EQ(elements[1].parentId, "panel");
    EXPECT_EQ(elements[1].parentIndex, 2U);
    EXPECT_EQ(elements[1].depth, 1U);
    EXPECT_EQ(elements[1].localBounds, (UiRect{4U, 5U, 40U, 12U}));
    EXPECT_EQ(elements[1].bounds, (UiRect{44U, 25U, 40U, 12U}));

    EXPECT_TRUE(elements[2].parentId.empty());
    EXPECT_EQ(elements[2].parentIndex, InvalidUiElementIndex);
    EXPECT_EQ(elements[2].depth, 0U);
    EXPECT_EQ(elements[2].localBounds, (UiRect{40U, 20U, 200U, 100U}));
    EXPECT_EQ(elements[2].bounds, elements[2].localBounds);
}

TEST(UiTextTests, InvalidAuthoredHierarchyAndPlacementDoNotPublishPartialDocument)
{
    constexpr std::string_view MissingParent = R"(format_version = 1
[canvas]
width = 64
height = 64
[[elements]]
id = "child"
kind = "label"
parent = "missing"
bounds = [0, 0, 8, 8]
)";
    const UiLoadResult missingParent = LoadUiToml(MissingParent);
    EXPECT_FALSE(missingParent.Succeeded());
    EXPECT_FALSE(missingParent.document.has_value());
    EXPECT_TRUE(HasDiagnosticPath(missingParent, "layout"));

    constexpr std::string_view Cycle = R"(format_version = 1
[canvas]
width = 64
height = 64
[[elements]]
id = "a"
kind = "panel"
parent = "b"
bounds = [0, 0, 16, 16]
[[elements]]
id = "b"
kind = "panel"
parent = "a"
bounds = [0, 0, 16, 16]
)";
    const UiLoadResult cycle = LoadUiToml(Cycle);
    EXPECT_FALSE(cycle.Succeeded());
    EXPECT_FALSE(cycle.document.has_value());
    EXPECT_TRUE(HasDiagnosticPath(cycle, "layout"));

    constexpr std::string_view ConflictingPlacement = R"(format_version = 1
[canvas]
width = 64
height = 64
[[elements]]
id = "bad"
kind = "button"
placement = "anchored_fixed"
bounds = [0, 0, 8, 8]
anchor = [0, 0]
pivot = [0, 0]
offset = [0, 0]
size = [8, 8]
)";
    const UiLoadResult conflicting = LoadUiToml(ConflictingPlacement);
    EXPECT_FALSE(conflicting.Succeeded());
    EXPECT_FALSE(conflicting.document.has_value());
    EXPECT_TRUE(HasDiagnosticPath(conflicting, "elements[0].bounds"));

    constexpr std::string_view InvalidNormalized = R"(format_version = 1
[canvas]
width = 64
height = 64
[[elements]]
id = "bad"
kind = "button"
placement = "anchored_fixed"
anchor = [1025, 0]
pivot = [0, 0]
offset = [0, 0]
size = [8, 8]
)";
    const UiLoadResult normalized = LoadUiToml(InvalidNormalized);
    EXPECT_FALSE(normalized.Succeeded());
    EXPECT_FALSE(normalized.document.has_value());
    EXPECT_TRUE(HasDiagnosticPath(normalized, "elements[0].anchor"));
}

TEST(UiStateTests, FocusActivationAndTextInputAreInspectableWithoutRendering)
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

    EXPECT_EQ(document.InputText("player_name", "Ada"), UiActionResult::Success);
    ASSERT_NE(document.Find("player_name"), nullptr);
    EXPECT_EQ(document.Find("player_name")->text, "Ada");
    EXPECT_EQ(document.Find("player_name")->name, "Player Name");

    EXPECT_EQ(document.Activate("title"), UiActionResult::NotActivatable);
    EXPECT_EQ(document.Activate("start"), UiActionResult::Success);
    EXPECT_EQ(document.Activate("start"), UiActionResult::Success);

    const UiElement* start = document.Find("start");
    ASSERT_NE(start, nullptr);
    EXPECT_EQ(start->activationCount, 2U);

    document.ClearFocus();
    EXPECT_EQ(document.FocusedElement(), nullptr);
    EXPECT_EQ(document.InputText("player_name", "Grace"), UiActionResult::NotFocused);
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

TEST(UiStateTests, DisabledMissingAndWrongTargetsFailDeterministically)
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
    EXPECT_EQ(document.InputText("disabled", "x"), UiActionResult::Disabled);
    EXPECT_EQ(document.Focus("missing"), UiActionResult::NotFound);
    EXPECT_EQ(document.Activate("missing"), UiActionResult::NotFound);
    EXPECT_EQ(document.InputText("missing", "x"), UiActionResult::NotFound);
    EXPECT_EQ(ToString(UiActionResult::NotFound), "not_found");
}

TEST(UiStateTests, HiddenStateControlsActionsAndRasterization)
{
    constexpr std::string_view HiddenUi = R"(format_version = 1

[canvas]
width = 64
height = 32

[[elements]]
id = "visible_label"
kind = "label"
bounds = [0, 0, 32, 12]
text = "Visible"

[[elements]]
id = "hidden_button"
kind = "button"
bounds = [0, 16, 32, 16]
name = "Hidden Button"
text = "Hidden"
visible = false
)";

    UiLoadResult result = LoadUiToml(HiddenUi);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());
    UiDocument& document = *result.document;

    const UiElement* hidden = document.Find("hidden_button");
    ASSERT_NE(hidden, nullptr);
    EXPECT_FALSE(hidden->visible);
    EXPECT_EQ(document.Focus("hidden_button"), UiActionResult::NotVisible);
    EXPECT_EQ(document.Activate("hidden_button"), UiActionResult::NotVisible);

    UiRasterImage image{};
    UiRasterMetrics metrics{};
    ASSERT_TRUE(RasterizeUi(document, image, &metrics));
    EXPECT_EQ(metrics.elementsRasterized, 1U);
}
} // namespace
} // namespace trace2d::ui
