#pragma once

#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Capture.hpp>
#include <trace2d/render/ParticleGpuRuntime.hpp>
#include <trace2d/render/RenderData.hpp>
#include <trace2d/render/SpriteBatch2D.hpp>
#include <trace2d/render/SpriteOrderMask2D.hpp>
#include <trace2d/render/SpritePixelPerfect2D.hpp>
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

// Production SR7 Sprite draw input. SR2/SR3 geometry/appearance semantics are already resolved;
// SR4 adds finite semantic painter order/group/mask state, SR5 may replace the legacy single quad
// with caller-owned primitive patches, SR6 may attach exact pixel-perfect frame state, and SR7 adds
// only a resolved material/pipeline compatibility identity for batching. The built-in identity is
// the only executable material in SR7; programmable Material2D/Shader2D remains owned by #89.
//
// Primitive patches stay one atomic top-level SR4 item and are consumed only for the duration of
// RenderFrame/CaptureFrame. `pixelPerfectViewport` is optional caller-owned frame-level SR6 state;
// when any submitted Sprite enables it, every Sprite in that frame must provide an equal mapping.
// Texture identity is the canonical R0 generation-safe resource handle; the SDL/GPU object behind
// that identity remains renderer-owned derived state. The SR7 material field is appended after the
// pre-SR7 fields so existing positional aggregate initialization keeps its field meaning while
// omitted material state naturally selects the built-in pipeline.
struct SpritePresentationRenderData final
{
    SpritePresentation2D presentation{};
    TextureHandle texture{InvalidTextureHandle};
    SpriteOrder2D order{};
    SpriteMask2D mask{};
    SpritePresentationGeometryKind geometryKind{SpritePresentationGeometryKind::Quad};
    std::span<const SpritePrimitivePatch2D> primitivePatches{};
    const SpritePixelPerfectViewport2D* pixelPerfectViewport{nullptr};
    SpriteMaterialPipelineIdentity materialPipeline{BuiltInSpriteMaterialPipelineIdentity};
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

    // SR4-SR7 production Sprite presentation metrics. Draw calls are actual GPU batch draws,
    // while Sprite counts distinguish semantic submissions from visible/cull output.
    std::uint64_t spritePresentationDrawCalls{0};
    std::uint64_t spritePresentationSprites{0};
    std::uint64_t spritePresentationVisibleSprites{0};
    std::uint64_t spritePresentationCulledSprites{0};
    std::uint64_t spritePresentationUploadedQuads{0};
    std::uint64_t spritePresentationUploadedVertexBytes{0};
    std::uint64_t spritePresentationCompatibilityRuns{0};

    std::uint64_t spriteSamplerCreations{0};
    std::uint64_t spritePipelineCreations{0};
    // Reusable capacity in six-vertex Sprite quad slots, so one sliced/tiled Sprite may consume
    // multiple slots. SR7 also publishes the matching retained byte capacity below.
    std::uint64_t spriteVertexCapacitySprites{0};
    std::uint64_t spriteVertexCapacityBytes{0};
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

    // Upload derived GPU residency for an already-resolved canonical R0 texture handle. Renderer
    // never allocates texture identity; callers must publish/resolve the canonical texture first.
    // Legacy RGBA8 upload keeps linear UNORM semantics for the pre-SR3 renderer/particle path.
    [[nodiscard]] TextureHandle CreateTextureRgba8(
        TextureHandle texture,
        const Rgba8TextureData& textureData);

    // SR3 texture creation preserves the canonical page color-space meaning through the matching
    // sampled GPU encoding. The canonical handle is retained as residency identity and validated
    // against every SR3+ draw.
    [[nodiscard]] TextureHandle CreateSpriteTextureRgba8(
        TextureHandle texture,
        const Rgba8TextureData& textureData,
        SpriteTextureEncoding encoding);

    // Releases derived GPU residency only. Canonical ResourceRegistry ownership/unload remains an
    // explicit resource-lifecycle operation owned by the caller/project.
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

    // SR4-SR7 production path. SR4 painter order remains authoritative; SR7 conservatively culls,
    // compacts visible vertices in that exact resolved order, and merges only contiguous compatible
    // GPU state. Resource/material identity never authorizes global sorting. SR5 patches stay atomic
    // beneath one top-level Sprite and SR6 viewport/scissor state remains exact frame-level truth.
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