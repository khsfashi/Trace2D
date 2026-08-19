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
    std::uint64_t materialShaderCompilations{0U};
    std::uint64_t materialPipelineCreations{0U};
    std::uint64_t materialPipelineCacheHits{0U};
    // Reusable six-vertex quad-slot capacity. One primitive Sprite may occupy multiple slots.
    std::uint64_t vertexCapacitySprites{0U};
    std::uint64_t vertexCapacityBytes{0U};
    std::uint64_t maskTargetCreations{0U};

    // Exact most-recent UploadPresentations/DrawPresentation plan. Renderer commits the frame
    // counters only after a successful command-buffer submission.
    std::uint64_t lastSubmittedSprites{0U};
    std::uint64_t lastVisibleSprites{0U};
    std::uint64_t lastCulledSprites{0U};
    std::uint64_t lastUploadedQuads{0U};
    std::uint64_t lastUploadedVertexBytes{0U};
    std::uint64_t lastCompatibilityRuns{0U};
    std::uint64_t lastMaterialPipelineSwitches{0U};
    std::uint64_t lastFragmentUniformUploads{0U};
    std::uint64_t lastFragmentUniformUploadBytes{0U};
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

    [[nodiscard]] MaterialGpuPrepareResult2D PrepareMaterial2D(
        const assets::ResourceRegistry& resources,
        assets::ResourceHandle<assets::Material2DResource> material);

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

    // Emits one actual GPU draw only for the first visible Sprite of a precomputed compatible run.
    // Culled/zero-output items and continuation items return false without issuing a primitive.
    bool DrawPresentation(
        SDL_GPUCommandBuffer* commandBuffer,
        SDL_GPURenderPass* renderPass,
        SDL_GPUTexture* texture,
        const SpritePresentationRenderData& presentation,
        std::size_t presentationIndex);

    [[nodiscard]] const SpriteGpuBackendMetrics& Metrics() const noexcept;

private:
    struct CustomPipelineRecord final
    {
        assets::ResourceHandleUntyped shaderIdentity{};
        std::uint64_t layoutIdentity{InvalidMaterial2DIdentity};
        SpriteSamplerCompatibility sampler{SpriteSamplerCompatibility::Nearest};
        SpriteBlendCompatibility blend{SpriteBlendCompatibility::Normal};
        SpriteMaterialPipelineIdentity identity{InvalidSpriteMaterialPipelineIdentity};
        SDL_GPUGraphicsPipeline* unmaskedPipeline{nullptr};
        SDL_GPUGraphicsPipeline* stencilCompatibleUnmaskedPipeline{nullptr};
        SDL_GPUGraphicsPipeline* maskInsidePipeline{nullptr};
        SDL_GPUGraphicsPipeline* maskOutsidePipeline{nullptr};
    };

    void CreateSamplers();
    void CreatePipelines();
    void EnsureVertexCapacity(std::size_t requiredQuadSlots);
    void EnsureMaskTarget(std::uint32_t width, std::uint32_t height);
    void ApplyPixelPerfectRasterState(
        SDL_GPURenderPass* renderPass,
        std::uint32_t targetWidth,
        std::uint32_t targetHeight) const;
    void Cleanup() noexcept;

    [[nodiscard]] const CustomPipelineRecord* ResolveCustomPipeline(
        SpriteMaterialPipelineIdentity identity) const noexcept;
    [[nodiscard]] SDL_GPUGraphicsPipeline* ResolvePipeline(
        SpriteMaterialPipelineIdentity materialPipeline,
        SpriteBlendCompatibility blend,
        SpriteMaskMode maskMode) const;

    SDL_GPUDevice* device_{nullptr};
    SDL_GPUTextureFormat colorTargetFormat_{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUTextureFormat depthStencilTargetFormat_{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUSampler* nearestSampler_{nullptr};
    SDL_GPUSampler* linearSampler_{nullptr};
    SDL_GPUShader* presentationVertexShader_{nullptr};
    std::array<SDL_GPUGraphicsPipeline*, 4U> unmaskedPipelines_{};
    std::array<SDL_GPUGraphicsPipeline*, 4U> stencilCompatibleUnmaskedPipelines_{};
    std::array<SDL_GPUGraphicsPipeline*, 4U> maskInsidePipelines_{};
    std::array<SDL_GPUGraphicsPipeline*, 4U> maskOutsidePipelines_{};
    SDL_GPUGraphicsPipeline* maskWritePipeline_{nullptr};
    std::vector<CustomPipelineRecord> customPipelines_{};
    SDL_GPUBuffer* vertexBuffer_{nullptr};
    SDL_GPUTransferBuffer* vertexTransferBuffer_{nullptr};
    std::size_t vertexCapacitySprites_{0U};
    SDL_GPUTexture* maskTarget_{nullptr};
    std::uint32_t maskTargetWidth_{0U};
    std::uint32_t maskTargetHeight_{0U};
    bool maskingRequired_{false};
    SpritePixelPerfectViewport2D pixelPerfectViewport_{};
    bool pixelPerfectViewportEnabled_{false};

    // All scratch is retained across frames. orderScratch_ is mutated into SR4 painter order;
    // source-index keyed arrays describe the compacted SR7 visible stream and batch-run starts.
    std::vector<SpriteOrderMaskEntry2D> orderScratch_{};
    std::vector<std::size_t> patchOffsetScratch_{};
    std::vector<std::size_t> batchQuadCountScratch_{};
    SpriteGpuBackendMetrics metrics_{};
};
} // namespace trace2d::render::detail
