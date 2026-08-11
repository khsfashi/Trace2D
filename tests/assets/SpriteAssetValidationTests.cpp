#include <trace2d/assets/SpriteAssets.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string_view>

namespace trace2d::assets
{
namespace
{
bool ContainsDiagnostic(
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

constexpr std::string_view ValidSpriteToml = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/player.png"
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
pivot = [1, 1, 1]
packed_rotation = "none"
)toml";

class ValidationProject final
{
public:
    ValidationProject()
        : root_{std::filesystem::temp_directory_path() / "trace2d_sprite_validation_tests"}
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
        error.clear();
        std::filesystem::create_directories(root_, error);
        EXPECT_FALSE(error);
    }

    ~ValidationProject()
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& Root() const noexcept
    {
        return root_;
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

    void CreateDirectory(const std::string_view reference) const
    {
        std::error_code error{};
        std::filesystem::create_directories(root_ / std::filesystem::path{reference}, error);
        ASSERT_FALSE(error);
    }

private:
    std::filesystem::path root_{};
};

TEST(SpriteAssetValidationTests, RejectsAbsoluteReferenceWrongSchemaAndIntegerOverflow)
{
    const SpriteAssetLoadResult absolute = ParseSpriteAssetToml(
        ValidSpriteToml,
        "/sprites/player.sprite.toml");
    EXPECT_TRUE(ContainsDiagnostic(
        absolute,
        SpriteAssetErrorCode::InvalidReference,
        "$reference"));

    constexpr std::string_view wrongSchema = R"toml(
schema = "other.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/player.png"
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
pivot = [1, 1, 1]
packed_rotation = "none"
)toml";
    const SpriteAssetLoadResult schema = ParseSpriteAssetToml(
        wrongSchema,
        "sprites/player.sprite.toml");
    EXPECT_TRUE(ContainsDiagnostic(
        schema,
        SpriteAssetErrorCode::SchemaError,
        "schema"));

    constexpr std::string_view overflowPivot = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/player.png"
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
pivot = [9223372036854775808, 0, 1]
packed_rotation = "none"
)toml";
    const SpriteAssetLoadResult overflow = ParseSpriteAssetToml(
        overflowPivot,
        "sprites/player.sprite.toml");
    EXPECT_TRUE(ContainsDiagnostic(
        overflow,
        SpriteAssetErrorCode::ParseError,
        "$"));
}

TEST(SpriteAssetValidationTests, RejectsZeroPageNoneExtentMismatchAndUnknownRotation)
{
    constexpr std::string_view invalid = R"toml(
schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/player.png"
size = [0, 4]
color_space = "srgb"
alpha_mode = "straight"

[[regions]]
id = "bad_extent"
page = "main"
source_size = [4, 4]
trim_offset = [0, 0]
trim_size = [2, 2]
packed_rect = [0, 0, 1, 2]
pivot = [0, 0, 1]
packed_rotation = "none"

[[regions]]
id = "bad_rotation"
page = "main"
source_size = [4, 4]
trim_offset = [0, 0]
trim_size = [2, 2]
packed_rect = [0, 0, 2, 2]
pivot = [0, 0, 1]
packed_rotation = "ccw90"
)toml";

    const SpriteAssetLoadResult result = ParseSpriteAssetToml(
        invalid,
        "sprites/invalid.sprite.toml");
    EXPECT_TRUE(ContainsDiagnostic(
        result,
        SpriteAssetErrorCode::SchemaError,
        "pages[0].size"));
    EXPECT_TRUE(ContainsDiagnostic(
        result,
        SpriteAssetErrorCode::SchemaError,
        "regions[0].packed_rect"));
    EXPECT_TRUE(ContainsDiagnostic(
        result,
        SpriteAssetErrorCode::SchemaError,
        "regions[1].packed_rotation"));
}

TEST(SpriteAssetValidationTests, CacheReportsMissingUnreadableAndMissingTextureSources)
{
    ValidationProject project{};
    SpriteAssetCache cache{project.Root()};

    const SpriteAssetLoadResult missing = cache.Load("sprites/missing.sprite.toml");
    EXPECT_TRUE(ContainsDiagnostic(
        missing,
        SpriteAssetErrorCode::MissingFile,
        "$reference"));

    project.CreateDirectory("sprites/directory.sprite.toml");
    const SpriteAssetLoadResult directory = cache.Load("sprites/directory.sprite.toml");
    EXPECT_TRUE(ContainsDiagnostic(
        directory,
        SpriteAssetErrorCode::ReadFailure,
        "$reference"));

    project.WriteText("sprites/player.sprite.toml", ValidSpriteToml);
    const SpriteAssetLoadResult texture = cache.Load("sprites/player.sprite.toml");
    EXPECT_TRUE(ContainsDiagnostic(
        texture,
        SpriteAssetErrorCode::TextureValidationError,
        "pages[0].texture"));

    const SpriteAssetCacheMetrics metrics = cache.Metrics();
    EXPECT_EQ(metrics.requests, 3U);
    EXPECT_EQ(metrics.cacheHits, 0U);
    EXPECT_EQ(metrics.cacheMisses, 1U);
    EXPECT_EQ(metrics.successfulImports, 0U);
    EXPECT_EQ(metrics.failedImports, 3U);
    EXPECT_EQ(metrics.cachedAssets, 0U);
}
} // namespace
} // namespace trace2d::assets
