#include <trace2d/assets/TextureAssets.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>

namespace trace2d::assets
{
namespace
{
constexpr std::array<std::uint8_t, 79> PlayerPng{
    137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2,
    0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 22, 73, 68, 65, 84,
    120, 218, 99, 248, 159, 144, 240, 255, 255, 150, 132, 255, 12, 32, 2, 196, 1, 0,
    105, 6, 11, 161, 208, 129, 231, 21, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
};

constexpr std::array<std::uint8_t, 79> MarkerPng{
    137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2,
    0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 22, 73, 68, 65, 84,
    120, 218, 99, 240, 88, 245, 255, 191, 71, 197, 157, 255, 12, 32, 2, 196, 1, 0,
    94, 213, 11, 23, 17, 142, 165, 170, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
};

class TempAssetProject final
{
public:
    TempAssetProject()
        : root_{std::filesystem::temp_directory_path() / "trace2d_texture_assets_tests"}
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
        error.clear();
        std::filesystem::create_directories(root_, error);
        EXPECT_FALSE(error);
    }

    ~TempAssetProject()
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& Root() const noexcept
    {
        return root_;
    }

    void Write(const std::string_view reference, const std::span<const std::uint8_t> bytes) const
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

private:
    std::filesystem::path root_{};
};

TEST(TextureAssetCacheTests, CanonicalProjectRelativeReferencesReuseDecodedAsset)
{
    TempAssetProject project{};
    project.Write("textures/player.png", PlayerPng);

    TextureAssetCache cache{project.Root()};
    const TextureAssetLoadResult first = cache.Load("textures/player.png");
    const TextureAssetLoadResult second = cache.Load("textures/./player.png");
    const TextureAssetLoadResult windowsSpelling = cache.Load("textures\\player.png");

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    ASSERT_TRUE(windowsSpelling.Succeeded());
    ASSERT_NE(first.asset, nullptr);
    EXPECT_EQ(first.asset.get(), second.asset.get());
    EXPECT_EQ(first.asset.get(), windowsSpelling.asset.get());
    EXPECT_EQ(first.asset->id, "textures/player.png");
    EXPECT_EQ(first.asset->width, 2U);
    EXPECT_EQ(first.asset->height, 2U);
    EXPECT_EQ(first.asset->rgba8.size(), 16U);
    EXPECT_EQ(first.asset->rgba8[0], 255U);
    EXPECT_EQ(first.asset->rgba8[1], 96U);
    EXPECT_EQ(first.asset->rgba8[2], 96U);
    EXPECT_EQ(first.asset->rgba8[3], 255U);

    const TextureAssetCacheMetrics metrics = cache.Metrics();
    EXPECT_EQ(metrics.requests, 3U);
    EXPECT_EQ(metrics.cacheHits, 2U);
    EXPECT_EQ(metrics.cacheMisses, 1U);
    EXPECT_EQ(metrics.successfulImports, 1U);
    EXPECT_EQ(metrics.failedImports, 0U);
    EXPECT_EQ(metrics.cachedAssets, 1U);
}

TEST(TextureAssetCacheTests, ExplicitInvalidationDropsDecodedOwnershipAndReloads)
{
    TempAssetProject project{};
    project.Write("textures/player.png", PlayerPng);

    TextureAssetCache cache{project.Root()};
    const TextureAssetLoadResult first = cache.Load("textures/player.png");
    ASSERT_TRUE(first.Succeeded());

    EXPECT_TRUE(cache.Invalidate("textures/./player.png"));
    EXPECT_EQ(cache.Metrics().cachedAssets, 0U);

    const TextureAssetLoadResult second = cache.Load("textures/player.png");
    ASSERT_TRUE(second.Succeeded());
    EXPECT_NE(first.asset.get(), second.asset.get());
    EXPECT_EQ(cache.Metrics().successfulImports, 2U);

    cache.Clear();
    EXPECT_EQ(cache.Metrics().cachedAssets, 0U);
}

TEST(TextureAssetCacheTests, InvalidAndFailedReferencesReturnStructuredDiagnostics)
{
    TempAssetProject project{};
    project.Write("textures/unsupported.gif", PlayerPng);
    constexpr std::array<std::uint8_t, 4> InvalidPng{1, 2, 3, 4};
    project.Write("textures/broken.png", InvalidPng);

    TextureAssetCache cache{project.Root()};

    const TextureAssetLoadResult traversal = cache.Load("../outside.png");
    ASSERT_TRUE(traversal.diagnostic.has_value());
    EXPECT_EQ(traversal.diagnostic->code, TextureAssetErrorCode::InvalidReference);
    EXPECT_EQ(ToString(traversal.diagnostic->code), "invalid_reference");

    const TextureAssetLoadResult absolute = cache.Load("/outside.png");
    ASSERT_TRUE(absolute.diagnostic.has_value());
    EXPECT_EQ(absolute.diagnostic->code, TextureAssetErrorCode::InvalidReference);

    const TextureAssetLoadResult missing = cache.Load("textures/missing.png");
    ASSERT_TRUE(missing.diagnostic.has_value());
    EXPECT_EQ(missing.diagnostic->code, TextureAssetErrorCode::MissingFile);
    EXPECT_FALSE(missing.diagnostic->resolvedPath.empty());

    const TextureAssetLoadResult unsupported = cache.Load("textures/unsupported.gif");
    ASSERT_TRUE(unsupported.diagnostic.has_value());
    EXPECT_EQ(unsupported.diagnostic->code, TextureAssetErrorCode::UnsupportedFormat);

    const TextureAssetLoadResult broken = cache.Load("textures/broken.png");
    ASSERT_TRUE(broken.diagnostic.has_value());
    EXPECT_EQ(broken.diagnostic->code, TextureAssetErrorCode::DecodeFailure);
    EXPECT_FALSE(broken.diagnostic->message.empty());

    EXPECT_EQ(cache.Metrics().failedImports, 5U);
    EXPECT_EQ(cache.Metrics().cachedAssets, 0U);
}

TEST(TextureAssetCacheTests, FailedLoadsAreNotCachedAndCanRecoverAfterSourceAppears)
{
    TempAssetProject project{};
    TextureAssetCache cache{project.Root()};

    const TextureAssetLoadResult missing = cache.Load("textures/player.png");
    ASSERT_TRUE(missing.diagnostic.has_value());
    EXPECT_EQ(missing.diagnostic->code, TextureAssetErrorCode::MissingFile);

    project.Write("textures/player.png", PlayerPng);
    const TextureAssetLoadResult recovered = cache.Load("textures/player.png");
    ASSERT_TRUE(recovered.Succeeded());

    const TextureAssetCacheMetrics metrics = cache.Metrics();
    EXPECT_EQ(metrics.cacheHits, 0U);
    EXPECT_EQ(metrics.cacheMisses, 2U);
    EXPECT_EQ(metrics.successfulImports, 1U);
    EXPECT_EQ(metrics.failedImports, 1U);
    EXPECT_EQ(metrics.cachedAssets, 1U);
}

TEST(TextureAssetCacheTests, SevenSpriteStyleSampleReusesTwoTextureAssets)
{
    TempAssetProject project{};
    project.Write("textures/player.png", PlayerPng);
    project.Write("textures/marker.png", MarkerPng);

    constexpr std::array<std::string_view, 7> references{
        "textures/player.png",
        "textures/marker.png",
        "textures/marker.png",
        "textures/marker.png",
        "textures/marker.png",
        "textures/marker.png",
        "textures/marker.png",
    };

    TextureAssetCache cache{project.Root()};
    std::array<std::shared_ptr<const TextureAssetData>, references.size()> loaded{};
    for (std::size_t index = 0U; index < references.size(); ++index)
    {
        const TextureAssetLoadResult result = cache.Load(references[index]);
        ASSERT_TRUE(result.Succeeded());
        loaded[index] = result.asset;
    }

    ASSERT_NE(loaded[0], nullptr);
    ASSERT_NE(loaded[1], nullptr);
    for (std::size_t index = 2U; index < loaded.size(); ++index)
    {
        EXPECT_EQ(loaded[index].get(), loaded[1].get());
    }

    const TextureAssetCacheMetrics metrics = cache.Metrics();
    EXPECT_EQ(metrics.requests, 7U);
    EXPECT_EQ(metrics.cacheHits, 5U);
    EXPECT_EQ(metrics.cacheMisses, 2U);
    EXPECT_EQ(metrics.successfulImports, 2U);
    EXPECT_EQ(metrics.cachedAssets, 2U);
}
} // namespace
} // namespace trace2d::assets
