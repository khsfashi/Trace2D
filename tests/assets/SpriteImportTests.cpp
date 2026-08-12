#include <trace2d/assets/SpriteImport.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::assets
{
namespace
{
std::vector<std::uint8_t> MakePixels(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint8_t value = 0x7fU)
{
    return std::vector<std::uint8_t>(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U,
        value);
}

SpriteAsepriteSheetImportOptions MakeAsepriteOptions(
    const SpriteAsepriteRotatedFramePolicy rotation =
        SpriteAsepriteRotatedFramePolicy::Reject)
{
    return SpriteAsepriteSheetImportOptions{
        .canonicalAssetId = "sprites/hero.sprite.toml",
        .pageId = "main",
        .textureReference = "textures/hero.png",
        .sampling = SpriteSampling::Nearest,
        .colorSpace = SpriteColorSpace::Srgb,
        .defaultPivot = SpriteRationalPivot{2, 4, 4},
        .rotatedFramePolicy = rotation,
    };
}

SpriteGenericSheetImportOptions MakeGenericOptions()
{
    return SpriteGenericSheetImportOptions{
        .canonicalAssetId = "sprites/generic.sprite.toml",
        .pageId = "sheet",
        .textureReference = "textures/generic.png",
        .sampling = SpriteSampling::Nearest,
        .colorSpace = SpriteColorSpace::Srgb,
    };
}
} // namespace

TEST(SpriteImportTests, AsepriteArrayImportsCanonicalFramesDurationsAndTag)
{
    const std::vector<std::uint8_t> pixels = MakePixels(4U, 2U);
    const SpriteImportDecodedImageView sheet{
        .id = "hero.png",
        .width = 4U,
        .height = 2U,
        .rgba8 = pixels,
    };
    const std::string json = R"json({
  "frames": [
    {
      "filename": "idle_0",
      "frame": {"x":0,"y":0,"w":2,"h":2},
      "rotated": false,
      "trimmed": false,
      "spriteSourceSize": {"x":0,"y":0,"w":2,"h":2},
      "sourceSize": {"w":2,"h":2},
      "duration": 100
    },
    {
      "filename": "idle_1",
      "frame": {"x":2,"y":0,"w":2,"h":2},
      "rotated": false,
      "trimmed": true,
      "spriteSourceSize": {"x":1,"y":0,"w":2,"h":2},
      "sourceSize": {"w":4,"h":2},
      "duration": 125
    }
  ],
  "meta": {
    "image": "hero.png",
    "format": "RGBA8888",
    "size": {"w":4,"h":2},
    "scale": "1",
    "frameTags": [
      {"name":"idle","from":0,"to":1,"direction":"forward"}
    ]
  }
})json";

    const SpriteImportResult result =
        ImportAsepriteSpriteSheetJson(json, sheet, MakeAsepriteOptions());

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.asset.pages.size(), 1U);
    EXPECT_EQ(result.asset.pages.front().id, "main");
    EXPECT_EQ(result.asset.pages.front().size, (SpritePixelSize{4U, 2U}));
    ASSERT_EQ(result.asset.regions.size(), 2U);
    EXPECT_EQ(result.asset.regions[0].id, "idle_0");
    EXPECT_EQ(result.asset.regions[1].id, "idle_1");
    EXPECT_EQ(result.asset.regions[1].sourceSize, (SpritePixelSize{4U, 2U}));
    EXPECT_EQ(result.asset.regions[1].trimOffset, (SpritePixelOffset{1U, 0U}));
    EXPECT_EQ(result.asset.regions[1].pivot, (SpriteRationalPivot{1, 2, 2}));

    ASSERT_EQ(result.frames.size(), 2U);
    ASSERT_TRUE(result.frames[0].durationNanoseconds.has_value());
    ASSERT_TRUE(result.frames[1].durationNanoseconds.has_value());
    EXPECT_EQ(*result.frames[0].durationNanoseconds, 100'000'000LL);
    EXPECT_EQ(*result.frames[1].durationNanoseconds, 125'000'000LL);

    ASSERT_EQ(result.tags.size(), 1U);
    EXPECT_EQ(result.tags.front().name, "idle");
    EXPECT_EQ(result.tags.front().firstFrame, 0U);
    EXPECT_EQ(result.tags.front().lastFrame, 1U);
    EXPECT_EQ(result.tags.front().direction, SpriteImportAnimationDirection::Forward);
}

TEST(SpriteImportTests, AsepriteHashPreservesManifestOrderAndSerializesByteIdentically)
{
    const std::vector<std::uint8_t> pixels = MakePixels(2U, 1U);
    const SpriteImportDecodedImageView sheet{
        .id = "hero.png",
        .width = 2U,
        .height = 1U,
        .rgba8 = pixels,
    };
    const std::string json = R"json({
  "frames": {
    "z_frame": {
      "frame": {"x":0,"y":0,"w":1,"h":1},
      "rotated": false,
      "trimmed": false,
      "spriteSourceSize": {"x":0,"y":0,"w":1,"h":1},
      "sourceSize": {"w":1,"h":1},
      "duration": 16
    },
    "a_frame": {
      "frame": {"x":1,"y":0,"w":1,"h":1},
      "rotated": false,
      "trimmed": false,
      "spriteSourceSize": {"x":0,"y":0,"w":1,"h":1},
      "sourceSize": {"w":1,"h":1},
      "duration": 17
    }
  },
  "meta": {
    "image": "hero.png",
    "format": "RGBA8888",
    "size": {"w":2,"h":1},
    "scale": "1"
  }
})json";

    const SpriteImportResult first =
        ImportAsepriteSpriteSheetJson(json, sheet, MakeAsepriteOptions());
    const SpriteImportResult second =
        ImportAsepriteSpriteSheetJson(json, sheet, MakeAsepriteOptions());

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    ASSERT_EQ(first.asset.regions.size(), 2U);
    EXPECT_EQ(first.asset.regions[0].id, "z_frame");
    EXPECT_EQ(first.asset.regions[1].id, "a_frame");
    EXPECT_EQ(first.asset, second.asset);
    EXPECT_EQ(first.frames, second.frames);
    EXPECT_EQ(
        SerializeSpriteImportResultJson(first),
        SerializeSpriteImportResultJson(second));
}

TEST(SpriteImportTests, AsepriteRotatedFrameRequiresExplicitCw90Policy)
{
    const std::vector<std::uint8_t> pixels = MakePixels(2U, 3U);
    const SpriteImportDecodedImageView sheet{
        .id = "hero.png",
        .width = 2U,
        .height = 3U,
        .rgba8 = pixels,
    };
    const std::string json = R"json({
  "frames": [
    {
      "filename": "rotated",
      "frame": {"x":0,"y":0,"w":2,"h":3},
      "rotated": true,
      "trimmed": false,
      "spriteSourceSize": {"x":0,"y":0,"w":3,"h":2},
      "sourceSize": {"w":3,"h":2},
      "duration": 20
    }
  ],
  "meta": {
    "image": "hero.png",
    "format": "RGBA8888",
    "size": {"w":2,"h":3},
    "scale": "1"
  }
})json";

    const SpriteImportResult rejected =
        ImportAsepriteSpriteSheetJson(json, sheet, MakeAsepriteOptions());

    ASSERT_FALSE(rejected.Succeeded());
    ASSERT_FALSE(rejected.diagnostics.empty());
    EXPECT_EQ(rejected.diagnostics.front().code, SpriteImportErrorCode::UnsupportedRotation);
    EXPECT_TRUE(rejected.asset.pages.empty());
    EXPECT_TRUE(rejected.asset.regions.empty());
    EXPECT_TRUE(rejected.frames.empty());

    const SpriteImportResult accepted = ImportAsepriteSpriteSheetJson(
        json,
        sheet,
        MakeAsepriteOptions(SpriteAsepriteRotatedFramePolicy::InterpretAsCw90));

    ASSERT_TRUE(accepted.Succeeded());
    ASSERT_EQ(accepted.asset.regions.size(), 1U);
    EXPECT_EQ(
        accepted.asset.regions.front().packedRotation,
        SpritePackedRotation::Cw90);
}

TEST(SpriteImportTests, AsepriteTagsMapAllSupportedDirections)
{
    const std::vector<std::uint8_t> pixels = MakePixels(4U, 1U);
    const SpriteImportDecodedImageView sheet{
        .id = "hero.png",
        .width = 4U,
        .height = 1U,
        .rgba8 = pixels,
    };
    const std::string json = R"json({
  "frames": [
    {"filename":"f0","frame":{"x":0,"y":0,"w":1,"h":1},"rotated":false,"trimmed":false,"spriteSourceSize":{"x":0,"y":0,"w":1,"h":1},"sourceSize":{"w":1,"h":1},"duration":1},
    {"filename":"f1","frame":{"x":1,"y":0,"w":1,"h":1},"rotated":false,"trimmed":false,"spriteSourceSize":{"x":0,"y":0,"w":1,"h":1},"sourceSize":{"w":1,"h":1},"duration":1},
    {"filename":"f2","frame":{"x":2,"y":0,"w":1,"h":1},"rotated":false,"trimmed":false,"spriteSourceSize":{"x":0,"y":0,"w":1,"h":1},"sourceSize":{"w":1,"h":1},"duration":1},
    {"filename":"f3","frame":{"x":3,"y":0,"w":1,"h":1},"rotated":false,"trimmed":false,"spriteSourceSize":{"x":0,"y":0,"w":1,"h":1},"sourceSize":{"w":1,"h":1},"duration":1}
  ],
  "meta": {
    "image":"hero.png",
    "format":"RGBA8888",
    "size":{"w":4,"h":1},
    "scale":"1",
    "frameTags":[
      {"name":"forward","from":0,"to":0,"direction":"forward"},
      {"name":"reverse","from":1,"to":1,"direction":"reverse"},
      {"name":"pingpong","from":2,"to":2,"direction":"pingpong"},
      {"name":"pingpong_reverse","from":3,"to":3,"direction":"pingpong_reverse"}
    ]
  }
})json";

    const SpriteImportResult result =
        ImportAsepriteSpriteSheetJson(json, sheet, MakeAsepriteOptions());

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.tags.size(), 4U);
    EXPECT_EQ(result.tags[0].direction, SpriteImportAnimationDirection::Forward);
    EXPECT_EQ(result.tags[1].direction, SpriteImportAnimationDirection::Reverse);
    EXPECT_EQ(result.tags[2].direction, SpriteImportAnimationDirection::PingPong);
    EXPECT_EQ(result.tags[3].direction, SpriteImportAnimationDirection::PingPongReverse);
}

TEST(SpriteImportTests, AsepriteRejectsManifestIdentityScaleAndDurationWithoutPartialOutput)
{
    const std::vector<std::uint8_t> pixels = MakePixels(1U, 1U);
    const SpriteImportDecodedImageView sheet{
        .id = "hero.png",
        .width = 1U,
        .height = 1U,
        .rgba8 = pixels,
    };

    const std::string wrongImage = R"json({
      "frames":[{"filename":"f","frame":{"x":0,"y":0,"w":1,"h":1},"rotated":false,"trimmed":false,"spriteSourceSize":{"x":0,"y":0,"w":1,"h":1},"sourceSize":{"w":1,"h":1},"duration":1}],
      "meta":{"image":"other.png","format":"RGBA8888","size":{"w":1,"h":1},"scale":"1"}
    })json";
    const SpriteImportResult imageResult =
        ImportAsepriteSpriteSheetJson(wrongImage, sheet, MakeAsepriteOptions());
    ASSERT_FALSE(imageResult.Succeeded());
    EXPECT_TRUE(imageResult.asset.pages.empty());

    const std::string scaled = R"json({
      "frames":[{"filename":"f","frame":{"x":0,"y":0,"w":1,"h":1},"rotated":false,"trimmed":false,"spriteSourceSize":{"x":0,"y":0,"w":1,"h":1},"sourceSize":{"w":1,"h":1},"duration":1}],
      "meta":{"image":"hero.png","format":"RGBA8888","size":{"w":1,"h":1},"scale":"2"}
    })json";
    const SpriteImportResult scaleResult =
        ImportAsepriteSpriteSheetJson(scaled, sheet, MakeAsepriteOptions());
    ASSERT_FALSE(scaleResult.Succeeded());
    EXPECT_TRUE(scaleResult.asset.regions.empty());

    const std::string zeroDuration = R"json({
      "frames":[{"filename":"f","frame":{"x":0,"y":0,"w":1,"h":1},"rotated":false,"trimmed":false,"spriteSourceSize":{"x":0,"y":0,"w":1,"h":1},"sourceSize":{"w":1,"h":1},"duration":0}],
      "meta":{"image":"hero.png","format":"RGBA8888","size":{"w":1,"h":1},"scale":"1"}
    })json";
    const SpriteImportResult durationResult =
        ImportAsepriteSpriteSheetJson(zeroDuration, sheet, MakeAsepriteOptions());
    ASSERT_FALSE(durationResult.Succeeded());
    EXPECT_TRUE(durationResult.frames.empty());
    ASSERT_FALSE(durationResult.diagnostics.empty());
    EXPECT_EQ(durationResult.diagnostics.front().code, SpriteImportErrorCode::InvalidDuration);
}

TEST(SpriteImportTests, GenericExplicitRegionsPreserveSourceTrimPivotAndCanonicalize)
{
    const std::vector<std::uint8_t> pixels = MakePixels(4U, 2U);
    const SpriteImportDecodedImageView sheet{
        .id = "generic.png",
        .width = 4U,
        .height = 2U,
        .rgba8 = pixels,
    };
    const std::array regions{
        SpriteGenericRegionView{
            .id = "left",
            .packedRect = SpritePixelRect{0U, 0U, 2U, 2U},
            .sourceSize = SpritePixelSize{4U, 2U},
            .trimOffset = SpritePixelOffset{1U, 0U},
            .trimSize = SpritePixelSize{2U, 2U},
            .pivot = SpriteRationalPivot{2, 4, 4},
            .packedRotation = SpritePackedRotation::None,
        },
        SpriteGenericRegionView{
            .id = "right",
            .packedRect = SpritePixelRect{2U, 0U, 2U, 2U},
        },
    };
    const SpriteGenericSheetImportSpec spec{
        .mode = SpriteGenericSheetMode::ExplicitRegions,
        .explicitRegions = regions,
        .grid = {},
        .gridRegionIds = {},
        .expectedFrameCount = 2U,
        .defaultPivot = SpriteRationalPivot{0, 0, 1},
    };

    const SpriteImportResult result =
        ImportGenericSpriteSheet(sheet, spec, MakeGenericOptions());

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.asset.regions.size(), 2U);
    EXPECT_EQ(result.asset.regions[0].sourceSize, (SpritePixelSize{4U, 2U}));
    EXPECT_EQ(result.asset.regions[0].trimOffset, (SpritePixelOffset{1U, 0U}));
    EXPECT_EQ(result.asset.regions[0].pivot, (SpriteRationalPivot{1, 2, 2}));
    EXPECT_EQ(result.asset.regions[1].sourceSize, (SpritePixelSize{2U, 2U}));
    ASSERT_EQ(result.frames.size(), 2U);
    EXPECT_FALSE(result.frames[0].durationNanoseconds.has_value());
    EXPECT_FALSE(result.frames[1].durationNanoseconds.has_value());
}

TEST(SpriteImportTests, GenericGridUsesExplicitRowOrColumnOrderAndHardCountGate)
{
    const std::vector<std::uint8_t> pixels = MakePixels(4U, 4U);
    const SpriteImportDecodedImageView sheet{
        .id = "generic.png",
        .width = 4U,
        .height = 4U,
        .rgba8 = pixels,
    };
    const std::array<std::string_view, 4U> ids{"a", "b", "c", "d"};

    SpriteGenericSheetImportSpec rowSpec{};
    rowSpec.mode = SpriteGenericSheetMode::UniformGrid;
    rowSpec.grid = SpriteExtractionGridSpec{
        .originX = 1U,
        .originY = 1U,
        .cellWidth = 1U,
        .cellHeight = 1U,
        .columns = 2U,
        .rows = 2U,
        .spacingX = 0U,
        .spacingY = 0U,
        .order = SpriteExtractionOrder::RowMajor,
    };
    rowSpec.gridRegionIds = ids;
    rowSpec.expectedFrameCount = 4U;

    const SpriteImportResult rowResult =
        ImportGenericSpriteSheet(sheet, rowSpec, MakeGenericOptions());

    ASSERT_TRUE(rowResult.Succeeded());
    ASSERT_EQ(rowResult.asset.regions.size(), 4U);
    EXPECT_EQ(rowResult.asset.regions[0].packedRect, (SpritePixelRect{1U, 1U, 1U, 1U}));
    EXPECT_EQ(rowResult.asset.regions[1].packedRect, (SpritePixelRect{2U, 1U, 1U, 1U}));
    EXPECT_EQ(rowResult.asset.regions[2].packedRect, (SpritePixelRect{1U, 2U, 1U, 1U}));
    EXPECT_EQ(rowResult.asset.regions[3].packedRect, (SpritePixelRect{2U, 2U, 1U, 1U}));

    SpriteGenericSheetImportSpec columnSpec = rowSpec;
    columnSpec.grid.order = SpriteExtractionOrder::ColumnMajor;
    const SpriteImportResult columnResult =
        ImportGenericSpriteSheet(sheet, columnSpec, MakeGenericOptions());

    ASSERT_TRUE(columnResult.Succeeded());
    EXPECT_EQ(columnResult.asset.regions[0].packedRect, (SpritePixelRect{1U, 1U, 1U, 1U}));
    EXPECT_EQ(columnResult.asset.regions[1].packedRect, (SpritePixelRect{1U, 2U, 1U, 1U}));
    EXPECT_EQ(columnResult.asset.regions[2].packedRect, (SpritePixelRect{2U, 1U, 1U, 1U}));
    EXPECT_EQ(columnResult.asset.regions[3].packedRect, (SpritePixelRect{2U, 2U, 1U, 1U}));

    columnSpec.expectedFrameCount = 3U;
    const SpriteImportResult mismatch =
        ImportGenericSpriteSheet(sheet, columnSpec, MakeGenericOptions());
    ASSERT_FALSE(mismatch.Succeeded());
    EXPECT_TRUE(mismatch.asset.pages.empty());
    EXPECT_TRUE(mismatch.asset.regions.empty());
}

TEST(SpriteImportTests, LooseFramesBecomeOrderedCanonicalPagesWithoutRepacking)
{
    const std::vector<std::uint8_t> firstPixels = MakePixels(2U, 1U);
    const std::vector<std::uint8_t> secondPixels = MakePixels(1U, 3U);
    const std::array frames{
        SpriteLooseFrameView{
            .pageId = "p0",
            .regionId = "walk_0",
            .textureReference = "textures/walk_0.png",
            .width = 2U,
            .height = 1U,
            .rgba8 = firstPixels,
            .pivot = SpriteRationalPivot{2, 0, 2},
        },
        SpriteLooseFrameView{
            .pageId = "p1",
            .regionId = "walk_1",
            .textureReference = "textures/walk_1.png",
            .width = 1U,
            .height = 3U,
            .rgba8 = secondPixels,
            .pivot = std::nullopt,
        },
    };
    const SpriteLooseFrameImportOptions options{
        .canonicalAssetId = "sprites/walk.sprite.toml",
        .sampling = SpriteSampling::Linear,
        .colorSpace = SpriteColorSpace::Linear,
        .defaultPivot = SpriteRationalPivot{0, 0, 1},
    };

    const SpriteImportResult result = ImportLooseSpriteFrames(frames, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.asset.pages.size(), 2U);
    ASSERT_EQ(result.asset.regions.size(), 2U);
    EXPECT_EQ(result.asset.pages[0].textureReference, "textures/walk_0.png");
    EXPECT_EQ(result.asset.pages[1].textureReference, "textures/walk_1.png");
    EXPECT_EQ(result.asset.regions[0].packedRect, (SpritePixelRect{0U, 0U, 2U, 1U}));
    EXPECT_EQ(result.asset.regions[1].packedRect, (SpritePixelRect{0U, 0U, 1U, 3U}));
    EXPECT_EQ(result.asset.regions[0].pivot, (SpriteRationalPivot{1, 0, 1}));
    EXPECT_EQ(result.asset.sampling, SpriteSampling::Linear);
    EXPECT_EQ(result.asset.pages[0].colorSpace, SpriteColorSpace::Linear);
}

TEST(SpriteImportTests, LooseFrameDuplicatePageOrRegionFailsTransactionally)
{
    const std::vector<std::uint8_t> pixels = MakePixels(1U, 1U);
    const std::array frames{
        SpriteLooseFrameView{
            .pageId = "same",
            .regionId = "r0",
            .textureReference = "textures/a.png",
            .width = 1U,
            .height = 1U,
            .rgba8 = pixels,
        },
        SpriteLooseFrameView{
            .pageId = "same",
            .regionId = "r0",
            .textureReference = "textures/b.png",
            .width = 1U,
            .height = 1U,
            .rgba8 = pixels,
        },
    };
    const SpriteLooseFrameImportOptions options{
        .canonicalAssetId = "sprites/duplicate.sprite.toml",
    };

    const SpriteImportResult result = ImportLooseSpriteFrames(frames, options);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_TRUE(result.asset.pages.empty());
    EXPECT_TRUE(result.asset.regions.empty());
    EXPECT_TRUE(result.frames.empty());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, SpriteImportErrorCode::DuplicatePageId);
}
} // namespace trace2d::assets
