#include <trace2d/render/Renderer.hpp>

#include "ParticleGpuBackend.hpp"
#include "SpriteGpuBackend.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace trace2d::render
{
namespace
{
constexpr SDL_GPUShaderFormat SupportedShaderFormats = static_cast<SDL_GPUShaderFormat>(
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL);
constexpr std::uint64_t Rgba8BytesPerPixel = 4U;

enum class CaptureSourceChannelOrder : std::uint8_t
{
    Rgba,
    Bgra,
};

struct SpriteVertex final
{
    float localX;
    float localY;
    float u;
    float v;
};

struct TextureResource final
{
    SDL_GPUTexture* texture{nullptr};
    std::uint32_t generation{0U};
    SpriteTextureEncoding spriteEncoding{SpriteTextureEncoding::Linear};
    bool hasSpriteEncoding{false};
};

constexpr std::array<SpriteVertex, 6> SpriteVertices{
    SpriteVertex{-1.0F, -1.0F, 0.0F, 1.0F},
    SpriteVertex{1.0F, -1.0F, 1.0F, 1.0F},
    SpriteVertex{1.0F, 1.0F, 1.0F, 0.0F},
    SpriteVertex{-1.0F, -1.0F, 0.0F, 1.0F},
    SpriteVertex{1.0F, 1.0F, 1.0F, 0.0F},
    SpriteVertex{-1.0F, 1.0F, 0.0F, 0.0F},
};

constexpr char SpriteVertexShaderHlsl[] = R"(
struct VertexInput
{
    float2 localPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 spriteTransform : TEXCOORD2;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = float4(
        input.spriteTransform.xy + input.localPosition * input.spriteTransform.zw,
        0.0,
        1.0);
    output.uv = input.uv;
    return output;
}
)";

constexpr char SpriteFragmentShaderHlsl[] = R"(
Texture2D SpriteTexture : register(t0, space2);
SamplerState SpriteSampler : register(s0, space2);

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(FragmentInput input) : SV_Target0
{
    return SpriteTexture.Sample(SpriteSampler, input.uv);
}
)";

[[nodiscard]] std::runtime_error MakeSdlError(const char* const context)
{
    return std::runtime_error{std::string{context} + ": " + SDL_GetError()};
}

[[nodiscard]] bool IsValidTextureHandle(const TextureHandle texture) noexcept
{
    return texture.generation != 0U &&
        texture.domain == assets::ResourceTypeDomain::Texture;
}

void ApplyPresentationRasterViewport(
    SDL_GPURenderPass* const renderPass,
    const OrthographicCamera& camera,
    const std::uint32_t targetWidth,
    const std::uint32_t targetHeight,
    const bool narrowerRasterContractAlreadyApplied)
{
    if (renderPass == nullptr || !camera.rasterViewport.enabled ||
        narrowerRasterContractAlreadyApplied)
    {
        return;
    }

    const PresentationRasterViewport2D& raster = camera.rasterViewport;
    if (raster.targetWidth != targetWidth || raster.targetHeight != targetHeight ||
        !std::isfinite(raster.origin.x) || !std::isfinite(raster.origin.y) ||
        !std::isfinite(raster.size.x) || !std::isfinite(raster.size.y) ||
        raster.size.x <= 0.0F || raster.size.y <= 0.0F)
    {
        throw std::invalid_argument{
            "Camera2D presentation raster viewport is invalid or stale for the acquired target."};
    }

    SDL_GPUViewport viewport{};
    viewport.x = raster.origin.x;
    viewport.y = raster.origin.y;
    viewport.w = raster.size.x;
    viewport.h = raster.size.y;
    viewport.min_depth = 0.0F;
    viewport.max_depth = 1.0F;
    SDL_SetGPUViewport(renderPass, &viewport);
}

class ShaderCrossProcessLifetime final
{
public:
    ShaderCrossProcessLifetime()
    {
        if (!SDL_ShaderCross_Init())
        {
            throw MakeSdlError("SDL_shadercross initialization failed");
        }
    }

    ~ShaderCrossProcessLifetime()
    {
        SDL_ShaderCross_Quit();
    }

    ShaderCrossProcessLifetime(const ShaderCrossProcessLifetime&) = delete;
    ShaderCrossProcessLifetime& operator=(const ShaderCrossProcessLifetime&) = delete;
};

void EnsureShaderCrossProcessLifetime()
{
    // The pinned SDL_shadercross contract permits one process-wide Init/Quit pair.
    // Delayed construction waits until Platform has initialized SDL. A Renderer that
    // triggers this static is destroyed before the static owner during normal C++
    // teardown, so all derived GPU shader/pipeline state is released before Quit.
    static const ShaderCrossProcessLifetime lifetime{};
    (void)lifetime;
}

[[nodiscard]] SDL_GPUShader* CompileHlslShader(
    SDL_GPUDevice* const device,
    const char* const source,
    const SDL_ShaderCross_ShaderStage shaderCrossStage)
{
    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.source = source;
    hlslInfo.entrypoint = "main";
    hlslInfo.shader_stage = shaderCrossStage;

    std::size_t spirvSize = 0U;
    void* const spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
    if (spirv == nullptr)
    {
        throw MakeSdlError("HLSL to SPIR-V compilation failed");
    }

    SDL_ShaderCross_GraphicsShaderMetadata* const metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8*>(spirv), spirvSize, 0);
    if (metadata == nullptr)
    {
        SDL_free(spirv);
        throw MakeSdlError("SPIR-V shader reflection failed");
    }

    SDL_ShaderCross_SPIRV_Info spirvInfo{};
    spirvInfo.bytecode = static_cast<const Uint8*>(spirv);
    spirvInfo.bytecode_size = spirvSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.shader_stage = shaderCrossStage;

    SDL_GPUShader* const shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device, &spirvInfo, &metadata->resource_info, 0);

    SDL_free(metadata);
    SDL_free(spirv);

    if (shader == nullptr)
    {
        throw MakeSdlError("SDL GPU shader compilation failed");
    }
    return shader;
}

[[nodiscard]] std::size_t ExpectedRgba8ByteCount(const Rgba8TextureData& textureData)
{
    if (textureData.width == 0U || textureData.height == 0U)
    {
        throw std::invalid_argument{"RGBA8 texture dimensions must be non-zero."};
    }

    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(textureData.width) *
        static_cast<std::uint64_t>(textureData.height);
    const std::uint64_t byteCount = pixelCount * Rgba8BytesPerPixel;
    if (byteCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        byteCount > static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()))
    {
        throw std::length_error{"RGBA8 texture upload exceeds SDL GPU transfer-buffer size limits."};
    }
    return static_cast<std::size_t>(byteCount);
}

[[nodiscard]] CaptureSourceChannelOrder ResolveCaptureSourceChannelOrder(
    const SDL_GPUTextureFormat format)
{
    switch (format)
    {
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
        return CaptureSourceChannelOrder::Rgba;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
        return CaptureSourceChannelOrder::Bgra;
    default:
        throw std::runtime_error{"Capture requires an 8-bit RGBA/BGRA SDR render-target format."};
    }
}

[[nodiscard]] CapturedFrame NormalizeCapturedFrame(
    const void* const mappedReadback,
    const CaptureReadbackLayout& layout,
    const SDL_GPUTextureFormat sourceFormat,
    const std::uint64_t simulationFrame,
    const Uint32 width,
    const Uint32 height)
{
    if (mappedReadback == nullptr)
    {
        throw std::invalid_argument{"Mapped capture readback pointer must not be null."};
    }

    const CaptureSourceChannelOrder channelOrder = ResolveCaptureSourceChannelOrder(sourceFormat);
    const std::size_t packedRowBytes =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(Rgba8BytesPerPixel);
    CapturedFrame frame{};
    frame.simulationFrame = simulationFrame;
    frame.width = width;
    frame.height = height;
    frame.rgba8Pixels.resize(packedRowBytes * static_cast<std::size_t>(height));

    const auto* const sourceBytes = static_cast<const std::uint8_t*>(mappedReadback);
    for (Uint32 y = 0U; y < height; ++y)
    {
        const std::uint8_t* const sourceRow =
            sourceBytes + static_cast<std::size_t>(y) * static_cast<std::size_t>(layout.rowPitchBytes);
        std::uint8_t* const destinationRow =
            frame.rgba8Pixels.data() + static_cast<std::size_t>(y) * packedRowBytes;

        if (channelOrder == CaptureSourceChannelOrder::Rgba)
        {
            std::memcpy(destinationRow, sourceRow, packedRowBytes);
            continue;
        }

        for (Uint32 x = 0U; x < width; ++x)
        {
            const std::size_t offset =
                static_cast<std::size_t>(x) * static_cast<std::size_t>(Rgba8BytesPerPixel);
            destinationRow[offset] = sourceRow[offset + 2U];
            destinationRow[offset + 1U] = sourceRow[offset + 1U];
            destinationRow[offset + 2U] = sourceRow[offset];
            destinationRow[offset + 3U] = sourceRow[offset + 3U];
        }
    }
    return frame;
}

[[nodiscard]] bool ParticleOrderLess(
    const GpuParticleRenderData& left,
    const GpuParticleRenderData& right) noexcept
{
    if (left.layer != right.layer)
    {
        return left.layer < right.layer;
    }
    return left.stableOrder < right.stableOrder;
}

[[nodiscard]] bool ParticleBeforeSprite(
    const GpuParticleRenderData& particle,
    const SpriteRenderData& sprite) noexcept
{
    if (particle.layer != sprite.layer)
    {
        return particle.layer < sprite.layer;
    }
    return particle.stableOrder < sprite.stableOrder;
}

[[nodiscard]] bool IsFiniteUnit(const float value) noexcept
{
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool IsFiniteNormalizedBounds(const SpriteSampleBounds& bounds) noexcept
{
    return std::isfinite(bounds.minimum.x) && std::isfinite(bounds.minimum.y) &&
        std::isfinite(bounds.maximum.x) && std::isfinite(bounds.maximum.y) &&
        bounds.minimum.x >= 0.0F && bounds.minimum.y >= 0.0F &&
        bounds.maximum.x <= 1.0F && bounds.maximum.y <= 1.0F &&
        bounds.minimum.x <= bounds.maximum.x && bounds.minimum.y <= bounds.maximum.y;
}

[[nodiscard]] bool IsFiniteDrawVertex(const SpriteDrawVertex& vertex) noexcept
{
    return std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) &&
        std::isfinite(vertex.uv.x) && std::isfinite(vertex.uv.y) &&
        vertex.uv.x >= 0.0F && vertex.uv.x <= 1.0F &&
        vertex.uv.y >= 0.0F && vertex.uv.y <= 1.0F;
}
} // namespace

class Renderer::Impl final
{
public:
    Impl(const RendererConfig& config, const platform::Platform& platform)
        : config_{config}
    {
        if (!platform.HasWindow() || platform.WindowIdValue() == platform::InvalidWindowId)
        {
            throw std::invalid_argument{"Trace2D renderer requires a windowed Platform instance."};
        }

        window_ = SDL_GetWindowFromID(platform.WindowIdValue());
        if (window_ == nullptr)
        {
            throw MakeSdlError("SDL window lookup failed");
        }

        device_ = SDL_CreateGPUDevice(SupportedShaderFormats, config_.enableDebugValidation, nullptr);
        if (device_ == nullptr)
        {
            throw MakeSdlError("SDL GPU device creation failed");
        }

        if (!SDL_ClaimWindowForGPUDevice(device_, window_))
        {
            const std::string error{SDL_GetError()};
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
            throw std::runtime_error{"SDL GPU window claim failed: " + error};
        }
        windowClaimed_ = true;

        try
        {
            colorTargetFormat_ = SDL_GetGPUSwapchainTextureFormat(device_, window_);
            if (colorTargetFormat_ == SDL_GPU_TEXTUREFORMAT_INVALID)
            {
                throw MakeSdlError("SDL GPU swapchain texture-format query failed");
            }

            EnsureShaderCrossProcessLifetime();

            CreatePersistentSpriteResources();
            spriteGpuBackend_ =
                std::make_unique<detail::SpriteGpuBackend>(device_, colorTargetFormat_);
            SyncSpriteGpuMetrics();
            particleGpuBackend_ =
                std::make_unique<detail::ParticleGpuBackend>(device_, colorTargetFormat_);

            const char* const driverName = SDL_GetGPUDeviceDriver(device_);
            if (driverName != nullptr)
            {
                driverName_ = driverName;
            }
        }
        catch (...)
        {
            Cleanup();
            throw;
        }
    }

    ~Impl()
    {
        Cleanup();
    }

    [[nodiscard]] TextureHandle CreateTextureRgba8(
        const TextureHandle texture,
        const Rgba8TextureData& textureData)
    {
        return CreateTextureRgba8Internal(
            texture,
            textureData,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            false,
            SpriteTextureEncoding::Linear);
    }

    [[nodiscard]] TextureHandle CreateSpriteTextureRgba8(
        const TextureHandle texture,
        const Rgba8TextureData& textureData,
        const SpriteTextureEncoding encoding)
    {
        const SDL_GPUTextureFormat format = detail::SpriteGpuBackend::ResolveTextureFormat(encoding);
        if (format == SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            throw std::invalid_argument{"Unsupported Sprite SR3 texture encoding."};
        }
        if (!detail::SpriteGpuBackend::SupportsTextureEncoding(device_, encoding))
        {
            throw std::runtime_error{
                "SDL GPU backend does not support the required Sprite SR3 sampled texture format."};
        }
        return CreateTextureRgba8Internal(texture, textureData, format, true, encoding);
    }

    void DestroyTexture(const TextureHandle texture) noexcept
    {
        if (!IsValidTextureHandle(texture))
        {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(texture.slot);
        if (index >= textures_.size())
        {
            return;
        }

        TextureResource& resource = textures_[index];
        if (resource.texture == nullptr || resource.generation != texture.generation)
        {
            return;
        }

        SDL_ReleaseGPUTexture(device_, resource.texture);
        resource.texture = nullptr;
        resource.generation = 0U;
        resource.spriteEncoding = SpriteTextureEncoding::Linear;
        resource.hasSpriteEncoding = false;
    }

    [[nodiscard]] MaterialGpuPrepareResult2D PrepareMaterial2D(
        const assets::ResourceRegistry& resources,
        const assets::ResourceHandle<assets::Material2DResource> material)
    {
        MaterialGpuPrepareResult2D result = spriteGpuBackend_->PrepareMaterial2D(resources, material);
        SyncSpriteGpuMetrics();
        return result;
    }

    [[nodiscard]] GpuParticleEmitterCreateResult CreateGpuParticleEmitter(
        const particles::ParticleProgram& program,
        const std::uint64_t globalSeed,
        const std::uint64_t emitterStableId,
        const TextureHandle texture)
    {
        static_cast<void>(ResolveTexture(texture));
        return particleGpuBackend_->CreateEmitter(program, globalSeed, emitterStableId, texture);
    }

    void DestroyGpuParticleEmitter(const GpuParticleEmitterHandle emitter) noexcept
    {
        particleGpuBackend_->DestroyEmitter(emitter);
    }

    void ResetGpuParticleEmitter(const GpuParticleEmitterHandle emitter)
    {
        SubmitParticleControl(
            emitter,
            [this](SDL_GPUCommandBuffer* commandBuffer, const GpuParticleEmitterHandle handle)
            {
                particleGpuBackend_->ResetEmitter(commandBuffer, handle);
            },
            "SDL GPU particle reset submission failed");
    }

    void PlayGpuParticleEmitter(const GpuParticleEmitterHandle emitter)
    {
        SubmitParticleControl(
            emitter,
            [this](SDL_GPUCommandBuffer* commandBuffer, const GpuParticleEmitterHandle handle)
            {
                particleGpuBackend_->PlayEmitter(commandBuffer, handle);
            },
            "SDL GPU particle play submission failed");
    }

    void RestartGpuParticleEmitter(const GpuParticleEmitterHandle emitter)
    {
        SubmitParticleControl(
            emitter,
            [this](SDL_GPUCommandBuffer* commandBuffer, const GpuParticleEmitterHandle handle)
            {
                particleGpuBackend_->RestartEmitter(commandBuffer, handle);
            },
            "SDL GPU particle restart submission failed");
    }

    void StopGpuParticleEmitter(const GpuParticleEmitterHandle emitter) noexcept
    {
        particleGpuBackend_->StopEmitter(emitter);
    }

    [[nodiscard]] bool StepGpuParticleEmitters(const std::span<const GpuParticleStepData> steps)
    {
        if (steps.empty())
        {
            return true;
        }

        SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(device_);
        if (commandBuffer == nullptr)
        {
            throw MakeSdlError("SDL GPU particle step command-buffer acquisition failed");
        }

        bool result = true;
        try
        {
            for (const GpuParticleStepData& step : steps)
            {
                result = particleGpuBackend_->StepEmitter(commandBuffer, step) && result;
            }
        }
        catch (...)
        {
            SDL_CancelGPUCommandBuffer(commandBuffer);
            throw;
        }

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            throw MakeSdlError("SDL GPU particle step submission failed");
        }
        return result;
    }

    [[nodiscard]] GpuParticleEmitterMetrics GpuParticleMetrics(
        const GpuParticleEmitterHandle emitter) const
    {
        return particleGpuBackend_->Metrics(emitter);
    }

    void RenderFrame()
    {
        RenderFrameInternal(nullptr, {}, {}, {}, nullptr, nullptr);
    }

    void RenderFrame(const OrthographicCamera& camera, const SpriteRenderData& sprite)
    {
        RenderFrame(camera, std::span<const SpriteRenderData>{&sprite, 1U});
    }

    void RenderFrame(
        const OrthographicCamera& camera,
        const std::span<const SpriteRenderData> sprites)
    {
        RenderFrameInternal(&camera, sprites, {}, {}, nullptr, nullptr);
    }

    void RenderFrame(
        const OrthographicCamera& camera,
        const std::span<const SpriteRenderData> sprites,
        const std::span<const GpuParticleRenderData> particles)
    {
        RenderFrameInternal(&camera, sprites, particles, {}, nullptr, nullptr);
    }

    void RenderFrame(
        const OrthographicCamera& camera,
        const SpritePresentationRenderData& sprite)
    {
        RenderFrame(camera, std::span<const SpritePresentationRenderData>{&sprite, 1U});
    }

    void RenderFrame(
        const OrthographicCamera& camera,
        const std::span<const SpritePresentationRenderData> sprites)
    {
        RenderFrameInternal(&camera, {}, {}, sprites, nullptr, nullptr);
    }

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const SpriteRenderData& sprite)
    {
        return CaptureFrame(request, camera, std::span<const SpriteRenderData>{&sprite, 1U});
    }

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const std::span<const SpriteRenderData> sprites)
    {
        return CaptureFrame(request, camera, sprites, {});
    }

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const std::span<const SpriteRenderData> sprites,
        const std::span<const GpuParticleRenderData> particles)
    {
        ValidateCaptureRequest(request);
        CapturedFrame frame{};
        RenderFrameInternal(&camera, sprites, particles, {}, &request, &frame);
        WriteCaptureArtifact(request, frame);
        return frame;
    }

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const SpritePresentationRenderData& sprite)
    {
        return CaptureFrame(
            request,
            camera,
            std::span<const SpritePresentationRenderData>{&sprite, 1U});
    }

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const std::span<const SpritePresentationRenderData> sprites)
    {
        ValidateCaptureRequest(request);
        CapturedFrame frame{};
        RenderFrameInternal(&camera, {}, {}, sprites, &request, &frame);
        WriteCaptureArtifact(request, frame);
        return frame;
    }

    [[nodiscard]] const RendererConfig& Config() const noexcept
    {
        return config_;
    }

    [[nodiscard]] const RenderMetrics& Metrics() const noexcept
    {
        return metrics_;
    }

    [[nodiscard]] std::string_view DriverName() const noexcept
    {
        return driverName_;
    }

private:
    [[nodiscard]] TextureHandle CreateTextureRgba8Internal(
        const TextureHandle textureHandle,
        const Rgba8TextureData& textureData,
        const SDL_GPUTextureFormat format,
        const bool hasSpriteEncoding,
        const SpriteTextureEncoding spriteEncoding)
    {
        if (!IsValidTextureHandle(textureHandle))
        {
            throw std::invalid_argument{
                "Renderer texture residency requires a valid canonical R0 texture handle."};
        }

        const std::size_t byteCount = ExpectedRgba8ByteCount(textureData);
        if (textureData.pixels.size() != byteCount)
        {
            throw std::invalid_argument{"RGBA8 texture byte count must equal width * height * 4."};
        }

        const std::size_t index = static_cast<std::size_t>(textureHandle.slot);
        if (index >= textures_.size())
        {
            textures_.resize(index + 1U);
        }

        TextureResource& residency = textures_[index];
        if (residency.texture != nullptr)
        {
            if (residency.generation == textureHandle.generation)
            {
                throw std::invalid_argument{"Renderer texture handle already has live GPU residency."};
            }
            throw std::invalid_argument{
                "Renderer texture slot still owns a different generation; destroy old GPU residency before reuse."};
        }

        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = format;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        textureInfo.width = textureData.width;
        textureInfo.height = textureData.height;
        textureInfo.layer_count_or_depth = 1U;
        textureInfo.num_levels = 1U;
        textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* const texture = SDL_CreateGPUTexture(device_, &textureInfo);
        if (texture == nullptr)
        {
            throw MakeSdlError("SDL GPU texture creation failed");
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(byteCount);
        SDL_GPUTransferBuffer* const transferBuffer = SDL_CreateGPUTransferBuffer(device_, &transferInfo);
        if (transferBuffer == nullptr)
        {
            SDL_ReleaseGPUTexture(device_, texture);
            throw MakeSdlError("SDL GPU texture transfer-buffer creation failed");
        }

        void* const mapped = SDL_MapGPUTransferBuffer(device_, transferBuffer, false);
        if (mapped == nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            SDL_ReleaseGPUTexture(device_, texture);
            throw MakeSdlError("SDL GPU texture transfer-buffer mapping failed");
        }
        std::memcpy(mapped, textureData.pixels.data(), byteCount);
        SDL_UnmapGPUTransferBuffer(device_, transferBuffer);

        SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(device_);
        if (commandBuffer == nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            SDL_ReleaseGPUTexture(device_, texture);
            throw MakeSdlError("SDL GPU texture upload command-buffer acquisition failed");
        }

        SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        if (copyPass == nullptr)
        {
            const std::string error{SDL_GetError()};
            SDL_CancelGPUCommandBuffer(commandBuffer);
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            SDL_ReleaseGPUTexture(device_, texture);
            throw std::runtime_error{"SDL GPU texture copy-pass creation failed: " + error};
        }

        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = transferBuffer;
        SDL_GPUTextureRegion destination{};
        destination.texture = texture;
        destination.w = textureData.width;
        destination.h = textureData.height;
        destination.d = 1U;
        SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
        SDL_EndGPUCopyPass(copyPass);

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            const std::string error{SDL_GetError()};
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            SDL_ReleaseGPUTexture(device_, texture);
            throw std::runtime_error{"SDL GPU texture upload submission failed: " + error};
        }

        SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
        residency.texture = texture;
        residency.generation = textureHandle.generation;
        residency.spriteEncoding = spriteEncoding;
        residency.hasSpriteEncoding = hasSpriteEncoding;
        return textureHandle;
    }

    static void ValidateCaptureRequest(const CaptureRequest& request)
    {
        if (request.artifactPath.empty())
        {
            throw std::invalid_argument{"Capture artifact path must not be empty."};
        }
        switch (request.format)
        {
        case CaptureImageFormat::Bmp:
            break;
        default:
            throw std::invalid_argument{"Unsupported capture image format."};
        }
    }

    template <typename Encode>
    void SubmitParticleControl(
        const GpuParticleEmitterHandle emitter,
        Encode&& encode,
        const char* const submitError)
    {
        SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(device_);
        if (commandBuffer == nullptr)
        {
            throw MakeSdlError("SDL GPU particle control command-buffer acquisition failed");
        }

        try
        {
            encode(commandBuffer, emitter);
        }
        catch (...)
        {
            SDL_CancelGPUCommandBuffer(commandBuffer);
            throw;
        }

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            throw MakeSdlError(submitError);
        }
    }

    void CreatePersistentSpriteResources()
    {
        CreateSpriteVertexBuffer();
        CreateSpriteSampler();
        CreateSpritePipeline();
    }

    void CreateSpriteVertexBuffer()
    {
        SDL_GPUBufferCreateInfo bufferInfo{};
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufferInfo.size = static_cast<Uint32>(sizeof(SpriteVertices));
        spriteVertexBuffer_ = SDL_CreateGPUBuffer(device_, &bufferInfo);
        if (spriteVertexBuffer_ == nullptr)
        {
            throw MakeSdlError("SDL GPU sprite vertex-buffer creation failed");
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(sizeof(SpriteVertices));
        SDL_GPUTransferBuffer* const transferBuffer = SDL_CreateGPUTransferBuffer(device_, &transferInfo);
        if (transferBuffer == nullptr)
        {
            throw MakeSdlError("SDL GPU sprite vertex transfer-buffer creation failed");
        }

        void* const mapped = SDL_MapGPUTransferBuffer(device_, transferBuffer, false);
        if (mapped == nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            throw MakeSdlError("SDL GPU sprite vertex transfer-buffer mapping failed");
        }
        std::memcpy(mapped, SpriteVertices.data(), sizeof(SpriteVertices));
        SDL_UnmapGPUTransferBuffer(device_, transferBuffer);

        SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(device_);
        if (commandBuffer == nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            throw MakeSdlError("SDL GPU sprite vertex upload command-buffer acquisition failed");
        }

        SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        if (copyPass == nullptr)
        {
            const std::string error{SDL_GetError()};
            SDL_CancelGPUCommandBuffer(commandBuffer);
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            throw std::runtime_error{"SDL GPU sprite vertex copy-pass creation failed: " + error};
        }

        SDL_GPUTransferBufferLocation source{};
        source.transfer_buffer = transferBuffer;
        SDL_GPUBufferRegion destination{};
        destination.buffer = spriteVertexBuffer_;
        destination.size = static_cast<Uint32>(sizeof(SpriteVertices));
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
        SDL_EndGPUCopyPass(copyPass);

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
        {
            const std::string error{SDL_GetError()};
            SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
            throw std::runtime_error{"SDL GPU sprite vertex upload submission failed: " + error};
        }
        SDL_ReleaseGPUTransferBuffer(device_, transferBuffer);
    }

    void CreateSpriteSampler()
    {
        SDL_GPUSamplerCreateInfo samplerInfo{};
        samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        spriteSampler_ = SDL_CreateGPUSampler(device_, &samplerInfo);
        if (spriteSampler_ == nullptr)
        {
            throw MakeSdlError("SDL GPU sprite sampler creation failed");
        }
    }

    void CreateSpritePipeline()
    {
        SDL_GPUShader* vertexShader = nullptr;
        SDL_GPUShader* fragmentShader = nullptr;

        try
        {
            vertexShader = CompileHlslShader(
                device_, SpriteVertexShaderHlsl, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
            fragmentShader = CompileHlslShader(
                device_, SpriteFragmentShaderHlsl, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

            SDL_GPUColorTargetDescription colorTargetDescription{};
            colorTargetDescription.format = colorTargetFormat_;
            colorTargetDescription.blend_state.enable_blend = true;
            colorTargetDescription.blend_state.color_write_mask = 0xFU;
            colorTargetDescription.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            colorTargetDescription.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            colorTargetDescription.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            colorTargetDescription.blend_state.dst_color_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            colorTargetDescription.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            colorTargetDescription.blend_state.dst_alpha_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

            std::array<SDL_GPUVertexBufferDescription, 2> bufferDescriptions{};
            bufferDescriptions[0].slot = 0U;
            bufferDescriptions[0].pitch = sizeof(SpriteVertex);
            bufferDescriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
            bufferDescriptions[1].slot = 1U;
            bufferDescriptions[1].pitch = sizeof(SpriteInstanceData);
            bufferDescriptions[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

            std::array<SDL_GPUVertexAttribute, 3> attributes{};
            attributes[0].location = 0U;
            attributes[0].buffer_slot = 0U;
            attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            attributes[0].offset = static_cast<Uint32>(offsetof(SpriteVertex, localX));
            attributes[1].location = 1U;
            attributes[1].buffer_slot = 0U;
            attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            attributes[1].offset = static_cast<Uint32>(offsetof(SpriteVertex, u));
            attributes[2].location = 2U;
            attributes[2].buffer_slot = 1U;
            attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            attributes[2].offset = static_cast<Uint32>(offsetof(SpriteInstanceData, centerClip));

            SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.vertex_shader = vertexShader;
            pipelineInfo.fragment_shader = fragmentShader;
            pipelineInfo.vertex_input_state.vertex_buffer_descriptions = bufferDescriptions.data();
            pipelineInfo.vertex_input_state.num_vertex_buffers =
                static_cast<Uint32>(bufferDescriptions.size());
            pipelineInfo.vertex_input_state.vertex_attributes = attributes.data();
            pipelineInfo.vertex_input_state.num_vertex_attributes =
                static_cast<Uint32>(attributes.size());
            pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pipelineInfo.rasterizer_state.enable_depth_clip = true;
            pipelineInfo.target_info.color_target_descriptions = &colorTargetDescription;
            pipelineInfo.target_info.num_color_targets = 1U;

            spritePipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipelineInfo);
            if (spritePipeline_ == nullptr)
            {
                throw MakeSdlError("SDL GPU sprite pipeline creation failed");
            }
        }
        catch (...)
        {
            if (vertexShader != nullptr)
            {
                SDL_ReleaseGPUShader(device_, vertexShader);
            }
            if (fragmentShader != nullptr)
            {
                SDL_ReleaseGPUShader(device_, fragmentShader);
            }
            throw;
        }

        SDL_ReleaseGPUShader(device_, vertexShader);
        SDL_ReleaseGPUShader(device_, fragmentShader);
    }

    void EnsureSpriteInstanceCapacity(const std::uint64_t requiredInstances)
    {
        if (requiredInstances == 0U || requiredInstances <= spriteInstanceCapacity_)
        {
            return;
        }

        constexpr std::uint64_t MaxInstances =
            static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()) /
            sizeof(SpriteInstanceData);
        if (requiredInstances > MaxInstances)
        {
            throw std::length_error{"Visible sprite instance data exceeds SDL GPU buffer size limits."};
        }

        std::uint64_t replacementCapacity =
            spriteInstanceCapacity_ == 0U ? 1U : spriteInstanceCapacity_;
        while (replacementCapacity < requiredInstances)
        {
            if (replacementCapacity > MaxInstances / 2U)
            {
                replacementCapacity = MaxInstances;
                break;
            }
            replacementCapacity *= 2U;
        }

        const Uint32 replacementBytes =
            static_cast<Uint32>(replacementCapacity * sizeof(SpriteInstanceData));

        SDL_GPUBufferCreateInfo bufferInfo{};
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufferInfo.size = replacementBytes;
        SDL_GPUBuffer* const replacementBuffer = SDL_CreateGPUBuffer(device_, &bufferInfo);
        if (replacementBuffer == nullptr)
        {
            throw MakeSdlError("SDL GPU sprite instance-buffer creation failed");
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = replacementBytes;
        SDL_GPUTransferBuffer* const replacementTransfer =
            SDL_CreateGPUTransferBuffer(device_, &transferInfo);
        if (replacementTransfer == nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, replacementBuffer);
            throw MakeSdlError("SDL GPU sprite instance transfer-buffer creation failed");
        }

        if (spriteInstanceTransferBuffer_ != nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(device_, spriteInstanceTransferBuffer_);
        }
        if (spriteInstanceBuffer_ != nullptr)
        {
            SDL_ReleaseGPUBuffer(device_, spriteInstanceBuffer_);
        }

        spriteInstanceBuffer_ = replacementBuffer;
        spriteInstanceTransferBuffer_ = replacementTransfer;
        spriteInstanceCapacity_ = replacementCapacity;
    }

    void UploadVisibleSpriteInstances(
        SDL_GPUCommandBuffer* const commandBuffer,
        const OrthographicView& view,
        const std::span<const SpriteRenderData> sprites,
        const std::uint64_t visibleSpriteCount)
    {
        if (visibleSpriteCount == 0U)
        {
            return;
        }

        EnsureSpriteInstanceCapacity(visibleSpriteCount);
        void* const mapped = SDL_MapGPUTransferBuffer(device_, spriteInstanceTransferBuffer_, true);
        if (mapped == nullptr)
        {
            throw MakeSdlError("SDL GPU sprite instance transfer-buffer mapping failed");
        }

        auto* const instances = static_cast<SpriteInstanceData*>(mapped);
        std::uint64_t visibleIndex = 0U;
        for (const SpriteRenderData& sprite : sprites)
        {
            if (!IsSpriteVisible(view, sprite))
            {
                continue;
            }
            instances[visibleIndex] = BuildSpriteInstanceData(view, sprite);
            ++visibleIndex;
        }
        SDL_UnmapGPUTransferBuffer(device_, spriteInstanceTransferBuffer_);

        if (visibleIndex != visibleSpriteCount)
        {
            throw std::logic_error{"Sprite visibility changed while building instance data."};
        }

        SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        if (copyPass == nullptr)
        {
            throw MakeSdlError("SDL GPU sprite instance upload copy-pass creation failed");
        }

        SDL_GPUTransferBufferLocation source{};
        source.transfer_buffer = spriteInstanceTransferBuffer_;
        SDL_GPUBufferRegion destination{};
        destination.buffer = spriteInstanceBuffer_;
        destination.size = static_cast<Uint32>(visibleSpriteCount * sizeof(SpriteInstanceData));
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);
        SDL_EndGPUCopyPass(copyPass);
    }

    void BindSpriteState(SDL_GPURenderPass* const renderPass)
    {
        SDL_GPUBufferBinding bindings[2]{};
        bindings[0].buffer = spriteVertexBuffer_;
        bindings[1].buffer = spriteInstanceBuffer_;
        SDL_BindGPUGraphicsPipeline(renderPass, spritePipeline_);
        SDL_BindGPUVertexBuffers(renderPass, 0U, bindings, 2U);
    }

    void DrawSpriteInstanceRun(
        SDL_GPURenderPass* const renderPass,
        const TextureHandle texture,
        const Uint32 firstInstance,
        const Uint32 instanceCount)
    {
        BindSpriteState(renderPass);

        SDL_GPUTextureSamplerBinding textureBinding{};
        textureBinding.texture = ResolveTexture(texture);
        textureBinding.sampler = spriteSampler_;
        SDL_BindGPUFragmentSamplers(renderPass, 0U, &textureBinding, 1U);
        SDL_DrawGPUPrimitives(
            renderPass,
            static_cast<Uint32>(SpriteVertices.size()),
            instanceCount,
            0U,
            firstInstance);
    }

    void EncodeVisibleSpriteRuns(
        SDL_GPURenderPass* const renderPass,
        const OrthographicView& view,
        const std::span<const SpriteRenderData> sprites,
        const std::uint64_t expectedVisibleSprites,
        std::uint64_t& encodedSpriteDraws,
        std::uint64_t& encodedSpriteCount)
    {
        if (expectedVisibleSprites == 0U)
        {
            return;
        }

        TextureHandle runTexture = InvalidTextureHandle;
        Uint32 runFirstInstance = 0U;
        Uint32 runInstanceCount = 0U;
        Uint32 visibleIndex = 0U;

        for (const SpriteRenderData& sprite : sprites)
        {
            if (!IsSpriteVisible(view, sprite))
            {
                continue;
            }

            if (runInstanceCount == 0U)
            {
                runTexture = sprite.texture;
                runFirstInstance = visibleIndex;
                runInstanceCount = 1U;
            }
            else if (sprite.texture == runTexture)
            {
                ++runInstanceCount;
            }
            else
            {
                DrawSpriteInstanceRun(renderPass, runTexture, runFirstInstance, runInstanceCount);
                ++encodedSpriteDraws;
                encodedSpriteCount += runInstanceCount;
                runTexture = sprite.texture;
                runFirstInstance = visibleIndex;
                runInstanceCount = 1U;
            }
            ++visibleIndex;
        }

        if (runInstanceCount != 0U)
        {
            DrawSpriteInstanceRun(renderPass, runTexture, runFirstInstance, runInstanceCount);
            ++encodedSpriteDraws;
            encodedSpriteCount += runInstanceCount;
        }

        if (static_cast<std::uint64_t>(visibleIndex) != expectedVisibleSprites ||
            encodedSpriteCount != expectedVisibleSprites)
        {
            throw std::logic_error{"Sprite visibility changed while encoding instance runs."};
        }
    }

    void EncodeMixedDraws(
        SDL_GPUCommandBuffer* const commandBuffer,
        SDL_GPURenderPass* const renderPass,
        const OrthographicView& view,
        const std::span<const SpriteRenderData> sprites,
        const std::span<const GpuParticleRenderData> particles,
        const std::uint64_t expectedVisibleSprites,
        std::uint64_t& encodedSpriteDraws,
        std::uint64_t& encodedSpriteCount,
        std::uint64_t& encodedParticleDraws,
        std::uint64_t& encodedParticleInstances)
    {
        std::size_t spriteIndex = 0U;
        std::size_t particleIndex = 0U;
        Uint32 visibleInstanceIndex = 0U;

        TextureHandle runTexture = InvalidTextureHandle;
        Uint32 runFirstInstance = 0U;
        Uint32 runInstanceCount = 0U;

        const auto flushSpriteRun = [&]()
        {
            if (runInstanceCount == 0U)
            {
                return;
            }
            DrawSpriteInstanceRun(renderPass, runTexture, runFirstInstance, runInstanceCount);
            ++encodedSpriteDraws;
            encodedSpriteCount += runInstanceCount;
            runInstanceCount = 0U;
        };

        while (spriteIndex < sprites.size() || particleIndex < particles.size())
        {
            const bool takeParticle =
                particleIndex < particles.size() &&
                (spriteIndex >= sprites.size() ||
                 ParticleBeforeSprite(particles[particleIndex], sprites[spriteIndex]));

            if (takeParticle)
            {
                flushSpriteRun();
                const GpuParticleRenderData& particle = particles[particleIndex];
                const TextureHandle textureHandle = particleGpuBackend_->Texture(particle.emitter);
                particleGpuBackend_->DrawEmitter(
                    commandBuffer,
                    renderPass,
                    view,
                    particle,
                    spriteVertexBuffer_,
                    spriteSampler_,
                    ResolveTexture(textureHandle),
                    encodedParticleDraws,
                    encodedParticleInstances);
                ++particleIndex;
                continue;
            }

            const SpriteRenderData& sprite = sprites[spriteIndex];
            ++spriteIndex;
            if (!IsSpriteVisible(view, sprite))
            {
                continue;
            }

            if (runInstanceCount == 0U)
            {
                runTexture = sprite.texture;
                runFirstInstance = visibleInstanceIndex;
                runInstanceCount = 1U;
            }
            else if (sprite.texture == runTexture)
            {
                ++runInstanceCount;
            }
            else
            {
                flushSpriteRun();
                runTexture = sprite.texture;
                runFirstInstance = visibleInstanceIndex;
                runInstanceCount = 1U;
            }
            ++visibleInstanceIndex;
        }

        flushSpriteRun();
        if (static_cast<std::uint64_t>(visibleInstanceIndex) != expectedVisibleSprites ||
            encodedSpriteCount != expectedVisibleSprites)
        {
            throw std::logic_error{"Sprite visibility changed while encoding mixed presentation."};
        }
    }

    void EncodeSpritePresentations(
        SDL_GPUCommandBuffer* const commandBuffer,
        SDL_GPURenderPass* const renderPass,
        const std::span<const SpritePresentationRenderData> sprites,
        std::uint64_t& encodedSpriteDraws,
        std::uint64_t& encodedSpriteCount)
    {
        for (std::size_t orderedIndex = 0U; orderedIndex < sprites.size(); ++orderedIndex)
        {
            const std::size_t sourceIndex = spriteGpuBackend_->OrderedSourceIndex(orderedIndex);
            const SpritePresentationRenderData& sprite = sprites[sourceIndex];
            if (spriteGpuBackend_->DrawPresentation(
                    commandBuffer,
                    renderPass,
                    ResolveTexture(sprite.texture),
                    sprite,
                    sourceIndex))
            {
                ++encodedSpriteDraws;
            }
        }
        encodedSpriteCount = spriteGpuBackend_->Metrics().lastVisibleSprites;
    }

    void EnsureOffscreenColorTarget(const Uint32 width, const Uint32 height)
    {
        if (width == 0U || height == 0U)
        {
            throw std::invalid_argument{"Offscreen color-target dimensions must be non-zero."};
        }
        if (offscreenColorTarget_ != nullptr &&
            offscreenTargetWidth_ == width && offscreenTargetHeight_ == height)
        {
            return;
        }

        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = colorTargetFormat_;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        textureInfo.width = width;
        textureInfo.height = height;
        textureInfo.layer_count_or_depth = 1U;
        textureInfo.num_levels = 1U;
        textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* const replacement = SDL_CreateGPUTexture(device_, &textureInfo);
        if (replacement == nullptr)
        {
            throw MakeSdlError("SDL GPU offscreen color-target creation failed");
        }
        if (offscreenColorTarget_ != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, offscreenColorTarget_);
        }

        offscreenColorTarget_ = replacement;
        offscreenTargetWidth_ = width;
        offscreenTargetHeight_ = height;
    }

    void EnsureCaptureTransferBuffer(const Uint32 requiredBytes)
    {
        if (requiredBytes == 0U)
        {
            throw std::invalid_argument{"Capture transfer-buffer capacity must be non-zero."};
        }
        if (captureTransferBuffer_ != nullptr && captureTransferBufferCapacity_ >= requiredBytes)
        {
            return;
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        transferInfo.size = requiredBytes;
        SDL_GPUTransferBuffer* const replacement = SDL_CreateGPUTransferBuffer(device_, &transferInfo);
        if (replacement == nullptr)
        {
            throw MakeSdlError("SDL GPU capture download transfer-buffer creation failed");
        }
        if (captureTransferBuffer_ != nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(device_, captureTransferBuffer_);
        }
        captureTransferBuffer_ = replacement;
        captureTransferBufferCapacity_ = requiredBytes;
    }

    [[nodiscard]] const TextureResource& ResolveTextureResource(const TextureHandle texture) const
    {
        if (!IsValidTextureHandle(texture))
        {
            throw std::invalid_argument{"Sprite texture handle is invalid."};
        }
        const std::size_t index = static_cast<std::size_t>(texture.slot);
        if (index >= textures_.size())
        {
            throw std::invalid_argument{
                "Sprite texture handle does not reference live renderer residency."};
        }

        const TextureResource& resource = textures_[index];
        if (resource.texture == nullptr || resource.generation != texture.generation)
        {
            throw std::invalid_argument{
                "Sprite texture handle generation does not reference live renderer residency."};
        }
        return resource;
    }

    [[nodiscard]] SDL_GPUTexture* ResolveTexture(const TextureHandle texture) const
    {
        return ResolveTextureResource(texture).texture;
    }

    void ValidatePresentationInputs(
        const std::span<const SpriteRenderData> sprites,
        const std::span<const GpuParticleRenderData> particles) const
    {
        for (const SpriteRenderData& sprite : sprites)
        {
            static_cast<void>(ResolveTexture(sprite.texture));
        }

        if (!particles.empty())
        {
            if (!std::is_sorted(sprites.begin(), sprites.end(), SpriteDrawOrderLess{}))
            {
                throw std::invalid_argument{
                    "Mixed sprite/particle presentation requires sprites sorted by (layer, stableOrder)."};
            }
            if (!std::is_sorted(particles.begin(), particles.end(), ParticleOrderLess))
            {
                throw std::invalid_argument{
                    "Mixed sprite/particle presentation requires GPU particles sorted by (layer, stableOrder)."};
            }
        }

        for (const GpuParticleRenderData& particle : particles)
        {
            const TextureHandle texture = particleGpuBackend_->Texture(particle.emitter);
            static_cast<void>(ResolveTexture(texture));
        }
    }

    void ValidateSpritePresentationInputs(
        const std::span<const SpritePresentationRenderData> sprites) const
    {
        for (const SpritePresentationRenderData& sprite : sprites)
        {
            const TextureResource& texture = ResolveTextureResource(sprite.texture);
            const SpriteAppearanceContractData& appearance = sprite.presentation.appearance;
            if (!texture.hasSpriteEncoding)
            {
                throw std::invalid_argument{
                    "SR4 Sprite presentation requires a texture created by CreateSpriteTextureRgba8."};
            }
            if (texture.spriteEncoding != appearance.textureEncoding)
            {
                throw std::invalid_argument{
                    "SR4 Sprite texture encoding does not match resolved canonical page color space."};
            }
            if (appearance.sourceAlphaMode != assets::SpriteAlphaMode::Straight)
            {
                throw std::invalid_argument{
                    "SR4 Sprite presentation requires canonical straight-alpha source truth."};
            }
            if (!IsFiniteUnit(appearance.tint.red) || !IsFiniteUnit(appearance.tint.green) ||
                !IsFiniteUnit(appearance.tint.blue) || !IsFiniteUnit(appearance.tint.alpha) ||
                !IsFiniteUnit(appearance.opacity))
            {
                throw std::invalid_argument{"SR4 Sprite presentation contains invalid tint or opacity."};
            }
            if (!IsFiniteNormalizedBounds(appearance.sampleBounds))
            {
                throw std::invalid_argument{
                    "SR4 Sprite presentation contains invalid atlas-safe sample bounds."};
            }
            switch (appearance.sampler)
            {
            case SpriteSamplerCompatibility::Nearest:
            case SpriteSamplerCompatibility::Linear:
                break;
            default:
                throw std::invalid_argument{"SR4 Sprite presentation has unsupported sampling."};
            }
            switch (appearance.blend)
            {
            case SpriteBlendCompatibility::Normal:
            case SpriteBlendCompatibility::Additive:
            case SpriteBlendCompatibility::Multiply:
            case SpriteBlendCompatibility::Screen:
                break;
            default:
                throw std::invalid_argument{"SR4 Sprite presentation has unsupported blending."};
            }
            switch (appearance.textureEncoding)
            {
            case SpriteTextureEncoding::Srgb:
            case SpriteTextureEncoding::Linear:
                break;
            default:
                throw std::invalid_argument{
                    "SR4 Sprite presentation has unsupported texture encoding."};
            }

            const SpriteDrawQuad& quad = sprite.presentation.quad;
            if (!IsFiniteDrawVertex(quad.topLeft) || !IsFiniteDrawVertex(quad.topRight) ||
                !IsFiniteDrawVertex(quad.bottomRight) || !IsFiniteDrawVertex(quad.bottomLeft))
            {
                throw std::invalid_argument{
                    "SR4 Sprite presentation contains invalid SR2 geometry or canonical UVs."};
            }
        }
    }

    void SyncSpriteGpuMetrics() noexcept
    {
        if (spriteGpuBackend_ == nullptr)
        {
            return;
        }
        const detail::SpriteGpuBackendMetrics& gpuMetrics = spriteGpuBackend_->Metrics();
        metrics_.spriteSamplerCreations = gpuMetrics.samplerCreations;
        metrics_.spritePipelineCreations = gpuMetrics.pipelineCreations;
        metrics_.materialShaderCompilations = gpuMetrics.materialShaderCompilations;
        metrics_.materialPipelineCreations = gpuMetrics.materialPipelineCreations;
        metrics_.materialPipelineCacheHits = gpuMetrics.materialPipelineCacheHits;
        metrics_.spriteVertexCapacitySprites = gpuMetrics.vertexCapacitySprites;
        metrics_.spriteVertexCapacityBytes = gpuMetrics.vertexCapacityBytes;
        metrics_.spriteMaskTargetCreations = gpuMetrics.maskTargetCreations;
    }

    void CommitFrameMetrics(
        const bool presented,
        const bool encodedRenderPass,
        const std::uint64_t encodedSpriteDraws,
        const std::uint64_t encodedSpriteCount,
        const std::uint64_t encodedParticleDraws,
        const std::uint64_t encodedParticleInstances,
        const std::uint64_t culledSpriteCount,
        const std::uint64_t encodedPresentationSpriteDraws,
        const std::uint64_t encodedPresentationSpriteCount,
        const Uint32 targetWidth,
        const Uint32 targetHeight) noexcept
    {
        ++metrics_.submittedFrames;
        metrics_.lastTargetWidth = targetWidth;
        metrics_.lastTargetHeight = targetHeight;
        if (presented)
        {
            ++metrics_.presentedFrames;
        }
        if (encodedRenderPass)
        {
            ++metrics_.renderPasses;
        }

        metrics_.drawCalls += encodedSpriteDraws + encodedParticleDraws;
        metrics_.submittedSprites += encodedSpriteCount;
        metrics_.submittedGpuParticleInstances += encodedParticleInstances;
        metrics_.gpuParticleDrawCalls += encodedParticleDraws;
        metrics_.culledSprites += culledSpriteCount;
        metrics_.spritePresentationDrawCalls += encodedPresentationSpriteDraws;
        metrics_.spritePresentationSprites += encodedPresentationSpriteCount;
        if (encodedPresentationSpriteCount != 0U && spriteGpuBackend_ != nullptr)
        {
            const detail::SpriteGpuBackendMetrics& gpuMetrics = spriteGpuBackend_->Metrics();
            metrics_.spritePresentationVisibleSprites += gpuMetrics.lastVisibleSprites;
            metrics_.spritePresentationCulledSprites += gpuMetrics.lastCulledSprites;
            metrics_.spritePresentationUploadedQuads += gpuMetrics.lastUploadedQuads;
            metrics_.spritePresentationUploadedVertexBytes += gpuMetrics.lastUploadedVertexBytes;
            metrics_.spritePresentationCompatibilityRuns += gpuMetrics.lastCompatibilityRuns;
            metrics_.materialPipelineSwitches += gpuMetrics.lastMaterialPipelineSwitches;
            metrics_.fragmentUniformUploads += gpuMetrics.lastFragmentUniformUploads;
            metrics_.fragmentUniformUploadBytes += gpuMetrics.lastFragmentUniformUploadBytes;
        }
        SyncSpriteGpuMetrics();
    }

    void RenderFrameInternal(
        const OrthographicCamera* const camera,
        const std::span<const SpriteRenderData> sprites,
        const std::span<const GpuParticleRenderData> particles,
        const std::span<const SpritePresentationRenderData> spritePresentations,
        const CaptureRequest* const captureRequest,
        CapturedFrame* const capturedFrame)
    {
        const bool captureRequested = captureRequest != nullptr;
        if (captureRequested != (capturedFrame != nullptr))
        {
            throw std::invalid_argument{
                "Capture request and capture output must be provided together."};
        }
        if (!spritePresentations.empty() && (!sprites.empty() || !particles.empty()))
        {
            throw std::invalid_argument{
                "SR4 Sprite presentation cannot be mixed with legacy SpriteRenderData or particles."};
        }

        const bool hasPresentation =
            !sprites.empty() || !particles.empty() || !spritePresentations.empty();
        if (hasPresentation)
        {
            if (camera == nullptr)
            {
                throw std::invalid_argument{"2D presentation requires an orthographic camera."};
            }
            if (!spritePresentations.empty())
            {
                ValidateSpritePresentationInputs(spritePresentations);
            }
            else
            {
                ValidatePresentationInputs(sprites, particles);
            }
        }

        SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(device_);
        if (commandBuffer == nullptr)
        {
            throw MakeSdlError("SDL GPU command buffer acquisition failed");
        }

        SDL_GPUTexture* swapchainTexture = nullptr;
        Uint32 targetWidth = 0U;
        Uint32 targetHeight = 0U;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                commandBuffer, window_, &swapchainTexture, &targetWidth, &targetHeight))
        {
            const std::string error{SDL_GetError()};
            SDL_CancelGPUCommandBuffer(commandBuffer);
            throw std::runtime_error{"SDL GPU swapchain acquisition failed: " + error};
        }

        bool encodedRenderPass = false;
        bool encodedCaptureDownload = false;
        std::uint64_t encodedSpriteDraws = 0U;
        std::uint64_t encodedSpriteCount = 0U;
        std::uint64_t encodedParticleDraws = 0U;
        std::uint64_t encodedParticleInstances = 0U;
        std::uint64_t culledSpriteCount = 0U;
        std::uint64_t encodedPresentationSpriteDraws = 0U;
        std::uint64_t encodedPresentationSpriteCount = 0U;
        CaptureReadbackLayout captureLayout{};

        if (swapchainTexture != nullptr)
        {
            OrthographicView view{};
            if (hasPresentation && !TryBuildOrthographicView(*camera, targetWidth, targetHeight, view))
            {
                static_cast<void>(SDL_SubmitGPUCommandBuffer(commandBuffer));
                throw std::invalid_argument{
                    "2D presentation requires a valid orthographic camera and render target."};
            }

            SpriteBatchMeasurement batchMeasurement{};
            if (!sprites.empty())
            {
                batchMeasurement = MeasureContiguousTextureBatching(view, sprites);
                culledSpriteCount = batchMeasurement.culledSprites;
            }

            try
            {
                EnsureOffscreenColorTarget(targetWidth, targetHeight);
                if (batchMeasurement.visibleSprites != 0U)
                {
                    UploadVisibleSpriteInstances(
                        commandBuffer, view, sprites, batchMeasurement.visibleSprites);
                }
                if (!spritePresentations.empty())
                {
                    spriteGpuBackend_->UploadPresentations(commandBuffer, view, spritePresentations);
                    SyncSpriteGpuMetrics();
                }

                if (captureRequested)
                {
                    if (!TryBuildCaptureReadbackLayout(targetWidth, targetHeight, captureLayout))
                    {
                        throw std::length_error{
                            "Capture target exceeds supported readback transfer-buffer limits."};
                    }
                    static_cast<void>(ResolveCaptureSourceChannelOrder(colorTargetFormat_));
                    EnsureCaptureTransferBuffer(captureLayout.transferBufferBytes);
                }
            }
            catch (...)
            {
                static_cast<void>(SDL_SubmitGPUCommandBuffer(commandBuffer));
                throw;
            }

            SDL_GPUColorTargetInfo colorTarget{};
            colorTarget.texture = offscreenColorTarget_;
            colorTarget.clear_color = SDL_FColor{
                config_.clearColor.red,
                config_.clearColor.green,
                config_.clearColor.blue,
                config_.clearColor.alpha,
            };
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            colorTarget.cycle = true;

            SDL_GPURenderPass* renderPass = nullptr;
            if (!spritePresentations.empty())
            {
                renderPass = spriteGpuBackend_->BeginPresentationRenderPass(
                    commandBuffer, colorTarget, targetWidth, targetHeight);
                SyncSpriteGpuMetrics();
            }
            else
            {
                renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1U, nullptr);
            }
            if (renderPass == nullptr)
            {
                const std::string error{SDL_GetError()};
                static_cast<void>(SDL_SubmitGPUCommandBuffer(commandBuffer));
                throw std::runtime_error{"SDL GPU render pass creation failed: " + error};
            }

            const bool spritePixelPerfectRaster =
                !spritePresentations.empty() &&
                spritePresentations.front().pixelPerfectViewport != nullptr;
            if (hasPresentation)
            {
                ApplyPresentationRasterViewport(
                    renderPass,
                    *camera,
                    targetWidth,
                    targetHeight,
                    spritePixelPerfectRaster);
            }

            if (!spritePresentations.empty())
            {
                EncodeSpritePresentations(
                    commandBuffer,
                    renderPass,
                    spritePresentations,
                    encodedSpriteDraws,
                    encodedSpriteCount);
                const detail::SpriteGpuBackendMetrics& presentationMetrics =
                    spriteGpuBackend_->Metrics();
                if (encodedSpriteDraws != presentationMetrics.lastCompatibilityRuns ||
                    encodedSpriteCount != presentationMetrics.lastVisibleSprites)
                {
                    throw std::logic_error{
                        "Sprite encoded draw/visibility metrics diverged from the upload plan."};
                }
                encodedPresentationSpriteDraws = encodedSpriteDraws;
                encodedPresentationSpriteCount = presentationMetrics.lastSubmittedSprites;
                culledSpriteCount = presentationMetrics.lastCulledSprites;
            }
            else if (!particles.empty())
            {
                EncodeMixedDraws(
                    commandBuffer,
                    renderPass,
                    view,
                    sprites,
                    particles,
                    batchMeasurement.visibleSprites,
                    encodedSpriteDraws,
                    encodedSpriteCount,
                    encodedParticleDraws,
                    encodedParticleInstances);
            }
            else if (batchMeasurement.visibleSprites != 0U)
            {
                EncodeVisibleSpriteRuns(
                    renderPass,
                    view,
                    sprites,
                    batchMeasurement.visibleSprites,
                    encodedSpriteDraws,
                    encodedSpriteCount);
            }

            SDL_EndGPURenderPass(renderPass);
            encodedRenderPass = true;

            SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
            if (copyPass == nullptr)
            {
                const std::string error{SDL_GetError()};
                static_cast<void>(SDL_SubmitGPUCommandBuffer(commandBuffer));
                throw std::runtime_error{
                    "SDL GPU presentation/capture copy-pass creation failed: " + error};
            }

            SDL_GPUTextureLocation presentationSource{};
            presentationSource.texture = offscreenColorTarget_;
            SDL_GPUTextureLocation presentationDestination{};
            presentationDestination.texture = swapchainTexture;
            SDL_CopyGPUTextureToTexture(
                copyPass,
                &presentationSource,
                &presentationDestination,
                targetWidth,
                targetHeight,
                1U,
                false);

            if (captureRequested)
            {
                SDL_GPUTextureRegion captureSource{};
                captureSource.texture = offscreenColorTarget_;
                captureSource.w = targetWidth;
                captureSource.h = targetHeight;
                captureSource.d = 1U;

                SDL_GPUTextureTransferInfo captureDestination{};
                captureDestination.transfer_buffer = captureTransferBuffer_;
                captureDestination.pixels_per_row = captureLayout.pixelsPerRow;
                captureDestination.rows_per_layer = targetHeight;
                SDL_DownloadFromGPUTexture(copyPass, &captureSource, &captureDestination);
                encodedCaptureDownload = true;
                ++metrics_.explicitGpuReadbacks;
            }
            SDL_EndGPUCopyPass(copyPass);
        }

        if (captureRequested && !encodedCaptureDownload)
        {
            if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
            {
                throw MakeSdlError(
                    "SDL GPU command buffer submission failed while capture target was unavailable");
            }
            CommitFrameMetrics(
                false,
                encodedRenderPass,
                encodedSpriteDraws,
                encodedSpriteCount,
                encodedParticleDraws,
                encodedParticleInstances,
                culledSpriteCount,
                encodedPresentationSpriteDraws,
                encodedPresentationSpriteCount,
                targetWidth,
                targetHeight);
            throw std::runtime_error{"Capture requires an available non-zero presentation target."};
        }

        if (!captureRequested)
        {
            if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
            {
                throw MakeSdlError("SDL GPU command buffer submission failed");
            }
            CommitFrameMetrics(
                swapchainTexture != nullptr,
                encodedRenderPass,
                encodedSpriteDraws,
                encodedSpriteCount,
                encodedParticleDraws,
                encodedParticleInstances,
                culledSpriteCount,
                encodedPresentationSpriteDraws,
                encodedPresentationSpriteCount,
                targetWidth,
                targetHeight);
            return;
        }

        SDL_GPUFence* const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commandBuffer);
        if (fence == nullptr)
        {
            throw MakeSdlError("SDL GPU capture command buffer submission/fence acquisition failed");
        }

        CommitFrameMetrics(
            true,
            encodedRenderPass,
            encodedSpriteDraws,
            encodedSpriteCount,
            encodedParticleDraws,
            encodedParticleInstances,
            culledSpriteCount,
            encodedPresentationSpriteDraws,
            encodedPresentationSpriteCount,
            targetWidth,
            targetHeight);

        SDL_GPUFence* fences[]{fence};
        ++metrics_.explicitGpuFenceWaits;
        if (!SDL_WaitForGPUFences(device_, true, fences, 1U))
        {
            const std::string error{SDL_GetError()};
            SDL_ReleaseGPUFence(device_, fence);
            throw std::runtime_error{"SDL GPU capture fence wait failed: " + error};
        }

        void* const mapped = SDL_MapGPUTransferBuffer(device_, captureTransferBuffer_, false);
        if (mapped == nullptr)
        {
            SDL_ReleaseGPUFence(device_, fence);
            throw MakeSdlError("SDL GPU capture transfer-buffer mapping failed");
        }

        try
        {
            *capturedFrame = NormalizeCapturedFrame(
                mapped,
                captureLayout,
                colorTargetFormat_,
                captureRequest->simulationFrame,
                targetWidth,
                targetHeight);
        }
        catch (...)
        {
            SDL_UnmapGPUTransferBuffer(device_, captureTransferBuffer_);
            SDL_ReleaseGPUFence(device_, fence);
            throw;
        }

        SDL_UnmapGPUTransferBuffer(device_, captureTransferBuffer_);
        SDL_ReleaseGPUFence(device_, fence);
    }

    void Cleanup() noexcept
    {
        if (device_ != nullptr)
        {
            particleGpuBackend_.reset();
            spriteGpuBackend_.reset();

            for (TextureResource& resource : textures_)
            {
                if (resource.texture != nullptr)
                {
                    SDL_ReleaseGPUTexture(device_, resource.texture);
                    resource.texture = nullptr;
                }
                resource.generation = 0U;
            }

            if (captureTransferBuffer_ != nullptr)
            {
                SDL_ReleaseGPUTransferBuffer(device_, captureTransferBuffer_);
                captureTransferBuffer_ = nullptr;
                captureTransferBufferCapacity_ = 0U;
            }
            if (offscreenColorTarget_ != nullptr)
            {
                SDL_ReleaseGPUTexture(device_, offscreenColorTarget_);
                offscreenColorTarget_ = nullptr;
                offscreenTargetWidth_ = 0U;
                offscreenTargetHeight_ = 0U;
            }
            if (spriteInstanceTransferBuffer_ != nullptr)
            {
                SDL_ReleaseGPUTransferBuffer(device_, spriteInstanceTransferBuffer_);
                spriteInstanceTransferBuffer_ = nullptr;
            }
            if (spriteInstanceBuffer_ != nullptr)
            {
                SDL_ReleaseGPUBuffer(device_, spriteInstanceBuffer_);
                spriteInstanceBuffer_ = nullptr;
                spriteInstanceCapacity_ = 0U;
            }
            if (spritePipeline_ != nullptr)
            {
                SDL_ReleaseGPUGraphicsPipeline(device_, spritePipeline_);
                spritePipeline_ = nullptr;
            }
            if (spriteSampler_ != nullptr)
            {
                SDL_ReleaseGPUSampler(device_, spriteSampler_);
                spriteSampler_ = nullptr;
            }
            if (spriteVertexBuffer_ != nullptr)
            {
                SDL_ReleaseGPUBuffer(device_, spriteVertexBuffer_);
                spriteVertexBuffer_ = nullptr;
            }
        }

        if (windowClaimed_ && device_ != nullptr && window_ != nullptr)
        {
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
            windowClaimed_ = false;
        }
        if (device_ != nullptr)
        {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        window_ = nullptr;
    }

    RendererConfig config_{};
    RenderMetrics metrics_{};
    SDL_Window* window_{nullptr};
    SDL_GPUDevice* device_{nullptr};
    SDL_GPUTextureFormat colorTargetFormat_{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUTexture* offscreenColorTarget_{nullptr};
    Uint32 offscreenTargetWidth_{0U};
    Uint32 offscreenTargetHeight_{0U};
    SDL_GPUTransferBuffer* captureTransferBuffer_{nullptr};
    Uint32 captureTransferBufferCapacity_{0U};
    SDL_GPUGraphicsPipeline* spritePipeline_{nullptr};
    SDL_GPUSampler* spriteSampler_{nullptr};
    SDL_GPUBuffer* spriteVertexBuffer_{nullptr};
    SDL_GPUBuffer* spriteInstanceBuffer_{nullptr};
    SDL_GPUTransferBuffer* spriteInstanceTransferBuffer_{nullptr};
    std::uint64_t spriteInstanceCapacity_{0U};
    std::vector<TextureResource> textures_{};
    std::unique_ptr<detail::ParticleGpuBackend> particleGpuBackend_{};
    std::unique_ptr<detail::SpriteGpuBackend> spriteGpuBackend_{};
    bool windowClaimed_{false};
    std::string driverName_{};
};

Renderer::Renderer(const RendererConfig& config, const platform::Platform& platform)
    : impl_{std::make_unique<Impl>(config, platform)}
{
}

Renderer::~Renderer() = default;

TextureHandle Renderer::CreateTextureRgba8(
    const TextureHandle texture,
    const Rgba8TextureData& textureData)
{
    return impl_->CreateTextureRgba8(texture, textureData);
}

TextureHandle Renderer::CreateSpriteTextureRgba8(
    const TextureHandle texture,
    const Rgba8TextureData& textureData,
    const SpriteTextureEncoding encoding)
{
    return impl_->CreateSpriteTextureRgba8(texture, textureData, encoding);
}

void Renderer::DestroyTexture(const TextureHandle texture) noexcept
{
    impl_->DestroyTexture(texture);
}

MaterialGpuPrepareResult2D Renderer::PrepareMaterial2D(
    const assets::ResourceRegistry& resources,
    const assets::ResourceHandle<assets::Material2DResource> material)
{
    return impl_->PrepareMaterial2D(resources, material);
}

GpuParticleEmitterCreateResult Renderer::CreateGpuParticleEmitter(
    const particles::ParticleProgram& program,
    const std::uint64_t globalSeed,
    const std::uint64_t emitterStableId,
    const TextureHandle texture)
{
    return impl_->CreateGpuParticleEmitter(program, globalSeed, emitterStableId, texture);
}

void Renderer::DestroyGpuParticleEmitter(const GpuParticleEmitterHandle emitter) noexcept
{
    impl_->DestroyGpuParticleEmitter(emitter);
}

void Renderer::ResetGpuParticleEmitter(const GpuParticleEmitterHandle emitter)
{
    impl_->ResetGpuParticleEmitter(emitter);
}

void Renderer::PlayGpuParticleEmitter(const GpuParticleEmitterHandle emitter)
{
    impl_->PlayGpuParticleEmitter(emitter);
}

void Renderer::RestartGpuParticleEmitter(const GpuParticleEmitterHandle emitter)
{
    impl_->RestartGpuParticleEmitter(emitter);
}

void Renderer::StopGpuParticleEmitter(const GpuParticleEmitterHandle emitter) noexcept
{
    impl_->StopGpuParticleEmitter(emitter);
}

bool Renderer::StepGpuParticleEmitter(const GpuParticleStepData& step)
{
    return impl_->StepGpuParticleEmitters(std::span<const GpuParticleStepData>{&step, 1U});
}

bool Renderer::StepGpuParticleEmitters(const std::span<const GpuParticleStepData> steps)
{
    return impl_->StepGpuParticleEmitters(steps);
}

GpuParticleEmitterMetrics Renderer::GpuParticleMetrics(const GpuParticleEmitterHandle emitter) const
{
    return impl_->GpuParticleMetrics(emitter);
}

void Renderer::RenderFrame()
{
    impl_->RenderFrame();
}

void Renderer::RenderFrame(const OrthographicCamera& camera, const SpriteRenderData& sprite)
{
    impl_->RenderFrame(camera, sprite);
}

void Renderer::RenderFrame(
    const OrthographicCamera& camera,
    const std::span<const SpriteRenderData> sprites)
{
    impl_->RenderFrame(camera, sprites);
}

void Renderer::RenderFrame(
    const OrthographicCamera& camera,
    const std::span<const SpriteRenderData> sprites,
    const std::span<const GpuParticleRenderData> particles)
{
    impl_->RenderFrame(camera, sprites, particles);
}

void Renderer::RenderFrame(
    const OrthographicCamera& camera,
    const SpritePresentationRenderData& sprite)
{
    impl_->RenderFrame(camera, sprite);
}

void Renderer::RenderFrame(
    const OrthographicCamera& camera,
    const std::span<const SpritePresentationRenderData> sprites)
{
    impl_->RenderFrame(camera, sprites);
}

CapturedFrame Renderer::CaptureFrame(
    const CaptureRequest& request,
    const OrthographicCamera& camera,
    const SpriteRenderData& sprite)
{
    return impl_->CaptureFrame(request, camera, sprite);
}

CapturedFrame Renderer::CaptureFrame(
    const CaptureRequest& request,
    const OrthographicCamera& camera,
    const std::span<const SpriteRenderData> sprites)
{
    return impl_->CaptureFrame(request, camera, sprites);
}

CapturedFrame Renderer::CaptureFrame(
    const CaptureRequest& request,
    const OrthographicCamera& camera,
    const std::span<const SpriteRenderData> sprites,
    const std::span<const GpuParticleRenderData> particles)
{
    return impl_->CaptureFrame(request, camera, sprites, particles);
}

CapturedFrame Renderer::CaptureFrame(
    const CaptureRequest& request,
    const OrthographicCamera& camera,
    const SpritePresentationRenderData& sprite)
{
    return impl_->CaptureFrame(request, camera, sprite);
}

CapturedFrame Renderer::CaptureFrame(
    const CaptureRequest& request,
    const OrthographicCamera& camera,
    const std::span<const SpritePresentationRenderData> sprites)
{
    return impl_->CaptureFrame(request, camera, sprites);
}

const RendererConfig& Renderer::Config() const noexcept
{
    return impl_->Config();
}

const RenderMetrics& Renderer::Metrics() const noexcept
{
    return impl_->Metrics();
}

std::string_view Renderer::DriverName() const noexcept
{
    return impl_->DriverName();
}
} // namespace trace2d::render
