#pragma once

#include <trace2d/render/Renderer.hpp>

#include <SDL3/SDL_gpu.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace trace2d::render::detail
{
struct SpriteGpuBackendMetrics final
{
    std::uint64_t samplerCreations{0U};
    std::uint64_t pipelineCreations{0U};
    std::uint64_t vertexCapacitySprites{0U};
};

class SpriteGpuBackend final
{
public:
    SpriteGpuBackend(SDL_GPUDevice* device, SDL_GPUTextureFormat colorTargetFormat);
    ~SpriteGpuBackend();

    SpriteGpuBackend(const SpriteGpuBackend&) = delete;
    SpriteGpuBackend& operator=(const SpriteGpuBackend&) = delete;
    SpriteGpuBackend(SpriteGpuBackend&&) = delete;
    SpriteGpuBackend& operator=(SpriteGpuBackend&&) = delete;

    [[nodiscard]] static SDL_GPUTextureFormat ResolveTextureFormat(
        SpriteTextureEncoding encoding) noexcept;

    [[nodiscard]] static bool SupportsTextureEncoding(
        SDL_GPUDevice* device,
        SpriteTextureEncoding encoding) noexcept;

    void UploadPresentations(
        SDL_GPUCommandBuffer* commandBuffer,
        const OrthographicView& view,
        std::span<const SpritePresentationRenderData> presentations);

    void DrawPresentation(
        SDL_GPUCommandBuffer* commandBuffer,
        SDL_GPURenderPass* renderPass,
        SDL_GPUTexture* texture,
        const SpritePresentationRenderData& presentation,
        std::size_t presentationIndex);

    [[nodiscard]] const SpriteGpuBackendMetrics& Metrics() const noexcept;

private:
    void CreateSamplers();
    void CreatePipelines();
    void EnsureVertexCapacity(std::size_t requiredSprites);
    void Cleanup() noexcept;

    SDL_GPUDevice* device_{nullptr};
    SDL_GPUTextureFormat colorTargetFormat_{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUSampler* nearestSampler_{nullptr};
    SDL_GPUSampler* linearSampler_{nullptr};
    SDL_GPUGraphicsPipeline* normalPipeline_{nullptr};
    SDL_GPUGraphicsPipeline* additivePipeline_{nullptr};
    SDL_GPUGraphicsPipeline* multiplyPipeline_{nullptr};
    SDL_GPUGraphicsPipeline* screenPipeline_{nullptr};
    SDL_GPUBuffer* vertexBuffer_{nullptr};
    SDL_GPUTransferBuffer* vertexTransferBuffer_{nullptr};
    std::size_t vertexCapacitySprites_{0U};
    SpriteGpuBackendMetrics metrics_{};
};
} // namespace trace2d::render::detail
