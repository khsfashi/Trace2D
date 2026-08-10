#pragma once

#include <trace2d/render/ParticleGpuRuntime.hpp>

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <memory>
#include <span>

namespace trace2d::particles
{
struct ParticleProgram;
}

namespace trace2d::render::detail
{
class ParticleGpuBackend final
{
public:
    ParticleGpuBackend(SDL_GPUDevice* device, SDL_GPUTextureFormat colorTargetFormat);
    ~ParticleGpuBackend();

    ParticleGpuBackend(const ParticleGpuBackend&) = delete;
    ParticleGpuBackend& operator=(const ParticleGpuBackend&) = delete;
    ParticleGpuBackend(ParticleGpuBackend&&) = delete;
    ParticleGpuBackend& operator=(ParticleGpuBackend&&) = delete;

    [[nodiscard]] GpuParticleEmitterCreateResult CreateEmitter(
        const particles::ParticleProgram& program,
        std::uint64_t globalSeed,
        std::uint64_t emitterStableId,
        TextureHandle texture);
    void DestroyEmitter(GpuParticleEmitterHandle emitter) noexcept;

    void ResetEmitter(SDL_GPUCommandBuffer* commandBuffer, GpuParticleEmitterHandle emitter);
    void PlayEmitter(GpuParticleEmitterHandle emitter);
    void RestartEmitter(SDL_GPUCommandBuffer* commandBuffer, GpuParticleEmitterHandle emitter);
    void StopEmitter(GpuParticleEmitterHandle emitter) noexcept;
    [[nodiscard]] bool StepEmitter(
        SDL_GPUCommandBuffer* commandBuffer,
        const GpuParticleStepData& step);

    [[nodiscard]] bool IsLive(GpuParticleEmitterHandle emitter) const noexcept;
    [[nodiscard]] TextureHandle Texture(GpuParticleEmitterHandle emitter) const;
    [[nodiscard]] GpuParticleEmitterMetrics Metrics(GpuParticleEmitterHandle emitter) const;

    void DrawEmitter(
        SDL_GPUCommandBuffer* commandBuffer,
        SDL_GPURenderPass* renderPass,
        const OrthographicView& view,
        const GpuParticleRenderData& renderData,
        SDL_GPUBuffer* quadVertexBuffer,
        SDL_GPUSampler* sampler,
        SDL_GPUTexture* texture,
        std::uint64_t& encodedDraws,
        std::uint64_t& encodedInstances);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace trace2d::render::detail
