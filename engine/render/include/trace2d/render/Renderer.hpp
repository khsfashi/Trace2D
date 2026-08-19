#pragma once

#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Capture.hpp>
#include <trace2d/render/MaterialGpu2D.hpp>
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

#if defined(TRACE2D_RENDER_TEST_TEXTURE_FACTORY)
namespace detail
{
// Existing GPU tests exercise rendering semantics rather than resource identity. Keep their
// synthetic identities isolated to the test target so production Renderer never regains a second
// texture-handle allocator. R0 lifecycle coverage uses a real ResourceRegistry separately.
[[nodiscard]] inline TextureHandle AllocateSyntheticTestTextureHandle() noexcept
{
    static std::uint32_t nextSlot = 0U;
    return TextureHandle{
        nextSlot++,
        1U,
        assets::ResourceTypeDomain::Texture,
    };
}
} // namespace detail
#endif

enum class SpritePresentationGeometryKind : std::uint8_t
{
    Quad = 0,
    PrimitivePatches = 1,
};

// Production SR7+ Sprite draw input. Geometry/appearance/order semantics are already resolved and
// MAT3 optionally appends one setup-prepared custom pipeline plus a caller-owned fixed MAT1 block.
// The block is consumed only for the duration of RenderFrame/CaptureFrame; steady rendering never
// resolves parameter names, reflects source or allocates a per-Sprite property container.
//
// Primitive patches stay one atomic top-level SR4 item and are consumed only for the duration of
// RenderFrame/CaptureFrame. `pixelPerfectViewport` is optional caller-owned frame-level SR6 state;
// when any submitted Sprite enables it, every Sprite in that frame must provide an equal mapping.
// Texture identity is the canonical R0 generation-safe resource handle; the SDL/GPU object behind
// that identity remains renderer-owned derived state. New MAT3 fields are appended so existing
// positional aggregate initialization keeps its field meaning and omitted state selects built-in.
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
    const MaterialParameterBlock2D* materialParameters{nullptr};
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
    // MAT3 setup/cache counters are cumulative renderer-lifetime evidence. Frame counters below are
    // committed only after a successful command submission, matching the existing metrics policy.
    std::uint64_t materialShaderCompilations{0};
    std::uint64_t materialPipelineCreations{0};
    std::uint64_t materialPipelineCacheHits{0};
    std::uint64_t materialPipelineSwitches{0};
    std::uint64_t fragmentUniformUploads{0};
    std::uint64_t fragmentUniformUploadBytes{0};
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

#if defined(TRACE2D_RENDER_TEST_TEXTURE_FACTORY)
    // Test-only source compatibility for pre-R0 GPU rendering tests. Production code never sees
    // these overloads, so runtime texture identity remains exclusively ResourceRegistry-owned.
    [[nodiscard]] TextureHandle CreateTextureRgba8(const Rgba8TextureData& textureData)
    {
        return CreateTextureRgba8(detail::AllocateSyntheticTestTextureHandle(), textureData);
    }

    [[nodiscard]] TextureHandle CreateSpriteTextureRgba8(
        const Rgba8TextureData& textureData,
        const SpriteTextureEncoding encoding)
    {
        return CreateSpriteTextureRgba8(
            detail::AllocateSyntheticTestTextureHandle(),
            textureData,
            encoding);
    }
#endif

    // Releases derived GPU residency only. Canonical ResourceRegistry ownership/unload remains an
    // explicit resource-lifecycle operation owned by the caller/project.
    void DestroyTexture(TextureHandle texture) noexcept;

    // MAT3 explicit setup boundary. Resolves generation-safe canonical Material2D/Shader2D state,
    // prepares MAT1 layout/defaults, compiles/reflects the fragment shader and reuses an immutable
    // renderer-owned pipeline bundle. This is never called implicitly from ordinary frame drawing.
    [[nodiscard]] MaterialGpuPrepareResult2D PrepareMaterial2D(
        const assets::ResourceRegistry& resources,
        assets::ResourceHandle<assets::Material2DResource> material);

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

    // SR4+ production path. Painter order remains authoritative; compatible contiguous runs alone
    // merge. MAT3 custom materials enter through an already-prepared integer pipeline identity and
    // a fixed parameter block, never through source/name/reflection work here.
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
