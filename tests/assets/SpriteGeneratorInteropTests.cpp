#include <trace2d/assets/SpriteGeneratorInterop.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace trace2d::assets
{
namespace
{
std::vector<std::uint8_t> MakeGeneratorPixels(
    const std::uint32_t width,
    const std::uint32_t height)
{
    return std::vector<std::uint8_t>(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U,
        0x7fU);
}

SpriteGeneratorManifestImportOptions MakeGeneratorOptions(
    const std::optional<SpriteRationalPivot> spriteGenPivot =
        SpriteRationalPivot{1, 2, 2})
{
    return SpriteGeneratorManifestImportOptions{
        .canonicalAssetId = "sprites/generated.sprite.toml",
        .pageId = "main",
        .textureReference = "textures/generated.png",
        .sampling = SpriteSampling::Nearest,
        .colorSpace = SpriteColorSpace::Srgb,
        .spriteGenDefaultPivot = spriteGenPivot,
    };
}

std::string SpriteGenManifest()
{
    return R"json({
  "characterId": "hero",
  "engine": "component-row",
  "game_input": "sprite-sheet-alpha.png",
  "degraded_static_fallback": false,
  "animation": {
    "cellWidth": 2,
    "cellHeight": 2,
    "columns": 2,
    "rows": {
      "walk": {
        "row": 1,
        "frames": 1,
        "fps": 12,
        "durations_ms": [83],
        "loop": false,
        "frame_variant": "pixel"
      },
      "idle": {
        "row": 0,
        "frames": 2,
        "fps": 10,
        "durations_ms": [100, 200],
        "loop": true,
        "frame_variant": "pixel"
      }
    }
  },
  "frame_layout": {
    "sheetWidth": 4,
    "sheetHeight": 4,
    "cellWidth": 2,
    "cellHeight": 2,
    "rows": {
      "walk": [
        {"x":0,"y":2,"w":2,"h":2}
      ],
      "idle": [
        {"x":0,"y":0,"w":2,"h":2},
        {"x":0,"y":0,"w":2,"h":2}
      ]
    }
  }
})json";
}

std::string PerfectPixelManifest()
{
    return R"json({
  "app": "perfectpixel",
  "generator": "perfectpixel/component-lane",
  "schema": "perfectpixel.sprite/2",
  "version": 2,
  "character": "hero",
  "sheet": {
    "image": "sprite-sheet.png",
    "width": 8,
    "height": 8,
    "cellWidth": 4,
    "cellHeight": 4
  },
  "animations": {
    "walk": {
      "row": 1,
      "frames": 1,
      "fps": 12,
      "loop": false,
      "durationMs": 83,
      "pivot": {"x":2,"y":4},
      "rects": [
        {"x":0,"y":4,"w":4,"h":4}
      ],
      "trims": [
        {"x":0,"y":1,"w":3,"h":3}
      ]
    },
    "idle": {
      "row": 0,
      "frames": 2,
      "fps": 8,
      "loop": true,
      "durationMs": 125,
      "pivot": {"x":2,"y":4},
      "rects": [
        {"x":0,"y":0,"w":4,"h":4},
        {"x":4,"y":0,"w":4,"h":4}
      ],
      "trims": [
        {"x":1,"y":1,"w":2,"h":3},
        {"x":0,"y":0,"w":4,"h":4}
      ]
    }
  }
})json";
}
} // namespace

TEST(SpriteGeneratorInteropTests, SpriteGenOrdersByRowPreservesRepeatedSlotsAndExactDurations)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(4U, 4U);
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet-alpha.png",
        .width = 4U,
        .height = 4U,
        .rgba8 = pixels,
    };

    const SpriteGeneratorImportResult result = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::SpriteGenComponentRow,
        SpriteGenManifest(),
        sheet,
        MakeGeneratorOptions());

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.manifestKind, SpriteGeneratorManifestKind::SpriteGenComponentRow);
    EXPECT_EQ(result.canonicalImport.sourceKind, SpriteImportSourceKind::GenericSheet);

    ASSERT_EQ(result.animations.size(), 2U);
    EXPECT_EQ(result.animations[0].name, "idle");
    EXPECT_EQ(result.animations[0].row, 0U);
    EXPECT_EQ(result.animations[0].firstFrame, 0U);
    EXPECT_EQ(result.animations[0].frameCount, 2U);
    EXPECT_EQ(result.animations[0].declaredFps, 10U);
    EXPECT_TRUE(result.animations[0].loop);
    EXPECT_EQ(result.animations[1].name, "walk");
    EXPECT_EQ(result.animations[1].firstFrame, 2U);
    EXPECT_FALSE(result.animations[1].loop);

    ASSERT_EQ(result.canonicalImport.asset.regions.size(), 3U);
    EXPECT_EQ(result.canonicalImport.asset.regions[0].id, "idle/frame-0");
    EXPECT_EQ(result.canonicalImport.asset.regions[1].id, "idle/frame-1");
    EXPECT_EQ(result.canonicalImport.asset.regions[2].id, "walk/frame-0");
    EXPECT_EQ(
        result.canonicalImport.asset.regions[0].packedRect,
        result.canonicalImport.asset.regions[1].packedRect);
    EXPECT_EQ(
        result.canonicalImport.asset.regions[0].pivot,
        (SpriteRationalPivot{1, 2, 2}));

    ASSERT_EQ(result.canonicalImport.frames.size(), 3U);
    ASSERT_TRUE(result.canonicalImport.frames[0].durationNanoseconds.has_value());
    ASSERT_TRUE(result.canonicalImport.frames[1].durationNanoseconds.has_value());
    ASSERT_TRUE(result.canonicalImport.frames[2].durationNanoseconds.has_value());
    EXPECT_EQ(*result.canonicalImport.frames[0].durationNanoseconds, 100'000'000LL);
    EXPECT_EQ(*result.canonicalImport.frames[1].durationNanoseconds, 200'000'000LL);
    EXPECT_EQ(*result.canonicalImport.frames[2].durationNanoseconds, 83'000'000LL);
}

TEST(SpriteGeneratorInteropTests, SpriteGenRequiresExplicitPivotTransactionally)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(4U, 4U);
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet-alpha.png",
        .width = 4U,
        .height = 4U,
        .rgba8 = pixels,
    };

    const SpriteGeneratorImportResult result = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::SpriteGenComponentRow,
        SpriteGenManifest(),
        sheet,
        MakeGeneratorOptions(std::nullopt));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, SpriteImportErrorCode::MissingField);
    EXPECT_TRUE(result.animations.empty());
    EXPECT_TRUE(result.canonicalImport.asset.pages.empty());
    EXPECT_TRUE(result.canonicalImport.asset.regions.empty());
}

TEST(SpriteGeneratorInteropTests, SpriteGenRejectsIdentityRowAndDurationFailuresWithoutPartialOutput)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(4U, 4U);
    const SpriteImportDecodedImageView wrongSheet{
        .id = "other.png",
        .width = 4U,
        .height = 4U,
        .rgba8 = pixels,
    };

    const SpriteGeneratorImportResult identity = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::SpriteGenComponentRow,
        SpriteGenManifest(),
        wrongSheet,
        MakeGeneratorOptions());
    ASSERT_FALSE(identity.Succeeded());
    EXPECT_TRUE(identity.canonicalImport.asset.pages.empty());

    std::string duplicateRows = SpriteGenManifest();
    const std::string rowNeedle = "\"row\": 1";
    duplicateRows.replace(
        duplicateRows.find(rowNeedle),
        rowNeedle.size(),
        "\"row\": 0");
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet-alpha.png",
        .width = 4U,
        .height = 4U,
        .rgba8 = pixels,
    };
    const SpriteGeneratorImportResult rows = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::SpriteGenComponentRow,
        duplicateRows,
        sheet,
        MakeGeneratorOptions());
    ASSERT_FALSE(rows.Succeeded());
    EXPECT_TRUE(rows.animations.empty());
    EXPECT_TRUE(rows.canonicalImport.asset.regions.empty());

    std::string zeroDuration = SpriteGenManifest();
    const std::string durationNeedle = "[100, 200]";
    zeroDuration.replace(
        zeroDuration.find(durationNeedle),
        durationNeedle.size(),
        "[0, 200]");
    const SpriteGeneratorImportResult duration = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::SpriteGenComponentRow,
        zeroDuration,
        sheet,
        MakeGeneratorOptions());
    ASSERT_FALSE(duration.Succeeded());
    EXPECT_TRUE(duration.canonicalImport.frames.empty());
    ASSERT_FALSE(duration.diagnostics.empty());
}

TEST(SpriteGeneratorInteropTests, PerfectPixelUsesRowOrderAndConvertsCellLocalTrimToS1Geometry)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(8U, 8U);
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet.png",
        .width = 8U,
        .height = 8U,
        .rgba8 = pixels,
    };

    const SpriteGeneratorImportResult result = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        PerfectPixelManifest(),
        sheet,
        MakeGeneratorOptions(std::nullopt));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.manifestKind, SpriteGeneratorManifestKind::PerfectPixelV2);
    EXPECT_EQ(result.canonicalImport.sourceKind, SpriteImportSourceKind::GenericSheet);

    ASSERT_EQ(result.animations.size(), 2U);
    EXPECT_EQ(result.animations[0].name, "idle");
    EXPECT_EQ(result.animations[0].row, 0U);
    EXPECT_EQ(result.animations[0].firstFrame, 0U);
    EXPECT_EQ(result.animations[0].frameCount, 2U);
    EXPECT_EQ(result.animations[0].declaredFps, 8U);
    EXPECT_TRUE(result.animations[0].loop);
    EXPECT_EQ(result.animations[1].name, "walk");
    EXPECT_EQ(result.animations[1].row, 1U);
    EXPECT_FALSE(result.animations[1].loop);

    ASSERT_EQ(result.canonicalImport.asset.regions.size(), 3U);
    const SpriteRegion& idle0 = result.canonicalImport.asset.regions[0];
    EXPECT_EQ(idle0.id, "idle/frame-0");
    EXPECT_EQ(idle0.sourceSize, (SpritePixelSize{4U, 4U}));
    EXPECT_EQ(idle0.trimOffset, (SpritePixelOffset{1U, 1U}));
    EXPECT_EQ(idle0.trimSize, (SpritePixelSize{2U, 3U}));
    EXPECT_EQ(idle0.packedRect, (SpritePixelRect{1U, 1U, 2U, 3U}));
    EXPECT_EQ(idle0.pivot, (SpriteRationalPivot{2, 4, 1}));

    const SpriteRegion& walk0 = result.canonicalImport.asset.regions[2];
    EXPECT_EQ(walk0.packedRect, (SpritePixelRect{0U, 5U, 3U, 3U}));
    EXPECT_EQ(walk0.trimOffset, (SpritePixelOffset{0U, 1U}));

    ASSERT_TRUE(result.canonicalImport.frames[0].durationNanoseconds.has_value());
    ASSERT_TRUE(result.canonicalImport.frames[2].durationNanoseconds.has_value());
    EXPECT_EQ(*result.canonicalImport.frames[0].durationNanoseconds, 125'000'000LL);
    EXPECT_EQ(*result.canonicalImport.frames[2].durationNanoseconds, 83'000'000LL);
}

TEST(SpriteGeneratorInteropTests, PerfectPixelRejectsSchemaTrimAndDuplicateRowsTransactionally)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(8U, 8U);
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet.png",
        .width = 8U,
        .height = 8U,
        .rgba8 = pixels,
    };

    std::string schema = PerfectPixelManifest();
    const std::string schemaNeedle = "\"perfectpixel.sprite/2\"";
    schema.replace(
        schema.find(schemaNeedle),
        schemaNeedle.size(),
        "\"perfectpixel.sprite/3\"");
    const SpriteGeneratorImportResult schemaResult = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        schema,
        sheet,
        MakeGeneratorOptions(std::nullopt));
    ASSERT_FALSE(schemaResult.Succeeded());
    EXPECT_TRUE(schemaResult.canonicalImport.asset.pages.empty());

    std::string emptyTrim = PerfectPixelManifest();
    const std::string trimNeedle = "{\"x\":1,\"y\":1,\"w\":2,\"h\":3}";
    emptyTrim.replace(
        emptyTrim.find(trimNeedle),
        trimNeedle.size(),
        "{\"x\":1,\"y\":1,\"w\":0,\"h\":3}");
    const SpriteGeneratorImportResult trimResult = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        emptyTrim,
        sheet,
        MakeGeneratorOptions(std::nullopt));
    ASSERT_FALSE(trimResult.Succeeded());
    EXPECT_TRUE(trimResult.canonicalImport.asset.regions.empty());

    std::string duplicateRows = PerfectPixelManifest();
    const std::string rowNeedle = "\"row\": 1";
    duplicateRows.replace(
        duplicateRows.find(rowNeedle),
        rowNeedle.size(),
        "\"row\": 0");
    const SpriteGeneratorImportResult rowsResult = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        duplicateRows,
        sheet,
        MakeGeneratorOptions(std::nullopt));
    ASSERT_FALSE(rowsResult.Succeeded());
    EXPECT_TRUE(rowsResult.animations.empty());
    EXPECT_TRUE(rowsResult.canonicalImport.asset.regions.empty());
}

TEST(SpriteGeneratorInteropTests, PerfectPixelRejectsImageCountAndDurationFailuresTransactionally)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(8U, 8U);
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet.png",
        .width = 8U,
        .height = 8U,
        .rgba8 = pixels,
    };

    std::string wrongImage = PerfectPixelManifest();
    const std::string imageNeedle = "\"sprite-sheet.png\"";
    wrongImage.replace(
        wrongImage.find(imageNeedle),
        imageNeedle.size(),
        "\"other.png\"");
    const SpriteGeneratorImportResult imageResult = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        wrongImage,
        sheet,
        MakeGeneratorOptions(std::nullopt));
    ASSERT_FALSE(imageResult.Succeeded());
    EXPECT_TRUE(imageResult.canonicalImport.asset.pages.empty());

    std::string countMismatch = PerfectPixelManifest();
    const std::string framesNeedle = "\"frames\": 2";
    countMismatch.replace(
        countMismatch.find(framesNeedle),
        framesNeedle.size(),
        "\"frames\": 3");
    const SpriteGeneratorImportResult countResult = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        countMismatch,
        sheet,
        MakeGeneratorOptions(std::nullopt));
    ASSERT_FALSE(countResult.Succeeded());
    EXPECT_TRUE(countResult.canonicalImport.asset.regions.empty());

    std::string zeroDuration = PerfectPixelManifest();
    const std::string durationNeedle = "\"durationMs\": 125";
    zeroDuration.replace(
        zeroDuration.find(durationNeedle),
        durationNeedle.size(),
        "\"durationMs\": 0");
    const SpriteGeneratorImportResult durationResult = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        zeroDuration,
        sheet,
        MakeGeneratorOptions(std::nullopt));
    ASSERT_FALSE(durationResult.Succeeded());
    EXPECT_TRUE(durationResult.canonicalImport.frames.empty());
}

TEST(SpriteGeneratorInteropTests, StructuralJsonIsByteIdenticalForRepeatedRequests)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(8U, 8U);
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet.png",
        .width = 8U,
        .height = 8U,
        .rgba8 = pixels,
    };

    const SpriteGeneratorImportResult first = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        PerfectPixelManifest(),
        sheet,
        MakeGeneratorOptions(std::nullopt));
    const SpriteGeneratorImportResult second = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::PerfectPixelV2,
        PerfectPixelManifest(),
        sheet,
        MakeGeneratorOptions(std::nullopt));

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(first.canonicalImport.asset, second.canonicalImport.asset);
    EXPECT_EQ(first.canonicalImport.frames, second.canonicalImport.frames);
    EXPECT_EQ(first.animations, second.animations);
    EXPECT_EQ(
        SerializeSpriteGeneratorImportResultJson(first),
        SerializeSpriteGeneratorImportResultJson(second));
}

TEST(SpriteGeneratorInteropTests, MalformedJsonAndCanonicalOptionsRemainTransactional)
{
    const std::vector<std::uint8_t> pixels = MakeGeneratorPixels(4U, 4U);
    const SpriteImportDecodedImageView sheet{
        .id = "sprite-sheet-alpha.png",
        .width = 4U,
        .height = 4U,
        .rgba8 = pixels,
    };

    const SpriteGeneratorImportResult malformed = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::SpriteGenComponentRow,
        "{",
        sheet,
        MakeGeneratorOptions());
    ASSERT_FALSE(malformed.Succeeded());
    ASSERT_FALSE(malformed.diagnostics.empty());
    EXPECT_EQ(malformed.diagnostics.front().code, SpriteImportErrorCode::JsonParseError);
    EXPECT_TRUE(malformed.canonicalImport.asset.pages.empty());

    SpriteGeneratorManifestImportOptions badOptions = MakeGeneratorOptions();
    badOptions.canonicalAssetId = "";
    const SpriteGeneratorImportResult canonical = ImportSpriteGeneratorManifestJson(
        SpriteGeneratorManifestKind::SpriteGenComponentRow,
        SpriteGenManifest(),
        sheet,
        badOptions);
    ASSERT_FALSE(canonical.Succeeded());
    ASSERT_FALSE(canonical.canonicalImport.diagnostics.empty());
    EXPECT_EQ(
        canonical.canonicalImport.diagnostics.front().code,
        SpriteImportErrorCode::EmptyAssetId);
    EXPECT_TRUE(canonical.animations.empty());
}
} // namespace trace2d::assets
