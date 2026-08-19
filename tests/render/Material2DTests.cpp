#include <trace2d/render/Material2D.hpp>
#include <trace2d/render/SpriteBatch2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

namespace trace2d::render
{
namespace
{
[[nodiscard]] assets::ResourceHandleUntyped ShaderIdentity(
    const std::uint32_t generation = 7U) noexcept
{
    // MAT1 consumes the already-frozen generation-safe resource-handle shape. #89's executable
    // resource slice will add the dedicated Shader2D domain without changing this preparation ABI.
    return assets::ResourceHandleUntyped{
        11U,
        generation,
        assets::ResourceTypeDomain::SceneTemplate};
}

constexpr std::array<MaterialParameterDeclaration2D, 3U> Declarations{
    MaterialParameterDeclaration2D{"intensity", MaterialParameterType2D::Float},
    MaterialParameterDeclaration2D{"offset", MaterialParameterType2D::Float2},
    MaterialParameterDeclaration2D{"tint", MaterialParameterType2D::Color},
};

constexpr std::array<MaterialParameterValue2D, 3U> Defaults{
    MaterialFloat2D(0.5F),
    MaterialFloat2D(1.0F, -2.0F),
    MaterialColor2D(1.0F, 0.75F, 0.5F, 1.0F),
};
} // namespace

TEST(Material2DTests, EqualInputsProduceStableSlotLayoutAndValueIdentity)
{
    MaterialParameterLayout2D firstLayout{};
    MaterialParameterLayout2D secondLayout{};
    ASSERT_TRUE(PrepareMaterialParameterLayout2D(ShaderIdentity(), Declarations, firstLayout).Succeeded());
    ASSERT_TRUE(PrepareMaterialParameterLayout2D(ShaderIdentity(), Declarations, secondLayout).Succeeded());

    EXPECT_EQ(firstLayout, secondLayout);
    EXPECT_NE(firstLayout.identity, InvalidMaterial2DIdentity);
    EXPECT_EQ(firstLayout.parameterCount, 3U);
    EXPECT_EQ(firstLayout.entries[0].slot, 0U);
    EXPECT_EQ(firstLayout.entries[1].slot, 1U);
    EXPECT_EQ(firstLayout.entries[2].slot, 2U);

    MaterialParameterBlock2D firstBlock{};
    MaterialParameterBlock2D secondBlock{};
    ASSERT_TRUE(PrepareMaterialParameterBlock2D(firstLayout, Defaults, firstBlock).Succeeded());
    ASSERT_TRUE(PrepareMaterialParameterBlock2D(secondLayout, Defaults, secondBlock).Succeeded());

    EXPECT_EQ(firstBlock, secondBlock);
    EXPECT_NE(firstBlock.valueIdentity, InvalidMaterial2DIdentity);
    EXPECT_EQ(firstBlock.ActivePackedBytes(), 3U * Material2DParameterSlotBytes);
    ASSERT_EQ(firstBlock.ActivePackedFloats().size(), 12U);
    EXPECT_FLOAT_EQ(firstBlock.packed[0], 0.5F);
    EXPECT_FLOAT_EQ(firstBlock.packed[1], 0.0F);
    EXPECT_FLOAT_EQ(firstBlock.packed[4], 1.0F);
    EXPECT_FLOAT_EQ(firstBlock.packed[5], -2.0F);
    EXPECT_FLOAT_EQ(firstBlock.packed[6], 0.0F);
    EXPECT_FLOAT_EQ(firstBlock.packed[8], 1.0F);
    EXPECT_FLOAT_EQ(firstBlock.packed[11], 1.0F);
}

TEST(Material2DTests, ResourceGenerationParticipatesInLayoutIdentity)
{
    MaterialParameterLayout2D first{};
    MaterialParameterLayout2D newerGeneration{};
    ASSERT_TRUE(PrepareMaterialParameterLayout2D(ShaderIdentity(7U), Declarations, first).Succeeded());
    ASSERT_TRUE(PrepareMaterialParameterLayout2D(ShaderIdentity(8U), Declarations, newerGeneration).Succeeded());

    EXPECT_NE(first.identity, newerGeneration.identity);

    MaterialParameterBinding2D oldBinding{};
    ASSERT_TRUE(ResolveMaterialParameterBinding2D(first, "offset", oldBinding).Succeeded());

    MaterialParameterBlock2D newerBlock{};
    ASSERT_TRUE(PrepareMaterialParameterBlock2D(newerGeneration, Defaults, newerBlock).Succeeded());
    const std::array overrides{
        ResolvedMaterialParameterOverride2D{oldBinding, MaterialFloat2D(4.0F, 5.0F)}};
    MaterialParameterBlock2D rejected{};
    const MaterialPrepareStatus2D status =
        ApplyMaterialParameterOverrides2D(newerBlock, overrides, rejected);
    ASSERT_FALSE(status.Succeeded());
    EXPECT_EQ(status.error, MaterialPrepareError2D::BindingLayoutMismatch);
}

TEST(Material2DTests, NameResolutionIsSetupOnlyAndOverrideUsesCompactBinding)
{
    MaterialParameterLayout2D layout{};
    ASSERT_TRUE(PrepareMaterialParameterLayout2D(ShaderIdentity(), Declarations, layout).Succeeded());

    MaterialParameterBinding2D offsetBinding{};
    ASSERT_TRUE(ResolveMaterialParameterBinding2D(layout, "offset", offsetBinding).Succeeded());
    EXPECT_EQ(offsetBinding.slot, 1U);
    EXPECT_EQ(offsetBinding.type, MaterialParameterType2D::Float2);

    MaterialParameterBlock2D defaults{};
    ASSERT_TRUE(PrepareMaterialParameterBlock2D(layout, Defaults, defaults).Succeeded());

    const std::array overrides{
        ResolvedMaterialParameterOverride2D{offsetBinding, MaterialFloat2D(8.0F, 9.0F)}};
    MaterialParameterBlock2D instance{};
    ASSERT_TRUE(ApplyMaterialParameterOverrides2D(defaults, overrides, instance).Succeeded());

    EXPECT_EQ(instance.layoutIdentity, defaults.layoutIdentity);
    EXPECT_NE(instance.valueIdentity, defaults.valueIdentity);
    EXPECT_FLOAT_EQ(instance.packed[4], 8.0F);
    EXPECT_FLOAT_EQ(instance.packed[5], 9.0F);
    EXPECT_FLOAT_EQ(instance.packed[6], 0.0F);
    EXPECT_FLOAT_EQ(instance.packed[7], 0.0F);
}

TEST(Material2DTests, PreparationRejectsMalformedDeclarationsAndValues)
{
    MaterialParameterLayout2D layout{};
    const std::array duplicateDeclarations{
        MaterialParameterDeclaration2D{"amount", MaterialParameterType2D::Float},
        MaterialParameterDeclaration2D{"amount", MaterialParameterType2D::Float2},
    };
    const MaterialPrepareStatus2D duplicate =
        PrepareMaterialParameterLayout2D(ShaderIdentity(), duplicateDeclarations, layout);
    ASSERT_FALSE(duplicate.Succeeded());
    EXPECT_EQ(duplicate.error, MaterialPrepareError2D::DuplicateParameterName);
    EXPECT_EQ(duplicate.parameterIndex, 1U);

    const std::array invalidName{
        MaterialParameterDeclaration2D{"bad-name", MaterialParameterType2D::Float}};
    EXPECT_EQ(
        PrepareMaterialParameterLayout2D(ShaderIdentity(), invalidName, layout).error,
        MaterialPrepareError2D::InvalidParameterName);

    EXPECT_EQ(
        PrepareMaterialParameterLayout2D(
            assets::ResourceHandleUntyped{11U, 0U, assets::ResourceTypeDomain::SceneTemplate},
            Declarations,
            layout)
            .error,
        MaterialPrepareError2D::InvalidShaderIdentity);

    ASSERT_TRUE(PrepareMaterialParameterLayout2D(ShaderIdentity(), Declarations, layout).Succeeded());
    MaterialParameterBinding2D missing{};
    EXPECT_EQ(
        ResolveMaterialParameterBinding2D(layout, "missing", missing).error,
        MaterialPrepareError2D::UnknownParameterName);

    std::array<MaterialParameterValue2D, 3U> badDefaults = Defaults;
    badDefaults[0] = MaterialFloat2D(std::numeric_limits<float>::infinity());
    MaterialParameterBlock2D block{};
    EXPECT_EQ(
        PrepareMaterialParameterBlock2D(layout, badDefaults, block).error,
        MaterialPrepareError2D::NonFiniteParameterValue);

    badDefaults = Defaults;
    badDefaults[1] = MaterialColor2D(1.0F, 1.0F, 1.0F, 1.0F);
    EXPECT_EQ(
        PrepareMaterialParameterBlock2D(layout, badDefaults, block).error,
        MaterialPrepareError2D::ParameterTypeMismatch);
}

TEST(Material2DTests, OverridesRejectDuplicateSlotsAndTypeMismatch)
{
    MaterialParameterLayout2D layout{};
    ASSERT_TRUE(PrepareMaterialParameterLayout2D(ShaderIdentity(), Declarations, layout).Succeeded());
    MaterialParameterBlock2D defaults{};
    ASSERT_TRUE(PrepareMaterialParameterBlock2D(layout, Defaults, defaults).Succeeded());

    MaterialParameterBinding2D intensity{};
    ASSERT_TRUE(ResolveMaterialParameterBinding2D(layout, "intensity", intensity).Succeeded());

    const std::array duplicateOverrides{
        ResolvedMaterialParameterOverride2D{intensity, MaterialFloat2D(1.0F)},
        ResolvedMaterialParameterOverride2D{intensity, MaterialFloat2D(2.0F)},
    };
    MaterialParameterBlock2D output{};
    EXPECT_EQ(
        ApplyMaterialParameterOverrides2D(defaults, duplicateOverrides, output).error,
        MaterialPrepareError2D::DuplicateOverride);

    const std::array wrongType{
        ResolvedMaterialParameterOverride2D{intensity, MaterialFloat2D(1.0F, 2.0F)}};
    EXPECT_EQ(
        ApplyMaterialParameterOverrides2D(defaults, wrongType, output).error,
        MaterialPrepareError2D::ParameterTypeMismatch);
}

TEST(Material2DTests, ParameterBlockIdentityParticipatesInContiguousSpriteBatching)
{
    SpriteBatchCompatibility2D compatibility{};
    compatibility.texture = TextureHandle{3U, 1U, assets::ResourceTypeDomain::Texture};
    compatibility.materialPipeline = 17U;
    compatibility.materialParameters = 1001U;

    SpriteBatchCompatibility2D changedParameters = compatibility;
    changedParameters.materialParameters = 1002U;

    std::array<SpriteBatchItem2D, 3U> items{
        SpriteBatchItem2D{compatibility, 1U, true},
        SpriteBatchItem2D{compatibility, 1U, true},
        SpriteBatchItem2D{changedParameters, 1U, true},
    };

    SpritePresentationBatchMeasurement2D measurement =
        MeasureContiguousSpritePresentationBatches(items);
    EXPECT_EQ(measurement.visibleSprites, 3U);
    EXPECT_EQ(measurement.contiguousRuns, 2U);

    items[2].compatibility = compatibility;
    measurement = MeasureContiguousSpritePresentationBatches(items);
    EXPECT_EQ(measurement.contiguousRuns, 1U);
}
} // namespace trace2d::render
