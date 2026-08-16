#include <trace2d/agent/SpriteAuthoring.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>

namespace trace2d::agent
{
namespace
{
constexpr std::array<std::uint8_t, 79> TestPng{
    137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2,
    0, 0, 0, 2, 8, 6, 0, 0, 0, 114, 182, 13, 36, 0, 0, 0, 22, 73, 68, 65, 84,
    120, 218, 99, 248, 159, 144, 240, 255, 255, 150, 132, 255, 12, 32, 2, 196, 1, 0,
    105, 6, 11, 161, 208, 129, 231, 21, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130,
};

constexpr std::string_view SpriteReference = "sprites/hero.sprite.toml";
constexpr std::string_view SpriteToml = R"toml(schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/hero.png"
size = [2, 2]
color_space = "srgb"
alpha_mode = "straight"

[[regions]]
id = "hero"
page = "main"
source_size = [2, 2]
trim_offset = [0, 0]
trim_size = [2, 2]
packed_rect = [0, 0, 2, 2]
pivot = [1, 1, 1]
packed_rotation = "none"
)toml";

std::atomic<std::uint64_t> TempProjectSerial{0U};

class TempSpriteAuthoringProject final
{
public:
    TempSpriteAuthoringProject()
        : root_{std::filesystem::temp_directory_path() /
                ("trace2d_sprite_authoring_tests_" +
                 std::to_string(TempProjectSerial.fetch_add(1U, std::memory_order_relaxed)))}
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
        error.clear();
        std::filesystem::create_directories(root_, error);
        EXPECT_FALSE(error);

        WriteBinary("textures/hero.png", TestPng);
        WriteText(SpriteReference, SpriteToml);
    }

    ~TempSpriteAuthoringProject()
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& Root() const noexcept
    {
        return root_;
    }

    [[nodiscard]] std::string ReadSpriteText() const
    {
        std::ifstream input{root_ / std::filesystem::path{SpriteReference}, std::ios::binary};
        EXPECT_TRUE(input);
        return std::string{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
    }

private:
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

    std::filesystem::path root_{};
};

std::size_t CountOccurrences(const std::string_view text, const std::string_view needle)
{
    std::size_t count = 0U;
    std::size_t cursor = 0U;
    while ((cursor = text.find(needle, cursor)) != std::string_view::npos)
    {
        ++count;
        cursor += needle.size();
    }
    return count;
}

bool HasDiagnosticCode(
    const SpriteAuthoringResult& result,
    const SpriteAuthoringErrorCode expected)
{
    for (const SpriteAuthoringDiagnostic& diagnostic : result.diagnostics)
    {
        if (diagnostic.code == expected)
        {
            return true;
        }
    }
    return false;
}

TEST(SpriteAuthoringTests, CommitsTypedMutationAndPreservesUnspecifiedFields)
{
    TempSpriteAuthoringProject project{};

    SpriteMutation mutation{};
    mutation.sampling = assets::SpriteSampling::Linear;
    mutation.region = SpriteRegionMutation{
        .regionId = "hero",
        .sourceSize = assets::SpritePixelSize{3U, 3U},
        .trimOffset = assets::SpritePixelOffset{1U, 1U},
        .pivot = assets::SpriteRationalPivot{2, -2, 2},
    };

    const SpriteAuthoringResult result = MutateSpriteResource(project.Root(), SpriteReference, mutation);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.validationPassed);
    EXPECT_TRUE(result.committed);
    EXPECT_EQ(
        result.changedFields,
        (std::vector<std::string>{
            "sampling",
            "regions.hero.source_size",
            "regions.hero.trim_offset",
            "regions.hero.pivot",
        }));

    assets::SpriteAssetCache cache{project.Root()};
    const assets::SpriteAssetLoadResult reloaded = cache.Load(SpriteReference);
    ASSERT_TRUE(reloaded.Succeeded());
    ASSERT_NE(reloaded.asset, nullptr);
    EXPECT_EQ(reloaded.asset->sampling, assets::SpriteSampling::Linear);
    ASSERT_EQ(reloaded.asset->regions.size(), 1U);
    const assets::SpriteRegion& region = reloaded.asset->regions[0];
    EXPECT_EQ(region.sourceSize, (assets::SpritePixelSize{3U, 3U}));
    EXPECT_EQ(region.trimOffset, (assets::SpritePixelOffset{1U, 1U}));
    EXPECT_EQ(region.trimSize, (assets::SpritePixelSize{2U, 2U}));
    EXPECT_EQ(region.packedRect, (assets::SpritePixelRect{0U, 0U, 2U, 2U}));
    EXPECT_EQ(region.pageId, "main");
    EXPECT_EQ(region.pivot, (assets::SpriteRationalPivot{1, -1, 1}));

    const std::string committedText = project.ReadSpriteText();
    EXPECT_EQ(CountOccurrences(committedText, "source_size ="), 1U);
    EXPECT_EQ(CountOccurrences(committedText, "trim_offset ="), 1U);
    EXPECT_EQ(CountOccurrences(committedText, "pivot ="), 1U);
    const assets::SpriteAssetLoadResult reparsed = assets::ParseSpriteAssetToml(
        committedText,
        SpriteReference);
    EXPECT_TRUE(reparsed.Succeeded());
}

TEST(SpriteAuthoringTests, RejectsUnknownRegionWithoutChangingResource)
{
    TempSpriteAuthoringProject project{};
    const std::string before = project.ReadSpriteText();

    SpriteMutation mutation{};
    mutation.region = SpriteRegionMutation{
        .regionId = "missing",
        .pivot = assets::SpriteRationalPivot{0, 0, 1},
    };

    const SpriteAuthoringResult result = MutateSpriteResource(project.Root(), SpriteReference, mutation);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.committed);
    EXPECT_FALSE(result.validationPassed);
    EXPECT_TRUE(HasDiagnosticCode(result, SpriteAuthoringErrorCode::UnknownRegion));
    EXPECT_EQ(project.ReadSpriteText(), before);
}

TEST(SpriteAuthoringTests, RejectsInvalidGeometryWithoutChangingResource)
{
    TempSpriteAuthoringProject project{};
    const std::string before = project.ReadSpriteText();

    SpriteMutation mutation{};
    mutation.region = SpriteRegionMutation{
        .regionId = "hero",
        .trimSize = assets::SpritePixelSize{3U, 3U},
    };

    const SpriteAuthoringResult result = MutateSpriteResource(project.Root(), SpriteReference, mutation);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.committed);
    EXPECT_FALSE(result.validationPassed);
    EXPECT_TRUE(HasDiagnosticCode(result, SpriteAuthoringErrorCode::ValidationFailed));
    EXPECT_EQ(project.ReadSpriteText(), before);
}

TEST(SpriteAuthoringTests, EquivalentCanonicalMutationDoesNotRewriteResource)
{
    TempSpriteAuthoringProject project{};
    const std::string before = project.ReadSpriteText();

    SpriteMutation mutation{};
    mutation.region = SpriteRegionMutation{
        .regionId = "hero",
        .pivot = assets::SpriteRationalPivot{2, 2, 2},
    };

    const SpriteAuthoringResult result = MutateSpriteResource(project.Root(), SpriteReference, mutation);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.validationPassed);
    EXPECT_FALSE(result.committed);
    EXPECT_TRUE(result.changedFields.empty());
    EXPECT_EQ(project.ReadSpriteText(), before);
}
} // namespace
} // namespace trace2d::agent
