#include <trace2d/particles/ParticleProgram.hpp>
#include <trace2d/render/ParticleGpuRuntime.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace
{
using trace2d::particles::CompileParticleGpuArtifact;
using trace2d::particles::CompileParticleProgram;
using trace2d::particles::HasParticleProgramFeature;
using trace2d::particles::ParticleEffectAsset;
using trace2d::particles::ParticleEffectBackend;
using trace2d::particles::ParticleProgramFeature;
using trace2d::render::AnalyzeGpuParticleRuntimeSupport;
using trace2d::render::GpuParticleRuntimeError;

[[nodiscard]] ParticleEffectAsset MakeEffect(const ParticleEffectBackend backend)
{
    ParticleEffectAsset effect{};
    effect.id = "effects/gpu_test.trace2d.particle.toml";
    effect.semanticId = "gpu_test";
    effect.backend = backend;
    effect.lifecycle.durationFrames = 12U;
    effect.lifecycle.loop = false;
    effect.lifecycle.playOnLoad = true;
    effect.definition.maxParticles = 64U;
    effect.definition.periodicStartFrame = 0U;
    effect.definition.periodicCount = 4U;
    effect.definition.periodicEveryFrames = 2U;
    effect.definition.lifetimeFrames = {4U, 4U};
    effect.definition.initialSize = {1.0F, 1.0F};
    effect.definition.endSizeMultiplier = 0.5F;
    effect.definition.spriteChoiceCount = 1U;
    effect.spriteReferences = {"textures/particles/spark.png"};
    return effect;
}

TEST(ParticleGpuRuntimeTests, SupportedGpuProgramUsesExactCompilerArtifact)
{
    const auto program = CompileParticleProgram(MakeEffect(ParticleEffectBackend::Gpu));
    const auto artifact = CompileParticleGpuArtifact(program);
    ASSERT_TRUE(artifact.Ok());

    const auto support = AnalyzeGpuParticleRuntimeSupport(program);
    ASSERT_TRUE(support.Ok());
    EXPECT_EQ(support.error, GpuParticleRuntimeError::None);
    EXPECT_EQ(support.unsupportedFeatureMask, 0U);
    EXPECT_EQ(support.programFingerprint, artifact.artifact.programFingerprint);
    EXPECT_EQ(support.artifactFingerprint, artifact.artifact.artifactFingerprint);
    EXPECT_EQ(support.pipelineVariantId, artifact.artifact.pipelineVariantId);
    EXPECT_EQ(support.capacity, artifact.artifact.capacity);
    EXPECT_EQ(support.fieldCount, artifact.artifact.fieldCount);
    EXPECT_EQ(support.strideBytes, artifact.artifact.strideBytes);
    EXPECT_EQ(support.particleBufferBytes, artifact.artifact.bufferBytes);
}

TEST(ParticleGpuRuntimeTests, CpuBackendNeverQualifiesForGpuRuntime)
{
    const auto program = CompileParticleProgram(MakeEffect(ParticleEffectBackend::Cpu));

    const auto support = AnalyzeGpuParticleRuntimeSupport(program);
    EXPECT_FALSE(support.Ok());
    EXPECT_EQ(support.error, GpuParticleRuntimeError::BackendNotSelected);
    EXPECT_EQ(support.programFingerprint, program.fingerprint);
}

TEST(ParticleGpuRuntimeTests, VariableSpriteChoiceFailsExplicitlyWithoutFallback)
{
    ParticleEffectAsset effect = MakeEffect(ParticleEffectBackend::Gpu);
    effect.definition.spriteChoiceCount = 2U;
    effect.spriteReferences.push_back("textures/particles/spark_b.png");
    const auto program = CompileParticleProgram(effect);
    ASSERT_TRUE(HasParticleProgramFeature(program.featureMask, ParticleProgramFeature::SpriteChoice));

    const auto compilerArtifact = CompileParticleGpuArtifact(program);
    ASSERT_TRUE(compilerArtifact.Ok());

    const auto support = AnalyzeGpuParticleRuntimeSupport(program);
    EXPECT_FALSE(support.Ok());
    EXPECT_EQ(support.error, GpuParticleRuntimeError::UnsupportedFeature);
    EXPECT_NE(
        support.unsupportedFeatureMask &
            trace2d::particles::ParticleProgramBit(ParticleProgramFeature::SpriteChoice),
        0U);
}

TEST(ParticleGpuRuntimeTests, InvalidMinimizedLayoutIsRejectedBeforeGpuCreation)
{
    auto program = CompileParticleProgram(MakeEffect(ParticleEffectBackend::Gpu));
    ASSERT_GT(program.gpuStrideBytes, 0U);
    ASSERT_GT(program.gpuBufferBytes, 0U);

    program.gpuStrideBytes = 0U;

    const auto support = AnalyzeGpuParticleRuntimeSupport(program);
    EXPECT_FALSE(support.Ok());
    EXPECT_EQ(support.error, GpuParticleRuntimeError::InvalidArtifact);
}
} // namespace
