#pragma once

#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Capture.hpp>
#include <trace2d/render/ParticleGpuRuntime.hpp>
#include <trace2d/render/RenderData.hpp>
#include <trace2d/render/SpriteOrderMask2D.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>
#include <trace2d/render/SpritePrimitive2D.hpp>

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

enum class SpritePresentationGeometryKind : std::uint8_t
{
    Quad = 0,
    PrimitivePatches = 1,
};

// Production SR5 Sprite draw input. SR2/SR3 geometry/appearance semantics are already resolved;
// SR4 adds finite semantic painter order/group/mask state and SR5 may replace the legacy single
// quad with caller-owned, already-derived primitive patches. Primitive patches stay one atomic
// top-level SR4 item and are consumed only for the duration of RenderFrame/CaptureFrame.
// Texture handles and all GPU resources remain derived renderer state.
struct SpritePresentationRenderData final
{
    SpritePresentation2D presentation{};
    TextureHandle texture{InvalidTextureHandle};
    SpriteOrder2D order{};
    SpriteMask2D mask{};
    SpritePresentationGeometryKind geometryKind{SpritePresentationGeometryKind::Quad};
    std::span<const SpritePrimitivePatch2D> primitivePatches{};
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
    // Kept for compatibility with SR3/SR4 metrics. In SR5 this is the reusable capacity in
    // six-vertex Sprite quad slots, so one sliced/tiled Sprite may consume multiple slots.
    std::uint64_t spriteVertexCapacitySprites{0};
    std::uint64_t spriteMaskTargetCreations{0};
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
    // sampled GPU encoding. The created handle is tagged and validated against each SR3+ draw.
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

    // SR4/SR5 production path. The renderer resolves painter order from each top-level input,
    // submits that order without resource sorting, then emits all SR5 patches of one Sprite as one
    // contiguous triangle-list draw. Default geometry/order/mask preserve pre-SR5 single-quad
    // behavior. SR7 owns broad cross-Sprite batching/culling.
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
