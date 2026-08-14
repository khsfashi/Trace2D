#include <trace2d/render/RendererWorkload.hpp>

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

namespace
{
[[nodiscard]] constexpr trace2d::render::TextureHandle TestTexture(
    const std::uint32_t slot) noexcept
{
    return trace2d::render::TextureHandle{
        slot,
        1U,
        trace2d::assets::ResourceTypeDomain::Texture,
    };
}

constexpr std::array<trace2d::render::TextureHandle, 2> TestTextures{
    TestTexture(11U),
    TestTexture(22U),
};

TEST(RendererWorkloadTests, CommittedSpecsHaveStableOrderAndDimensions)
{
    const std::span<const trace2d::render::RendererWorkloadSpec> specs =
        trace2d::render::RendererWorkloadSpecs();

    ASSERT_EQ(specs.size(), trace2d::render::RendererWorkloadCount);

    EXPECT_EQ(specs[0].name, "dense_single_texture");
    EXPECT_EQ(specs[0].authoredSpriteCount, 400U);
    EXPECT_EQ(specs[0].textureSlotCount, 1U);

    EXPECT_EQ(specs[1].name, "alternating_two_textures");
    EXPECT_EQ(specs[1].authoredSpriteCount, 400U);
    EXPECT_EQ(specs[1].textureSlotCount, 2U);

    EXPECT_EQ(specs[2].name, "interleaved_culling");
    EXPECT_EQ(specs[2].authoredSpriteCount, 600U);
    EXPECT_EQ(specs[2].textureSlotCount, 2U);

    for (const trace2d::render::RendererWorkloadSpec& spec : specs)
    {
        EXPECT_EQ(spec.targetWidth, 1280U);
        EXPECT_EQ(spec.targetHeight, 720U);
        EXPECT_FLOAT_EQ(spec.camera.center.x, 0.0F);
        EXPECT_FLOAT_EQ(spec.camera.center.y, 0.0F);
        EXPECT_FLOAT_EQ(spec.camera.verticalSize, 24.0F);
    }
}

TEST(RendererWorkloadTests, DenseSingleTextureHasOneVisibleRun)
{
    const trace2d::render::RendererWorkload workload = trace2d::render::BuildRendererWorkload(
        trace2d::render::RendererWorkloadId::DenseSingleTexture,
        TestTextures);

    const trace2d::render::RendererWorkloadStructure structure =
        trace2d::render::MeasureRendererWorkloadStructure(workload);

    EXPECT_EQ(
        structure,
        (trace2d::render::RendererWorkloadStructure{
            400,
            400,
            0,
            1,
        }));
}

TEST(RendererWorkloadTests, AlternatingTexturesPreservePainterOrderAndWorstCaseRuns)
{
    const trace2d::render::RendererWorkload workload = trace2d::render::BuildRendererWorkload(
        trace2d::render::RendererWorkloadId::AlternatingTwoTextures,
        TestTextures);

    const trace2d::render::RendererWorkloadStructure structure =
        trace2d::render::MeasureRendererWorkloadStructure(workload);

    EXPECT_EQ(
        structure,
        (trace2d::render::RendererWorkloadStructure{
            400,
            400,
            0,
            400,
        }));

    ASSERT_EQ(workload.sprites.size(), 400U);
    for (std::size_t index = 0; index < workload.sprites.size(); ++index)
    {
        EXPECT_EQ(workload.sprites[index].stableOrder, index);
        EXPECT_EQ(workload.sprites[index].texture, TestTextures[index % TestTextures.size()]);
    }
}

TEST(RendererWorkloadTests, InterleavedCullingDoesNotSplitVisibleTextureRun)
{
    const trace2d::render::RendererWorkload workload = trace2d::render::BuildRendererWorkload(
        trace2d::render::RendererWorkloadId::InterleavedCulling,
        TestTextures);

    const trace2d::render::RendererWorkloadStructure structure =
        trace2d::render::MeasureRendererWorkloadStructure(workload);

    EXPECT_EQ(
        structure,
        (trace2d::render::RendererWorkloadStructure{
            600,
            400,
            200,
            1,
        }));
}

TEST(RendererWorkloadTests, ParsesOnlyCommittedWorkloadNames)
{
    trace2d::render::RendererWorkloadId id = trace2d::render::RendererWorkloadId::DenseSingleTexture;

    EXPECT_TRUE(trace2d::render::TryParseRendererWorkloadId("alternating_two_textures", id));
    EXPECT_EQ(id, trace2d::render::RendererWorkloadId::AlternatingTwoTextures);

    EXPECT_FALSE(trace2d::render::TryParseRendererWorkloadId("not_a_workload", id));
}

TEST(RendererWorkloadTests, RejectsMissingOrInvalidTextureSlots)
{
    const std::array<trace2d::render::TextureHandle, 1> oneTexture{TestTexture(11U)};
    EXPECT_THROW(
        static_cast<void>(trace2d::render::BuildRendererWorkload(
            trace2d::render::RendererWorkloadId::AlternatingTwoTextures,
            oneTexture)),
        std::invalid_argument);

    const std::array<trace2d::render::TextureHandle, 2> invalidTextures{
        TestTexture(11U),
        trace2d::render::InvalidTextureHandle,
    };
    EXPECT_THROW(
        static_cast<void>(trace2d::render::BuildRendererWorkload(
            trace2d::render::RendererWorkloadId::AlternatingTwoTextures,
            invalidTextures)),
        std::invalid_argument);
}
} // namespace