#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] bool HasDiagnostic(
    const UiLoadResult& result,
    const std::string_view path,
    const std::string_view messagePart = {})
{
    return std::any_of(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [path, messagePart](const UiTextDiagnostic& diagnostic)
        {
            return diagnostic.path == path &&
                   (messagePart.empty() || diagnostic.message.find(messagePart) != std::string::npos);
        });
}
} // namespace

TEST(UiAuthoredScrollTests, AuthoredContentSizeDrivesLayoutAndRuntimeScrollAuthority)
{
    constexpr std::string_view source = R"toml(
format_version = 1

[canvas]
width = 320
height = 240

[[elements]]
id = "viewport"
kind = "panel"
bounds = [16, 20, 120, 80]
scroll_content_size = [120, 160]

[[elements]]
id = "late_button"
kind = "button"
parent = "viewport"
bounds = [8, 112, 80, 24]
text = "Late"
)toml";

    UiLoadResult result = LoadUiToml(source, "authored-scroll.toml");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());

    UiDocument& document = *result.document;
    const UiElement* viewport = document.Find("viewport");
    const UiElement* button = document.Find("late_button");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(button, nullptr);

    EXPECT_TRUE(viewport->scroll.viewport);
    EXPECT_TRUE(viewport->clipChildren);
    EXPECT_EQ(viewport->scroll.contentWidth, 120U);
    EXPECT_EQ(viewport->scroll.contentHeight, 160U);
    EXPECT_EQ(viewport->bounds, (UiRect{16U, 20U, 120U, 80U}));

    EXPECT_EQ(button->localBounds, (UiRect{8U, 112U, 80U, 24U}));
    EXPECT_EQ(button->bounds, (UiRect{24U, 132U, 80U, 24U}));
    EXPECT_EQ(button->scrollOwnerIndex, 0U);
    EXPECT_TRUE(button->clipActive);
    EXPECT_EQ(button->clipBounds, viewport->bounds);

    ASSERT_EQ(document.ScrollTo("viewport", 0U, 999U), UiActionResult::Success);
    viewport = document.Find("viewport");
    button = document.Find("late_button");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(viewport->scroll.offsetY, 80U);
    EXPECT_EQ(button->bounds, (UiRect{24U, 132U, 80U, 24U}));
    EXPECT_EQ(button->presentationBounds, (UiPresentationRect{24, 52, 80U, 24U}));
}

TEST(UiAuthoredScrollTests, LegacyPanelStillRejectsChildOutsideVisibleBounds)
{
    constexpr std::string_view source = R"toml(
format_version = 1

[canvas]
width = 320
height = 240

[[elements]]
id = "viewport"
kind = "panel"
bounds = [16, 20, 120, 80]

[[elements]]
id = "late_button"
kind = "button"
parent = "viewport"
bounds = [8, 112, 80, 24]
)toml";

    const UiLoadResult result = LoadUiToml(source, "legacy-containment.toml");
    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(HasDiagnostic(result, "layout", "child_outside_parent"));
}

TEST(UiAuthoredScrollTests, RejectsMalformedAndNonPanelScrollDeclarations)
{
    constexpr std::string_view malformed = R"toml(
format_version = 1

[canvas]
width = 320
height = 240

[[elements]]
id = "viewport"
kind = "panel"
bounds = [0, 0, 120, 80]
scroll_content_size = [120]
)toml";

    const UiLoadResult malformedResult = LoadUiToml(malformed, "malformed-scroll.toml");
    EXPECT_FALSE(malformedResult.Succeeded());
    EXPECT_FALSE(malformedResult.document.has_value());
    EXPECT_TRUE(HasDiagnostic(malformedResult, "elements[0].scroll_content_size"));

    constexpr std::string_view nonPanel = R"toml(
format_version = 1

[canvas]
width = 320
height = 240

[[elements]]
id = "label"
kind = "label"
bounds = [0, 0, 120, 24]
scroll_content_size = [120, 80]
)toml";

    const UiLoadResult nonPanelResult = LoadUiToml(nonPanel, "non-panel-scroll.toml");
    EXPECT_FALSE(nonPanelResult.Succeeded());
    EXPECT_FALSE(nonPanelResult.document.has_value());
    EXPECT_TRUE(HasDiagnostic(
        nonPanelResult,
        "elements[0].scroll_content_size",
        "only valid for kind = \"panel\""));
}

TEST(UiAuthoredScrollTests, NestedAuthoredScrollIsRejectedTransactionallyByU9)
{
    constexpr std::string_view source = R"toml(
format_version = 1

[canvas]
width = 320
height = 240

[[elements]]
id = "outer"
kind = "panel"
bounds = [0, 0, 200, 100]
scroll_content_size = [200, 200]

[[elements]]
id = "inner"
kind = "panel"
parent = "outer"
bounds = [10, 120, 100, 60]
scroll_content_size = [100, 60]
)toml";

    const UiLoadResult result = LoadUiToml(source, "nested-scroll.toml");
    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(HasDiagnostic(
        result,
        "elements[1].scroll_content_size",
        "unsupported_scroll_hierarchy"));
}
} // namespace trace2d::ui
