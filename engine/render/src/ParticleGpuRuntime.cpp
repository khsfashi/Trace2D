#include <trace2d/render/ParticleGpuRuntime.hpp>

#include <trace2d/particles/ParticleProgram.hpp>

#include <limits>

namespace trace2d::render
{
GpuParticleRuntimeSupport AnalyzeGpuParticleRuntimeSupport(
    const particles::ParticleProgram& program)
{
    GpuParticleRuntimeSupport support{};
    support.programFingerprint = program.fingerprint;

    const particles::ParticleGpuCompileResult compile = particles::CompileParticleGpuArtifact(program);
    if (!compile.Ok())
    {
        support.error = compile.error == particles::ParticleGpuCompileError::BackendNotSelected
            ? GpuParticleRuntimeError::BackendNotSelected
            : GpuParticleRuntimeError::UnsupportedFeature;
        return support;
    }

    support.artifactFingerprint = compile.artifact.artifactFingerprint;
    support.pipelineVariantId = compile.artifact.pipelineVariantId;
    support.capacity = compile.artifact.capacity;
    support.fieldCount = compile.artifact.fieldCount;
    support.strideBytes = compile.artifact.strideBytes;
    support.particleBufferBytes = compile.artifact.bufferBytes;

    support.unsupportedFeatureMask = program.featureMask &
        particles::ParticleProgramBit(particles::ParticleProgramFeature::SpriteChoice);
    if (support.unsupportedFeatureMask != 0U)
    {
        support.error = GpuParticleRuntimeError::UnsupportedFeature;
        return support;
    }

    bool hasPosition = false;
    bool hasAgeFrames = false;
    for (std::uint32_t index = 0; index < compile.artifact.fieldCount; ++index)
    {
        const particles::ParticleGpuRuntimeField& field = compile.artifact.fields[index];
        if (field.offsetBytes > compile.artifact.strideBytes ||
            field.sizeBytes > compile.artifact.strideBytes - field.offsetBytes)
        {
            support.error = GpuParticleRuntimeError::InvalidArtifact;
            return support;
        }

        hasPosition = hasPosition || field.kind == particles::ParticleGpuRuntimeFieldKind::Position;
        hasAgeFrames = hasAgeFrames || field.kind == particles::ParticleGpuRuntimeFieldKind::AgeFrames;
    }

    const std::uint64_t expectedBytes =
        static_cast<std::uint64_t>(compile.artifact.capacity) * compile.artifact.strideBytes;
    if (!hasPosition || !hasAgeFrames ||
        compile.artifact.capacity == 0U ||
        compile.artifact.strideBytes == 0U ||
        compile.artifact.bufferBytes != expectedBytes ||
        compile.artifact.bufferBytes > std::numeric_limits<std::uint32_t>::max())
    {
        support.error = GpuParticleRuntimeError::InvalidArtifact;
        return support;
    }

    return support;
}
} // namespace trace2d::render
