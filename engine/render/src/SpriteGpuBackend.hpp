#pragma once

#include <trace2d/render/Renderer.hpp>

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace trace2d::render::detail
{
struct SpriteGpuBackendMetrics final
{
    std::uint64_t samplerCreations{0U};
    std::uint64_t pipelineCreations{0U};
    std::uint64_t vertexCapacitySprites{0U};
    std::uint64_t maskTargetCreations{0U};
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

    [[nodiscard]] SDL_GPURenderPass* BeginPresentationRenderPass(
        SDL_GPUCommandBuffer* commandBuffer,
        const SDL_GPUColorTargetInfo& colorTarget,
        std::uint32_t targetWidth,
        std::uint32_t targetHeight);

    [[nodiscard]] std::size_t OrderedSourceIndex(std::size_t orderedIndex) const;

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
    void EnsureMaskTarget(std::uint32_t width, std::uint32_t height);
    void Cleanup() noexcept;

    [[nodiscard]] SDL_GPUGraphicsPipeline* ResolvePipeline(
        SpriteBlendCompatibility blend,
        SpriteMaskMode maskMode) const;

    SDL_GPUDevice* device_{nullptr};
    SDL_GPUTextureFormat colorTargetFormat_{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUTextureFormat depthStencilTargetFormat_{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUSampler* nearestSampler_{nullptr};
    SDL_GPUSampler* linearSampler_{nullptr};
    std::array<SDL_GPUGraphicsPipeline*, 4U> unmaskedPipelines_{};
    std::array<SDL_GPUGraphicsPipeline*, 4U> stencilCompatibleUnmaskedPipelines_{};
    std::array<SDL_GPUGraphicsPipeline*, 4U> maskInsidePipelines_{};
    std::array<SDL_GPUGraphicsPipeline*, 4U> maskOutsidePipelines_{};
    SDL_GPUGraphicsPipeline* maskWritePipeline_{nullptr};
    SDL_GPUBuffer* vertexBuffer_{nullptr};
    SDL_GPUTransferBuffer* vertexTransferBuffer_{nullptr};
    std::size_t vertexCapacitySprites_{0U};
    SDL_GPUTexture* maskTarget_{nullptr};
    std::uint32_t maskTargetWidth_{0U};
    std::uint32_t maskTargetHeight_{0U};
    bool maskingRequired_{false};
    std::vector<SpriteOrderMaskEntry2D> orderScratch_{};
    SpriteGpuBackendMetrics metrics_{};
};
} // namespace trace2d::render::detail
