#include <trace2d/assets/SpriteAssets.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>

namespace trace2d::assets
{
namespace
{
constexpr std::array<std::uint8_t, 79> TestPng{
    137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2,
    0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 22, 73, 68, 65, 84,
    120, 218, 99, 248, 159, 144, 240, 255, 255, 150, 132, 255, 12, 32, 2, 196, 1, 0,
    105, 6, 11, 161, 208, 129, 231, 21, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
};

constexpr std::string_view MinimalSpriteToml = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/./player.png"
size = [2, 2]
color_space = "srgb"
alpha_mode = "straight"

[[regions]]
id = "player"
page = "main"
source_size = [2, 2]
trim_offset = [0, 0]
trim_size = [2, 2]
packed_rect = [0, 0, 2, 2]
pivot = [8, -4, 4]
packed_rotation = "none"
)toml";

class TempSpriteProject final
{
public:
    TempSpriteProject()
        : root_{std::filesystem::temp_directory_path() / "trace2d_sprite_assets_tests"}
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
        error.clear();
        std::filesystem::create_directories(root_, error);
        EXPECT_FALSE(error);
    }

    ~TempSpriteProject()
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& Root() const noexcept
    {
        return root_;
    }

    void WriteBinary(const std::string_view reference, const std::span<const std::uint8_t> bytes) const
    {
        const std::filesystem::path path = root_ / std::filesystem::path{reference};
        std::error_code error{};
        std::filesystem::create_directories(path.parent_path(), error);
        ASSERT_FALSE(error);

        std::ofstream output{path, std::ios::binary};
        ASSERT_TRUE(output);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        ASSERT_TRUE(output);
    }

    void WriteText(const std::string_view reference, const std::string_view text) const
    {
        const std::filesystem::path path = root_ / std::filesystem::path{reference};
        std::error_code error{};
        std::filesystem::create_directories(path.parent_path(), error);
        ASSERT_FALSE(error);

        std::ofstream output{path, std::ios::binary};
        ASSERT_TRUE(output);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        ASSERT_TRUE(output);
    }

private:
    std::filesystem::path root_{};
};

bool HasDiagnostic(
    const SpriteAssetLoadResult& result,
    const SpriteAssetErrorCode code,
    const std::string_view path)
{
    for (const SpriteAssetDiagnostic& diagnostic : result.diagnostics)
    {
        if (diagnostic.code == code && diagnostic.path == path)
        {
            return true;
        }
    }
    return false;
}

TEST(SpriteAssetTests, ParsesCanonicalAssetWithoutRendererAndReducesExactPivot)
{
    const SpriteAssetLoadResult result = ParseSpriteAssetToml(
        MinimalSpriteToml,
        "sprites/./player.sprite.toml",
        "memory");

    ASSERT_TRUE(result.Succeeded());
    ASSERT_NE(result.asset, nullptr);
    EXPECT_EQ(result.asset->id, "sprites/player.sprite.toml");
    EXPECT_EQ(result.asset->schemaVersion, 1U);
    EXPECT_EQ(result.asset->sampling, SpriteSampling::Nearest);
    ASSERT_EQ(result.asset->pages.size(), 1U);
    EXPECT_EQ(result.asset->pages[0].textureReference, "textures/player.png");
    EXPECT_EQ(result.asset->pages[0].size, (SpritePixelSize{2U, 2U}));
    ASSERT_EQ(result.asset->regions.size(), 1U);
    EXPECT_EQ(result.asset->regions[0].pivot, (SpriteRationalPivot{2, -1, 1}));

    // The pivot is intentionally outside the 2x2 source and must not be clamped.
    EXPECT_GT(result.asset->regions[0].pivot.xNumerator, 1);
}

TEST(SpriteAssetTests, MultiPageCanonicalSerializationIsStableAndRoundTrips)
{
    constexpr std::string_view text = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "linear"

[[pages]]
id = "a"
texture = "textures/a.png"
size = [2, 2]
color_space = "srgb"
alpha_mode = "straight"

[[pages]]
id = "b"
texture = "textures/b.png"
size = [2, 2]
color_space = "linear"
alpha_mode = "straight"

[[regions]]
id = "normal"
page = "a"
source_size = [2, 2]
trim_offset = [0, 0]
trim_size = [2, 2]
packed_rect = [0, 0, 2, 2]
pivot = [1, 1, 1]
packed_rotation = "none"

[[regions]]
id = "rotated"
page = "b"
source_size = [2, 2]
trim_offset = [0, 0]
trim_size = [1, 2]
packed_rect = [0, 0, 2, 1]
pivot = [0, 0, 7]
packed_rotation = "cw90"
)toml";

    const SpriteAssetLoadResult first = ParseSpriteAssetToml(text, "sprites/multi.sprite.toml");
    ASSERT_TRUE(first.Succeeded());
    const std::string canonical = SaveSpriteAssetToml(*first.asset);
    const SpriteAssetLoadResult second = ParseSpriteAssetToml(canonical, "sprites/multi.sprite.toml");
    ASSERT_TRUE(second.Succeeded());

    EXPECT_EQ(*first.asset, *second.asset);
    EXPECT_EQ(canonical, SaveSpriteAssetToml(*second.asset));
    ASSERT_EQ(second.asset->regions.size(), 2U);
    EXPECT_EQ(second.asset->regions[1].packedRotation, SpritePackedRotation::Cw90);
    EXPECT_EQ(second.asset->regions[1].pivot, (SpriteRationalPivot{0, 0, 1}));
}

TEST(SpriteAssetTests, RejectsReferenceFormatParseAndVersionFailures)
{
    const SpriteAssetLoadResult traversal = ParseSpriteAssetToml(
        MinimalSpriteToml,
        "../player.sprite.toml");
    EXPECT_TRUE(HasDiagnostic(traversal, SpriteAssetErrorCode::InvalidReference, "$reference"));

    const SpriteAssetLoadResult suffix = ParseSpriteAssetToml(
        MinimalSpriteToml,
        "sprites/player.toml");
    EXPECT_TRUE(HasDiagnostic(suffix, SpriteAssetErrorCode::UnsupportedFormat, "$reference"));

    const SpriteAssetLoadResult parse = ParseSpriteAssetToml(
        "schema = [",
        "sprites/player.sprite.toml");
    EXPECT_TRUE(HasDiagnostic(parse, SpriteAssetErrorCode::ParseError, "$"));

    std::string wrongVersion{MinimalSpriteToml};
    const std::size_t versionPosition = wrongVersion.find("version = 1");
    ASSERT_NE(versionPosition, std::string::npos);
    wrongVersion.replace(versionPosition, std::string{"version = 1"}.size(), "version = 2");
    const SpriteAssetLoadResult version = ParseSpriteAssetToml(
        wrongVersion,
        "sprites/player.sprite.toml");
    EXPECT_TRUE(HasDiagnostic(version, SpriteAssetErrorCode::SchemaError, "version"));
}

TEST(SpriteAssetTests, RejectsDuplicateIdsUnknownPageAndUnknownFields)
{
    constexpr std::string_view text = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "nearest"
extra = 1

[[pages]]
id = "main"
texture = "textures/a.png"
size = [2, 2]
color_space = "srgb"
alpha_mode = "straight"

[[pages]]
id = "main"
texture = "textures/b.png"
size = [2, 2]
color_space = "srgb"
alpha_mode = "straight"

[[regions]]
id = "same"
page = "missing"
source_size = [2, 2]
trim_offset = [0, 0]
trim_size = [2, 2]
packed_rect = [0, 0, 2, 2]
pivot = [0, 0, 1]
packed_rotation = "none"

[[regions]]
id = "same"
page = "main"
source_size = [2, 2]
trim_offset = [0, 0]
trim_size = [2, 2]
packed_rect = [0, 0, 2, 2]
pivot = [0, 0, 1]
packed_rotation = "none"
)toml";

    const SpriteAssetLoadResult result = ParseSpriteAssetToml(text, "sprites/bad.sprite.toml");
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "extra"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "pages[1].id"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[1].id"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[0].page"));
}

TEST(SpriteAssetTests, RejectsInvalidGeometryPivotEnumsAndTextureReference)
{
    constexpr std::string_view text = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "cubic"

[[pages]]
id = "main"
texture = "../outside.png"
size = [2, 2]
color_space = "display-p3"
alpha_mode = "premultiplied"

[[regions]]
id = "bad"
page = "main"
source_size = [2, 2]
trim_offset = [1, 1]
trim_size = [2, 2]
packed_rect = [1, 1, 2, 2]
pivot = [1, 1, 0]
packed_rotation = "cw90"
)toml";

    const SpriteAssetLoadResult result = ParseSpriteAssetToml(text, "sprites/bad.sprite.toml");
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "sampling"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::InvalidReference, "pages[0].texture"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "pages[0].color_space"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "pages[0].alpha_mode"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[0].pivot[2]"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[0].trim_offset"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[0].packed_rect"));
}

TEST(SpriteAssetTests, EnforcesCw90SwappedExtentAndPositiveDrawableExtents)
{
    constexpr std::string_view text = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/a.png"
size = [4, 4]
color_space = "srgb"
alpha_mode = "straight"

[[regions]]
id = "zero"
page = "main"
source_size = [0, 2]
trim_offset = [0, 0]
trim_size = [0, 2]
packed_rect = [0, 0, 0, 2]
pivot = [0, 0, 1]
packed_rotation = "none"

[[regions]]
id = "rotated"
page = "main"
source_size = [2, 3]
trim_offset = [0, 0]
trim_size = [2, 3]
packed_rect = [0, 0, 2, 3]
pivot = [0, 0, 1]
packed_rotation = "cw90"
)toml";

    const SpriteAssetLoadResult result = ParseSpriteAssetToml(text, "sprites/bad.sprite.toml");
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[0].source_size"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[0].trim_size"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[0].packed_rect"));
    EXPECT_TRUE(HasDiagnostic(result, SpriteAssetErrorCode::SchemaError, "regions[1].packed_rect"));
}

TEST(SpriteAssetCacheTests, LoadsValidatesAndReusesCanonicalSpriteAndTextureAssets)
{
    TempSpriteProject project{};
    project.WriteBinary("textures/player.png", TestPng);
    project.WriteText("sprites/player.sprite.toml", MinimalSpriteToml);

    SpriteAssetCache cache{project.Root()};
    const SpriteAssetLoadResult first = cache.Load("sprites/player.sprite.toml");
    const SpriteAssetLoadResult second = cache.Load("sprites/./player.sprite.toml");
    const SpriteAssetLoadResult windowsSpelling = cache.Load("sprites\\player.sprite.toml");

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    ASSERT_TRUE(windowsSpelling.Succeeded());
    EXPECT_EQ(first.asset.get(), second.asset.get());
    EXPECT_EQ(first.asset.get(), windowsSpelling.asset.get());

    const SpriteAssetCacheMetrics metrics = cache.Metrics();
    EXPECT_EQ(metrics.requests, 3U);
    EXPECT_EQ(metrics.cacheHits, 2U);
    EXPECT_EQ(metrics.cacheMisses, 1U);
    EXPECT_EQ(metrics.successfulImports, 1U);
    EXPECT_EQ(metrics.failedImports, 0U);
    EXPECT_EQ(metrics.cachedAssets, 1U);

    const TextureAssetCacheMetrics textureMetrics = cache.TextureMetrics();
    EXPECT_EQ(textureMetrics.successfulImports, 1U);
    EXPECT_EQ(textureMetrics.cachedAssets, 1U);
}

TEST(SpriteAssetCacheTests, RejectsDecodedTextureDimensionMismatchAndRecoversAfterInvalidation)
{
    TempSpriteProject project{};
    project.WriteBinary("textures/player.png", TestPng);

    std::string mismatched{MinimalSpriteToml};
    const std::size_t sizePosition = mismatched.find("size = [2, 2]");
    ASSERT_NE(sizePosition, std::string::npos);
    mismatched.replace(sizePosition, std::string{"size = [2, 2]"}.size(), "size = [3, 2]");
    project.WriteText("sprites/player.sprite.toml", mismatched);

    SpriteAssetCache cache{project.Root()};
    const SpriteAssetLoadResult mismatch = cache.Load("sprites/player.sprite.toml");
    EXPECT_TRUE(HasDiagnostic(
        mismatch,
        SpriteAssetErrorCode::TextureValidationError,
        "pages[0].size"));

    project.WriteText("sprites/player.sprite.toml", MinimalSpriteToml);
    const SpriteAssetLoadResult recovered = cache.Load("sprites/player.sprite.toml");
    ASSERT_TRUE(recovered.Succeeded());
    EXPECT_EQ(cache.Metrics().successfulImports, 1U);
    EXPECT_EQ(cache.Metrics().failedImports, 1U);

    EXPECT_TRUE(cache.Invalidate("sprites/./player.sprite.toml"));
    EXPECT_EQ(cache.Metrics().cachedAssets, 0U);
    EXPECT_EQ(cache.TextureMetrics().cachedAssets, 0U);
}
} // namespace
} // namespace trace2d::assets
