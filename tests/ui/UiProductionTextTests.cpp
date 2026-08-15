#include <trace2d/agent/Inspection.hpp>
#include <trace2d/ui/UiTextLayout.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] std::vector<std::uint8_t> LoadUiProductionTextFont()
{
    std::ifstream input(std::filesystem::path{TRACE2D_UI_TEST_FONT_PATH}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] text::GlyphAtlasPrepareResult PrepareUiProductionTextAtlas(
    assets::ResourceRegistry& registry)
{
    assets::FontResource font{};
    font.canonicalBytes = LoadUiProductionTextFont();
    const auto published = registry.PublishFont("content/fonts/ui-production.ttf", std::move(font));
    EXPECT_TRUE(published.Succeeded());
    if (!published.Succeeded())
    {
        return {};
    }
    return text::PrepareGlyphAtlas(
        registry,
        published.handle,
        text::GlyphAtlasConfig{256U, 128U, 20U, 1U, 64U});
}

[[nodiscard]] UiDocument MakeFocusedTextInputDocument()
{
    UiDocument document(320U, 180U);
    EXPECT_EQ(
        document.AddElement(UiElement{
            .id = "chat",
            .kind = UiElementKind::TextInput,
            .bounds = UiRect{10U, 10U, 120U, 32U},
            .name = "Chat",
            .text = "A",
        }),
        UiActionResult::Success);
    EXPECT_EQ(document.Focus("chat"), UiActionResult::Success);
    return document;
}
} // namespace

TEST(UiProductionTextTests, ImeDisplayLayoutIsCachedAndExposedWithoutReplacingCommittedSemanticText)
{
    assets::ResourceRegistry registry("project");
    text::GlyphAtlasPrepareResult atlas = PrepareUiProductionTextAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());

    UiDocument document = MakeFocusedTextInputDocument();
    ASSERT_EQ(
        document.ApplyTextInput(input::TextInputEvent{
            .type = input::TextInputEventType::Composition,
            .text = "한",
            .selectionStart = 0,
            .selectionLength = 1,
        }),
        UiActionResult::Success);

    UiTextLayoutCachePrepareResult prepared = PrepareUiTextLayoutCache(
        UiTextLayoutCacheConfig{
            .text = text::TextLayoutCacheConfig{
                .layout = text::TextLayoutRunConfig{16U, 4U},
                .maxFallbackFonts = 2U,
            },
            .maxComposedUtf8Bytes = 64U,
        });
    ASSERT_TRUE(prepared.Succeeded());

    const std::array<text::TextFontAtlasRef, 1U> fallback{
        text::TextFontAtlasRef{atlas.atlas.get()},
    };
    const UiTextLayoutUpdateResult first = prepared.cache->Update(document, "chat", fallback);
    ASSERT_TRUE(first.Succeeded());
    EXPECT_FALSE(first.reused);
    EXPECT_TRUE(first.includesComposition);
    EXPECT_EQ(first.metrics->glyphCount, 2U);
    EXPECT_EQ(first.metrics->layoutWidth26_6, 120 * 64);
    EXPECT_EQ(first.metrics->layoutHeight26_6, 32 * 64);
    ASSERT_NE(prepared.cache->Layout(), nullptr);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiTreeResult inspected = facade.InspectUi();
    ASSERT_TRUE(inspected.Succeeded());
    ASSERT_EQ(inspected.tree->elements.size(), 1U);
    const agent::UiElementSnapshot& firstSnapshot = inspected.tree->elements.front();
    EXPECT_EQ(firstSnapshot.text, "A");
    ASSERT_TRUE(firstSnapshot.composition.has_value());
    EXPECT_EQ(firstSnapshot.composition->text, "한");
    EXPECT_EQ(firstSnapshot.composition->selectionStart, 0);
    EXPECT_EQ(firstSnapshot.composition->selectionLength, 1);
    ASSERT_TRUE(firstSnapshot.textLayout.has_value());
    EXPECT_TRUE(firstSnapshot.textLayout->includesComposition);
    EXPECT_EQ(firstSnapshot.textLayout->glyphCount, 2U);
    EXPECT_EQ(firstSnapshot.textLayout->layoutWidth26_6, 120 * 64);

    const text::GlyphAtlasMetrics beforeCacheHit = atlas.atlas->Metrics();
    const UiTextLayoutUpdateResult second = prepared.cache->Update(document, "chat", fallback);
    ASSERT_TRUE(second.Succeeded());
    EXPECT_TRUE(second.reused);
    const text::GlyphAtlasMetrics afterCacheHit = atlas.atlas->Metrics();
    EXPECT_EQ(afterCacheHit.cacheHits, beforeCacheHit.cacheHits);
    EXPECT_EQ(afterCacheHit.cacheMisses, beforeCacheHit.cacheMisses);
    EXPECT_EQ(afterCacheHit.rasterizations, beforeCacheHit.rasterizations);

    const UiElement* beforeSelectionOnly = document.Find("chat");
    ASSERT_NE(beforeSelectionOnly, nullptr);
    const std::uint64_t displayRevision = beforeSelectionOnly->displayTextRevision;
    ASSERT_EQ(
        document.ApplyTextInput(input::TextInputEvent{
            .type = input::TextInputEventType::Composition,
            .text = "한",
            .selectionStart = 1,
            .selectionLength = 0,
        }),
        UiActionResult::Success);
    ASSERT_NE(document.Find("chat"), nullptr);
    EXPECT_EQ(document.Find("chat")->displayTextRevision, displayRevision);

    const UiTextLayoutUpdateResult selectionOnly = prepared.cache->Update(document, "chat", fallback);
    ASSERT_TRUE(selectionOnly.Succeeded());
    EXPECT_TRUE(selectionOnly.reused);

    const agent::UiTreeResult selectionInspection = facade.InspectUi();
    ASSERT_TRUE(selectionInspection.Succeeded());
    ASSERT_TRUE(selectionInspection.tree->elements.front().composition.has_value());
    EXPECT_EQ(selectionInspection.tree->elements.front().composition->selectionStart, 1);
    EXPECT_EQ(selectionInspection.tree->elements.front().composition->selectionLength, 0);

    ASSERT_EQ(
        document.ApplyTextInput(input::TextInputEvent{
            .type = input::TextInputEventType::Committed,
            .text = "한",
        }),
        UiActionResult::Success);
    const UiTextLayoutUpdateResult committed = prepared.cache->Update(document, "chat", fallback);
    ASSERT_TRUE(committed.Succeeded());
    EXPECT_FALSE(committed.reused);
    EXPECT_FALSE(committed.includesComposition);
    EXPECT_EQ(committed.metrics->glyphCount, 2U);

    const agent::UiTreeResult committedInspection = facade.InspectUi();
    ASSERT_TRUE(committedInspection.Succeeded());
    const agent::UiElementSnapshot& committedSnapshot = committedInspection.tree->elements.front();
    EXPECT_EQ(committedSnapshot.text, "A한");
    EXPECT_FALSE(committedSnapshot.composition.has_value());
    ASSERT_TRUE(committedSnapshot.textLayout.has_value());
    EXPECT_FALSE(committedSnapshot.textLayout->includesComposition);
    EXPECT_EQ(committedSnapshot.textLayout->glyphCount, 2U);
}

TEST(UiProductionTextTests, BoundedCompositionScratchRejectsOversizeWithoutPublishingStaleEvidence)
{
    assets::ResourceRegistry registry("project");
    text::GlyphAtlasPrepareResult atlas = PrepareUiProductionTextAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());

    UiDocument document = MakeFocusedTextInputDocument();
    ASSERT_EQ(
        document.ApplyTextInput(input::TextInputEvent{
            .type = input::TextInputEventType::Composition,
            .text = "한",
            .selectionStart = 0,
            .selectionLength = 1,
        }),
        UiActionResult::Success);

    UiTextLayoutCachePrepareResult prepared = PrepareUiTextLayoutCache(
        UiTextLayoutCacheConfig{
            .text = text::TextLayoutCacheConfig{
                .layout = text::TextLayoutRunConfig{8U, 2U},
                .maxFallbackFonts = 1U,
            },
            .maxComposedUtf8Bytes = 2U,
        });
    ASSERT_TRUE(prepared.Succeeded());

    const text::TextFontAtlasRef fallback{atlas.atlas.get()};
    const UiTextLayoutUpdateResult failed = prepared.cache->Update(
        document,
        "chat",
        std::span<const text::TextFontAtlasRef>(&fallback, 1U));
    ASSERT_FALSE(failed.Succeeded());
    ASSERT_TRUE(failed.diagnostic.has_value());
    EXPECT_EQ(
        failed.diagnostic->code,
        UiTextLayoutErrorCode::ComposedTextCapacityExceeded);
    EXPECT_EQ(failed.diagnostic->requiredUtf8Bytes, 4U);
    ASSERT_NE(document.Find("chat"), nullptr);
    EXPECT_FALSE(document.Find("chat")->textLayout.valid);
}
} // namespace trace2d::ui
