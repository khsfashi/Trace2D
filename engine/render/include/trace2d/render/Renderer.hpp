#pragma once

#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Capture.hpp>
#include <trace2d/render/ParticleGpuRuntime.hpp>
#include <trace2d/render/RenderData.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace trace2d::particles
{
struct ParticleProgram;
}

namespace trace2d::render
{
struct ClearColor
{
    float red{0.08F};
    float green{0.09F};
    float blue{0.12F};
    float alpha{1.0F};
};

struct RendererConfig
{
    ClearColor clearColor{};
    bool enableDebugValidation{false};
};

struct Rgba8TextureData final
{
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::span<const std::uint8_t> pixels{};
};

// Production SR3 Sprite draw input. SR2/SR3 semantics are already resolved before entering the
// backend; the renderer owns only the live texture handle and derived GPU resources.
struct SpritePresentationRenderData final
{
    SpritePresentation2D presentation{};
    TextureHandle texture{InvalidTextureHandle};
};

struct RenderMetrics
{
    std::uint64_t submittedFrames{0};
    std::uint64_t presentedFrames{0};
    std::uint64_t renderPasses{0};
    std::uint64_t drawCalls{0};
    std::uint64_t submittedSprites{0};
    std::uint64_t submittedGpuParticleInstances{0};
    std::uint64_t gpuParticleDrawCalls{0};
    std::uint64_t culledSprites{0};
    std::uint64_t spritePresentationDrawCalls{0};
    std::uint64_t spritePresentationSprites{0};
    std::uint64_t spriteSamplerCreations{0};
    std::uint64_t spritePipelineCreations{0};
    std::uint64_t spriteVertexCapacitySprites{0};
    std::uint64_t explicitGpuReadbacks{0};
    std::uint64_t explicitGpuFenceWaits{0};
    std::uint32_t lastTargetWidth{0};
    std::uint32_t lastTargetHeight{0};
};

class Renderer final
{
public:
    Renderer(const RendererConfig& config, const platform::Platform& platform);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Legacy RGBA8 upload keeps linear UNORM semantics for the pre-SR3 renderer/particle path.
    [[nodiscard]] TextureHandle CreateTextureRgba8(const Rgba8TextureData& textureData);

    // SR3 texture creation preserves the canonical page color-space meaning through the matching
    // sampled GPU encoding. The created handle is tagged and validated against each SR3 draw.
    [[nodiscard]] TextureHandle CreateSpriteTextureRgba8(
        const Rgba8TextureData& textureData,
        SpriteTextureEncoding encoding);

    void DestroyTexture(TextureHandle texture) noexcept;

    [[nodiscard]] GpuParticleEmitterCreateResult CreateGpuParticleEmitter(
        const particles::ParticleProgram& program,
        std::uint64_t globalSeed,
        std::uint64_t emitterStableId,
        TextureHandle texture);
    void DestroyGpuParticleEmitter(GpuParticleEmitterHandle emitter) noexcept;
    void ResetGpuParticleEmitter(GpuParticleEmitterHandle emitter);
    void PlayGpuParticleEmitter(GpuParticleEmitterHandle emitter);
    void RestartGpuParticleEmitter(GpuParticleEmitterHandle emitter);
    void StopGpuParticleEmitter(GpuParticleEmitterHandle emitter) noexcept;
    [[nodiscard]] bool StepGpuParticleEmitter(const GpuParticleStepData& step);
    [[nodiscard]] bool StepGpuParticleEmitters(std::span<const GpuParticleStepData> steps);
    [[nodiscard]] GpuParticleEmitterMetrics GpuParticleMetrics(
        GpuParticleEmitterHandle emitter) const;

    void RenderFrame();
    void RenderFrame(const OrthographicCamera& camera, const SpriteRenderData& sprite);
    void RenderFrame(const OrthographicCamera& camera, std::span<const SpriteRenderData> sprites);
    void RenderFrame(
        const OrthographicCamera& camera,
        std::span<const SpriteRenderData> sprites,
        std::span<const GpuParticleRenderData> particles);

    // SR3 production path. Input order is preserved exactly; SR4 owns painter-order/group/mask
    // semantics and SR7 owns broad batching. These overloads intentionally do not mix legacy
    // SpriteRenderData or particles into the same call.
    void RenderFrame(
        const OrthographicCamera& camera,
        const SpritePresentationRenderData& sprite);
    void RenderFrame(
        const OrthographicCamera& camera,
        std::span<const SpritePresentationRenderData> sprites);

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const SpriteRenderData& sprite);
    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        std::span<const SpriteRenderData> sprites);
    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        std::span<const SpriteRenderData> sprites,
        std::span<const GpuParticleRenderData> particles);
    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const SpritePresentationRenderData& sprite);
    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        std::span<const SpritePresentationRenderData> sprites);

    [[nodiscard]] const RendererConfig& Config() const noexcept;
    [[nodiscard]] const RenderMetrics& Metrics() const noexcept;
    [[nodiscard]] std::string_view DriverName() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace trace2d::render
