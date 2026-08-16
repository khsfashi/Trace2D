#include <trace2d/agent/Inspection.hpp>
#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/ui/UiRaster.hpp>
#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

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

[[nodiscard]] assets::ResourceHandle<assets::TextureResource> PublishTexture(
    assets::ResourceRegistry& resources,
    const std::string_view reference,
    std::vector<std::uint8_t> rgba8)
{
    assets::TextureResource texture{};
    texture.width = 1U;
    texture.height = 1U;
    texture.cpuRetention = assets::CpuRetentionPolicy::Required;
    texture.retentionReason = "U14 authored Image fixture";
    texture.canonicalRgba8 = std::move(rgba8);

    const auto published = resources.PublishTexture(reference, std::move(texture));
    EXPECT_TRUE(published.Succeeded());
    return published.handle;
}
} // namespace

TEST(UiAuthoredWidgetTests, ProgressConvergesOnExistingRuntimeAgentAndRasterAuthority)
{
    constexpr std::string_view source = R"toml(
format_version = 1

[canvas]
width = 160
height = 80

[[elements]]
id = "health"
kind = "progress"
bounds = [8, 8, 100, 12]
name = "Health"
progress_value = 25
progress_maximum = 100
)toml";

    UiLoadResult result = LoadUiToml(source, "authored-progress.toml");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());

    UiDocument& document = *result.document;
    const UiElement* health = document.Find("health");
    ASSERT_NE(health, nullptr);
    EXPECT_EQ(health->kind, UiElementKind::Panel);
    EXPECT_TRUE(health->progress.Active());
    EXPECT_EQ(health->progress.Value(), 25U);
    EXPECT_EQ(health->progress.Maximum(), 100U);
    EXPECT_EQ(health->progress.Revision(), 1U);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiQueryOneResult query = facade.QueryOneUi(agent::UiSelector{
        .role = agent::UiRole::ProgressBar,
    });
    ASSERT_TRUE(query.Succeeded());
    ASSERT_TRUE(query.match.has_value());
    EXPECT_EQ(query.match->id, "health");
    EXPECT_EQ(query.match->progressValue, 25U);
    EXPECT_EQ(query.match->progressMaximum, 100U);

    UiRasterImage raster{};
    UiRasterMetrics metrics{};
    EXPECT_TRUE(RasterizeUi(document, raster, &metrics));
    EXPECT_EQ(metrics.elementsRasterized, 1U);
}

TEST(UiAuthoredWidgetTests, ImageResolvesCanonicalEquivalentReadyTextureWithoutRepublish)
{
    assets::ResourceRegistry resources{"."};
    const auto published = PublishTexture(
        resources,
        "ui/portrait.rgba",
        {40U, 80U, 120U, 255U});
    const std::uint64_t duplicateReadyLoadsBefore = resources.Stats().duplicateReadyLoads;

    const auto firstLookup = resources.FindReadyTexture("./ui/portrait.rgba");
    const auto secondLookup = resources.FindReadyTexture("ui/./portrait.rgba");
    ASSERT_TRUE(firstLookup.has_value());
    ASSERT_TRUE(secondLookup.has_value());
    EXPECT_EQ(*firstLookup, published);
    EXPECT_EQ(*secondLookup, published);
    EXPECT_EQ(resources.Stats().duplicateReadyLoads, duplicateReadyLoadsBefore);

    constexpr std::string_view source = R"toml(
format_version = 1

[canvas]
width = 32
height = 16

[[elements]]
id = "portrait"
kind = "image"
bounds = [2, 2, 8, 8]
name = "Portrait"
texture = "./ui/portrait.rgba"
)toml";

    UiLoadResult result = LoadUiToml(source, resources, "authored-image.toml");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.document.has_value());

    UiDocument& document = *result.document;
    const UiElement* portrait = document.Find("portrait");
    ASSERT_NE(portrait, nullptr);
    EXPECT_EQ(portrait->kind, UiElementKind::Panel);
    EXPECT_TRUE(portrait->image.Active());
    EXPECT_EQ(portrait->image.Texture(), published);
    EXPECT_EQ(portrait->image.Revision(), 1U);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiQueryOneResult query = facade.QueryOneUi(agent::UiSelector{
        .role = agent::UiRole::Image,
    });
    ASSERT_TRUE(query.Succeeded());
    ASSERT_TRUE(query.match.has_value());
    EXPECT_EQ(query.match->id, "portrait");
    EXPECT_EQ(query.match->imageTextureSlot, published.slot);
    EXPECT_EQ(query.match->imageTextureGeneration, published.generation);

    UiRasterImage raster{};
    EXPECT_TRUE(RasterizeUi(document, resources, raster));
}

TEST(UiAuthoredWidgetTests, ImageRequiresReadyResourceAndResourceAwareLoad)
{
    constexpr std::string_view source = R"toml(
format_version = 1

[canvas]
width = 32
height = 16

[[elements]]
id = "portrait"
kind = "image"
bounds = [2, 2, 8, 8]
texture = "ui/missing.rgba"
)toml";

    const UiLoadResult noRegistry = LoadUiToml(source, "image-no-registry.toml");
    EXPECT_FALSE(noRegistry.Succeeded());
    EXPECT_FALSE(noRegistry.document.has_value());
    EXPECT_TRUE(HasDiagnostic(noRegistry, "elements[0].texture", "resource-aware"));

    assets::ResourceRegistry resources{"."};
    const UiLoadResult missing = LoadUiToml(source, resources, "image-missing.toml");
    EXPECT_FALSE(missing.Succeeded());
    EXPECT_FALSE(missing.document.has_value());
    EXPECT_TRUE(HasDiagnostic(missing, "elements[0].texture", "not a ready TextureResource"));

    const auto texture = PublishTexture(
        resources,
        "ui/missing.rgba",
        {255U, 255U, 255U, 255U});
    ASSERT_TRUE(resources.Unload(texture.Untyped()).Succeeded());

    const UiLoadResult unloaded = LoadUiToml(source, resources, "image-unloaded.toml");
    EXPECT_FALSE(unloaded.Succeeded());
    EXPECT_FALSE(unloaded.document.has_value());
    EXPECT_TRUE(HasDiagnostic(unloaded, "elements[0].texture", "not a ready TextureResource"));
}

TEST(UiAuthoredWidgetTests, InvalidProgressRangeAndWidgetOnlyFieldsRejectTransaction)
{
    constexpr std::string_view invalidRange = R"toml(
format_version = 1

[canvas]
width = 160
height = 80

[[elements]]
id = "health"
kind = "progress"
bounds = [8, 8, 100, 12]
progress_value = 101
progress_maximum = 100
)toml";

    const UiLoadResult rangeResult = LoadUiToml(invalidRange, "invalid-progress.toml");
    EXPECT_FALSE(rangeResult.Succeeded());
    EXPECT_FALSE(rangeResult.document.has_value());
    EXPECT_TRUE(HasDiagnostic(rangeResult, "elements[0].progress_value", "invalid_range"));

    constexpr std::string_view wrongField = R"toml(
format_version = 1

[canvas]
width = 160
height = 80

[[elements]]
id = "panel"
kind = "panel"
bounds = [8, 8, 100, 12]
progress_value = 1
progress_maximum = 2
)toml";

    const UiLoadResult wrongFieldResult = LoadUiToml(wrongField, "wrong-widget-field.toml");
    EXPECT_FALSE(wrongFieldResult.Succeeded());
    EXPECT_FALSE(wrongFieldResult.document.has_value());
    EXPECT_TRUE(HasDiagnostic(wrongFieldResult, "elements[0].progress_value", "only valid"));
    EXPECT_TRUE(HasDiagnostic(wrongFieldResult, "elements[0].progress_maximum", "only valid"));
}

TEST(UiAuthoredWidgetTests, ProgressAndImageCannotClaimScrollViewportAuthority)
{
    constexpr std::string_view progressScroll = R"toml(
format_version = 1

[canvas]
width = 160
height = 80

[[elements]]
id = "health"
kind = "progress"
bounds = [8, 8, 100, 12]
progress_value = 1
progress_maximum = 2
scroll_content_size = [100, 24]
)toml";

    const UiLoadResult progressResult = LoadUiToml(progressScroll, "progress-scroll.toml");
    EXPECT_FALSE(progressResult.Succeeded());
    EXPECT_FALSE(progressResult.document.has_value());
    EXPECT_TRUE(HasDiagnostic(progressResult, "elements[0].scroll_content_size", "only valid for kind = \"panel\""));

    assets::ResourceRegistry resources{"."};
    static_cast<void>(PublishTexture(resources, "ui/image.rgba", {1U, 2U, 3U, 255U}));
    constexpr std::string_view imageScroll = R"toml(
format_version = 1

[canvas]
width = 160
height = 80

[[elements]]
id = "portrait"
kind = "image"
bounds = [8, 8, 40, 40]
texture = "ui/image.rgba"
scroll_content_size = [40, 80]
)toml";

    const UiLoadResult imageResult = LoadUiToml(imageScroll, resources, "image-scroll.toml");
    EXPECT_FALSE(imageResult.Succeeded());
    EXPECT_FALSE(imageResult.document.has_value());
    EXPECT_TRUE(HasDiagnostic(imageResult, "elements[0].scroll_content_size", "only valid for kind = \"panel\""));
}
} // namespace trace2d::ui
