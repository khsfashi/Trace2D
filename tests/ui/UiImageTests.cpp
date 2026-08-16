#include <trace2d/agent/Inspection.hpp>
#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/ui/Ui.hpp>
#include <trace2d/ui/UiRaster.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

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

[[nodiscard]] assets::ResourceHandle<assets::TextureResource> PublishTexture(
    assets::ResourceRegistry& resources,
    const std::string_view reference,
    const std::uint32_t width,
    const std::uint32_t height,
    std::vector<std::uint8_t> rgba8)
{
    assets::TextureResource texture{};
    texture.width = width;
    texture.height = height;
    texture.cpuRetention = assets::CpuRetentionPolicy::Required;
    texture.retentionReason = "UI deterministic raster fixture";
    texture.canonicalRgba8 = std::move(rgba8);

    const auto published = resources.PublishTexture(reference, std::move(texture));
    EXPECT_TRUE(published.Succeeded());
    return published.handle;
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

TEST(UiImageTests, ConfigureAndSetUseLiveGenerationSafeHandlesAndNoOpRevision)
{
    assets::ResourceRegistry resources{"."};
    const auto first = PublishTexture(
        resources,
        "ui/first.rgba",
        1U,
        1U,
        {255U, 0U, 0U, 255U});
    const auto second = PublishTexture(
        resources,
        "ui/second.rgba",
        1U,
        1U,
        {0U, 255U, 0U, 255U});

    UiDocument document(32U, 16U);
    AddElement(document, "portrait", UiElementKind::Panel, UiRect{2U, 2U, 8U, 8U});
    AddElement(document, "label", UiElementKind::Label, UiRect{12U, 2U, 8U, 8U});

    ASSERT_EQ(document.ConfigureImage("portrait", first, resources), UiImageResult::Success);
    const UiElement* portrait = document.Find("portrait");
    ASSERT_NE(portrait, nullptr);
    EXPECT_TRUE(portrait->image.Active());
    EXPECT_EQ(portrait->image.Texture(), first);
    EXPECT_EQ(portrait->image.Revision(), 1U);

    EXPECT_EQ(document.SetImage("portrait", first, resources), UiImageResult::Success);
    EXPECT_EQ(document.Find("portrait")->image.Revision(), 1U);

    EXPECT_EQ(document.SetImage("portrait", second, resources), UiImageResult::Success);
    portrait = document.Find("portrait");
    ASSERT_NE(portrait, nullptr);
    EXPECT_EQ(portrait->image.Texture(), second);
    EXPECT_EQ(portrait->image.Revision(), 2U);

    const assets::ResourceHandle<assets::TextureResource> invalid{
        999U,
        1U,
        assets::ResourceTypeDomain::Texture};
    EXPECT_EQ(document.SetImage("portrait", invalid, resources), UiImageResult::InvalidTexture);
    EXPECT_EQ(document.ConfigureImage("label", first, resources), UiImageResult::InvalidTarget);
    EXPECT_EQ(document.ConfigureImage("missing", first, resources), UiImageResult::NotFound);
    EXPECT_EQ(document.SetImage("label", first, resources), UiImageResult::NotImage);
    EXPECT_EQ(document.ConfigureProgress("portrait", 1U, 2U), UiProgressResult::InvalidTarget);
    EXPECT_EQ(ToString(UiImageResult::InvalidTexture), "invalid_texture");

    portrait = document.Find("portrait");
    ASSERT_NE(portrait, nullptr);
    EXPECT_EQ(portrait->image.Texture(), second);
    EXPECT_EQ(portrait->image.Revision(), 2U);
}

TEST(UiImageTests, AgentReportsImageRoleAndHandleGenerationEvidence)
{
    assets::ResourceRegistry resources{"."};
    const auto texture = PublishTexture(
        resources,
        "ui/agent-image.rgba",
        1U,
        1U,
        {20U, 40U, 60U, 255U});

    UiDocument document(32U, 16U);
    AddElement(document, "portrait", UiElementKind::Panel, UiRect{2U, 2U, 8U, 8U}, "Portrait");
    ASSERT_EQ(document.ConfigureImage("portrait", texture, resources), UiImageResult::Success);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiQueryOneResult query = facade.QueryOneUi(agent::UiSelector{
        .role = agent::UiRole::Image,
    });
    ASSERT_TRUE(query.Succeeded());
    ASSERT_TRUE(query.match.has_value());
    EXPECT_EQ(query.match->id, "portrait");
    EXPECT_EQ(query.match->role, agent::UiRole::Image);
    EXPECT_EQ(query.match->imageTextureSlot, texture.slot);
    EXPECT_EQ(query.match->imageTextureGeneration, texture.generation);
    EXPECT_EQ(query.match->imageRevision, 1U);
    EXPECT_EQ(agent::ToString(query.match->role), "image");

    const agent::UiAssertionResult assertion = facade.AssertUi(
        agent::UiSelector{.id = "portrait"},
        agent::UiExpectedState{
            .imageTextureSlot = texture.slot,
            .imageTextureGeneration = texture.generation,
            .imageRevision = 1U,
        });
    EXPECT_TRUE(assertion.Succeeded());
}

TEST(UiImageTests, RasterUsesNearestNeighborAndRequiresLiveCanonicalTexture)
{
    assets::ResourceRegistry resources{"."};
    const auto texture = PublishTexture(
        resources,
        "ui/two-pixel.rgba",
        2U,
        1U,
        {
            255U, 0U, 0U, 255U,
            0U, 255U, 0U, 255U,
        });

    UiDocument document(6U, 4U);
    AddElement(document, "image", UiElementKind::Panel, UiRect{1U, 1U, 4U, 2U});
    ASSERT_EQ(document.ConfigureImage("image", texture, resources), UiImageResult::Success);

    UiRasterImage withoutResources{};
    EXPECT_FALSE(RasterizeUi(document, withoutResources));

    UiRasterImage image{};
    UiRasterMetrics metrics{};
    ASSERT_TRUE(RasterizeUi(document, resources, image, &metrics));
    EXPECT_EQ(metrics.elementsRasterized, 1U);
    EXPECT_EQ(metrics.glyphsRasterized, 0U);

    const std::array<std::uint8_t, 4U> red{255U, 0U, 0U, 255U};
    const std::array<std::uint8_t, 4U> green{0U, 255U, 0U, 255U};
    EXPECT_EQ(PixelAt(image, 1U, 1U), red);
    EXPECT_EQ(PixelAt(image, 2U, 1U), red);
    EXPECT_EQ(PixelAt(image, 3U, 1U), green);
    EXPECT_EQ(PixelAt(image, 4U, 1U), green);

    ASSERT_TRUE(resources.Unload(texture.Untyped()).Succeeded());
    EXPECT_EQ(document.SetImage("image", texture, resources), UiImageResult::InvalidTexture);
    EXPECT_FALSE(RasterizeUi(document, resources, image));
}

TEST(UiImageTests, ScrolledImageSamplingUsesTranslatedPresentationAndResolvedClip)
{
    assets::ResourceRegistry resources{"."};
    const auto texture = PublishTexture(
        resources,
        "ui/scroll-image.rgba",
        1U,
        4U,
        {
            10U, 0U, 0U, 255U,
            20U, 0U, 0U, 255U,
            30U, 0U, 0U, 255U,
            40U, 0U, 0U, 255U,
        });

    UiDocument document(4U, 4U);
    AddElement(document, "viewport", UiElementKind::Panel, UiRect{0U, 0U, 4U, 2U});

    UiElement child{};
    child.id = "image";
    child.kind = UiElementKind::Panel;
    child.parentId = "viewport";
    child.parentIndex = 0U;
    child.depth = 1U;
    child.bounds = UiRect{0U, 0U, 4U, 4U};
    ASSERT_EQ(document.AddElement(std::move(child)), UiActionResult::Success);

    ASSERT_EQ(document.ConfigureScrollViewport("viewport", 4U, 4U), UiActionResult::Success);
    ASSERT_EQ(document.ConfigureImage("image", texture, resources), UiImageResult::Success);
    ASSERT_EQ(document.ScrollTo("viewport", 0U, 2U), UiActionResult::Success);

    UiRasterImage image{};
    ASSERT_TRUE(RasterizeUi(document, resources, image));
    EXPECT_EQ(PixelAt(image, 1U, 0U), (std::array<std::uint8_t, 4U>{30U, 0U, 0U, 255U}));
    EXPECT_EQ(PixelAt(image, 1U, 1U), (std::array<std::uint8_t, 4U>{40U, 0U, 0U, 255U}));
    EXPECT_NE(PixelAt(image, 1U, 2U), (std::array<std::uint8_t, 4U>{40U, 0U, 0U, 255U}));
}
} // namespace trace2d::ui
