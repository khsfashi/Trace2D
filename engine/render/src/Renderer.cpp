#include <trace2d/render/Renderer.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace trace2d::render
{
namespace
{
constexpr SDL_GPUShaderFormat SupportedShaderFormats = static_cast<SDL_GPUShaderFormat>(
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL);
constexpr std::uint64_t Rgba8BytesPerPixel = 4;

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

struct alignas(16) SpriteUniforms final
{
    float centerClipX;
    float centerClipY;
    float halfClipX;
    float halfClipY;
};

static_assert(sizeof(SpriteUniforms) == 16);

constexpr std::array<SpriteVertex, 6> SpriteVertices{
    SpriteVertex{-1.0F, -1.0F, 0.0F, 1.0F},
    SpriteVertex{1.0F, -1.0F, 1.0F, 1.0F},
    SpriteVertex{1.0F, 1.0F, 1.0F, 0.0F},
    SpriteVertex{-1.0F, -1.0F, 0.0F, 1.0F},
    SpriteVertex{1.0F, 1.0F, 1.0F, 0.0F},
    SpriteVertex{-1.0F, 1.0F, 0.0F, 0.0F},
};

constexpr char SpriteVertexShaderHlsl[] = R"(
cbuffer SpriteUniforms : register(b0, space1)
{
    float4 spriteTransform;
};

struct VertexInput
{
    float2 localPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
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
        spriteTransform.xy + input.localPosition * spriteTransform.zw,
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

[[nodiscard]] std::runtime_error MakeSdlError(const char* context)
{
    return std::runtime_error{std::string{context} + ": " + SDL_GetError()};
}

class ShaderCrossScope final
{
public:
    ShaderCrossScope()
    {
        if (!SDL_ShaderCross_Init())
        {
            throw MakeSdlError("SDL_shadercross initialization failed");
        }
    }

    ~ShaderCrossScope()
    {
        SDL_ShaderCross_Quit();
    }

    ShaderCrossScope(const ShaderCrossScope&) = delete;
    ShaderCrossScope& operator=(const ShaderCrossScope&) = delete;
};

[[nodiscard]] SDL_GPUShader* CompileHlslShader(
    SDL_GPUDevice* const device,
    const char* const source,
    const SDL_ShaderCross_ShaderStage shaderCrossStage)
{
    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.source = source;
    hlslInfo.entrypoint = "main";
    hlslInfo.shader_stage = shaderCrossStage;

    std::size_t spirvSize = 0;
    void* const spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
    if (spirv == nullptr)
    {
        throw MakeSdlError("HLSL to SPIR-V compilation failed");
    }

    SDL_ShaderCross_GraphicsShaderMetadata* const metadata = SDL_ShaderCross_ReflectGraphicsSPIRV(
        static_cast<const Uint8*>(spirv), spirvSize, 0);
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
    if (textureData.width == 0 || textureData.height == 0)
    {
        throw std::invalid_argument{"RGBA8 texture dimensions must be non-zero."};
    }

    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(textureData.width) * static_cast<std::uint64_t>(textureData.height);
    const std::uint64_t byteCount = pixelCount * Rgba8BytesPerPixel;

    if (byteCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        byteCount > static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()))
    {
        throw std::length_error{"RGBA8 texture upload exceeds SDL GPU transfer-buffer size limits."};
    }

    return static_cast<std::size_t>(byteCount);
}

[[nodiscard]] CaptureSourceChannelOrder ResolveCaptureSourceChannelOrder(const SDL_GPUTextureFormat format)
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
    const std::size_t packedRowBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(Rgba8BytesPerPixel);
    const std::size_t canonicalByteCount = packedRowBytes * static_cast<std::size_t>(height);

    CapturedFrame frame{};
    frame.simulationFrame = simulationFrame;
    frame.width = width;
    frame.height = height;
    frame.rgba8Pixels.resize(canonicalByteCount);

    const auto* const sourceBytes = static_cast<const std::uint8_t*>(mappedReadback);

    for (Uint32 y = 0; y < height; ++y)
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

        for (Uint32 x = 0; x < width; ++x)
        {
            const std::size_t offset = static_cast<std::size_t>(x) * static_cast<std::size_t>(Rgba8BytesPerPixel);
            destinationRow[offset] = sourceRow[offset + 2];
            destinationRow[offset + 1] = sourceRow[offset + 1];
            destinationRow[offset + 2] = sourceRow[offset];
            destinationRow[offset + 3] = sourceRow[offset + 3];
        }
    }

    return frame;
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

            CreatePersistentSpriteResources();

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

    [[nodiscard]] TextureHandle CreateTextureRgba8(const Rgba8TextureData& textureData)
    {
        const std::size_t byteCount = ExpectedRgba8ByteCount(textureData);
        if (textureData.pixels.size() != byteCount)
        {
            throw std::invalid_argument{"RGBA8 texture byte count must equal width * height * 4."};
        }

        if (textures_.size() >= static_cast<std::size_t>(std::numeric_limits<TextureHandle>::max() - 1U))
        {
            throw std::length_error{"Trace2D texture handle space exhausted."};
        }

        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        textureInfo.width = textureData.width;
        textureInfo.height = textureData.height;
        textureInfo.layer_count_or_depth = 1;
        textureInfo.num_levels = 1;
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
        destination.d = 1;

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
        textures_.push_back(texture);
        return static_cast<TextureHandle>(textures_.size());
    }

    void DestroyTexture(const TextureHandle texture) noexcept
    {
        SDL_GPUTexture*& resource = ResolveTextureSlot(texture);
        if (resource != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, resource);
            resource = nullptr;
        }
    }

    void RenderFrame()
    {
        RenderFrameInternal(nullptr, {}, nullptr, nullptr);
    }

    void RenderFrame(const OrthographicCamera& camera, const SpriteRenderData& sprite)
    {
        RenderFrame(camera, std::span<const SpriteRenderData>{&sprite, 1});
    }

    void RenderFrame(const OrthographicCamera& camera, const std::span<const SpriteRenderData> sprites)
    {
        RenderFrameInternal(&camera, sprites, nullptr, nullptr);
    }

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const SpriteRenderData& sprite)
    {
        return CaptureFrame(request, camera, std::span<const SpriteRenderData>{&sprite, 1});
    }

    [[nodiscard]] CapturedFrame CaptureFrame(
        const CaptureRequest& request,
        const OrthographicCamera& camera,
        const std::span<const SpriteRenderData> sprites)
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

        CapturedFrame frame{};
        RenderFrameInternal(&camera, sprites, &request, &frame);
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
        const ShaderCrossScope shaderCrossScope{};

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
            colorTargetDescription.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            colorTargetDescription.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            colorTargetDescription.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

            SDL_GPUVertexBufferDescription vertexBufferDescription{};
            vertexBufferDescription.slot = 0;
            vertexBufferDescription.pitch = sizeof(SpriteVertex);
            vertexBufferDescription.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
            vertexBufferDescription.instance_step_rate = 0;

            std::array<SDL_GPUVertexAttribute, 2> vertexAttributes{};
            vertexAttributes[0].location = 0;
            vertexAttributes[0].buffer_slot = 0;
            vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            vertexAttributes[0].offset = static_cast<Uint32>(offsetof(SpriteVertex, localX));
            vertexAttributes[1].location = 1;
            vertexAttributes[1].buffer_slot = 0;
            vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            vertexAttributes[1].offset = static_cast<Uint32>(offsetof(SpriteVertex, u));

            SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.vertex_shader = vertexShader;
            pipelineInfo.fragment_shader = fragmentShader;
            pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDescription;
            pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
            pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes.data();
            pipelineInfo.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(vertexAttributes.size());
            pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            pipelineInfo.rasterizer_state.enable_depth_clip = true;
            pipelineInfo.target_info.color_target_descriptions = &colorTargetDescription;
            pipelineInfo.target_info.num_color_targets = 1;

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

    void EnsureOffscreenColorTarget(const Uint32 width, const Uint32 height)
    {
        if (width == 0 || height == 0)
        {
            throw std::invalid_argument{"Offscreen color-target dimensions must be non-zero."};
        }

        if (offscreenColorTarget_ != nullptr && offscreenTargetWidth_ == width && offscreenTargetHeight_ == height)
        {
            return;
        }

        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = colorTargetFormat_;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        textureInfo.width = width;
        textureInfo.height = height;
        textureInfo.layer_count_or_depth = 1;
        textureInfo.num_levels = 1;
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
        if (requiredBytes == 0)
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

    [[nodiscard]] SDL_GPUTexture* ResolveTexture(const TextureHandle texture) const
    {
        if (texture == InvalidTextureHandle)
        {
            throw std::invalid_argument{"Sprite texture handle is invalid."};
        }

        const std::size_t index = static_cast<std::size_t>(texture - 1U);
        if (index >= textures_.size() || textures_[index] == nullptr)
        {
            throw std::invalid_argument{"Sprite texture handle does not reference a live renderer texture."};
        }

        return textures_[index];
    }

    [[nodiscard]] SDL_GPUTexture*& ResolveTextureSlot(const TextureHandle texture) noexcept
    {
        static SDL_GPUTexture* invalidSlot = nullptr;

        if (texture == InvalidTextureHandle)
        {
            return invalidSlot;
        }

        const std::size_t index = static_cast<std::size_t>(texture - 1U);
        if (index >= textures_.size())
        {
            return invalidSlot;
        }

        return textures_[index];
    }

    void ValidateSpriteTextures(const std::span<const SpriteRenderData> sprites) const
    {
        for (const SpriteRenderData& sprite : sprites)
        {
            static_cast<void>(ResolveTexture(sprite.texture));
        }
    }

    void CommitFrameMetrics(
        const bool presented,
        const bool encodedRenderPass,
        const std::uint64_t encodedSpriteDraws,
        const std::uint64_t culledSpriteCount,
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

        metrics_.drawCalls += encodedSpriteDraws;
        metrics_.submittedSprites += encodedSpriteDraws;
        metrics_.culledSprites += culledSpriteCount;
    }

    void RenderFrameInternal(
        const OrthographicCamera* const camera,
        const std::span<const SpriteRenderData> sprites,
        const CaptureRequest* const captureRequest,
        CapturedFrame* const capturedFrame)
    {
        const bool captureRequested = captureRequest != nullptr;
        if (captureRequested != (capturedFrame != nullptr))
        {
            throw std::invalid_argument{"Capture request and capture output must be provided together."};
        }

        if (!sprites.empty())
        {
            if (camera == nullptr)
            {
                throw std::invalid_argument{"Sprite rendering requires an orthographic camera."};
            }

            ValidateSpriteTextures(sprites);
        }

        SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(device_);
        if (commandBuffer == nullptr)
        {
            throw MakeSdlError("SDL GPU command buffer acquisition failed");
        }

        SDL_GPUTexture* swapchainTexture = nullptr;
        Uint32 targetWidth = 0;
        Uint32 targetHeight = 0;

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                commandBuffer, window_, &swapchainTexture, &targetWidth, &targetHeight))
        {
            const std::string error{SDL_GetError()};
            SDL_CancelGPUCommandBuffer(commandBuffer);
            throw std::runtime_error{"SDL GPU swapchain acquisition failed: " + error};
        }

        bool encodedRenderPass = false;
        bool encodedCaptureDownload = false;
        std::uint64_t encodedSpriteDraws = 0;
        std::uint64_t culledSpriteCount = 0;
        CaptureReadbackLayout captureLayout{};

        if (swapchainTexture != nullptr)
        {
            OrthographicView view{};
            if (!sprites.empty() && !TryBuildOrthographicView(*camera, targetWidth, targetHeight, view))
            {
                static_cast<void>(SDL_SubmitGPUCommandBuffer(commandBuffer));
                throw std::invalid_argument{"Sprite rendering requires a valid orthographic camera and render target."};
            }

            try
            {
                EnsureOffscreenColorTarget(targetWidth, targetHeight);

                if (captureRequested)
                {
                    if (!TryBuildCaptureReadbackLayout(targetWidth, targetHeight, captureLayout))
                    {
                        throw std::length_error{"Capture target exceeds supported readback transfer-buffer limits."};
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

            SDL_GPURenderPass* const renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
            if (renderPass == nullptr)
            {
                const std::string error{SDL_GetError()};
                static_cast<void>(SDL_SubmitGPUCommandBuffer(commandBuffer));
                throw std::runtime_error{"SDL GPU render pass creation failed: " + error};
            }

            if (!sprites.empty())
            {
                SDL_GPUBufferBinding vertexBinding{};
                vertexBinding.buffer = spriteVertexBuffer_;
                bool spriteStateBound = false;

                for (const SpriteRenderData& sprite : sprites)
                {
                    if (!IsSpriteVisible(view, sprite))
                    {
                        ++culledSpriteCount;
                        continue;
                    }

                    if (!spriteStateBound)
                    {
                        SDL_BindGPUGraphicsPipeline(renderPass, spritePipeline_);
                        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
                        spriteStateBound = true;
                    }

                    const Float2 centerClip = WorldToClip(view, sprite.center);
                    const SpriteUniforms uniforms{
                        centerClip.x,
                        centerClip.y,
                        sprite.halfExtents.x * view.clipScale.x,
                        sprite.halfExtents.y * view.clipScale.y,
                    };
                    SDL_PushGPUVertexUniformData(commandBuffer, 0, &uniforms, sizeof(uniforms));

                    SDL_GPUTextureSamplerBinding textureBinding{};
                    textureBinding.texture = ResolveTexture(sprite.texture);
                    textureBinding.sampler = spriteSampler_;

                    SDL_BindGPUFragmentSamplers(renderPass, 0, &textureBinding, 1);
                    SDL_DrawGPUPrimitives(renderPass, static_cast<Uint32>(SpriteVertices.size()), 1, 0, 0);
                    ++encodedSpriteDraws;
                }
            }

            SDL_EndGPURenderPass(renderPass);
            encodedRenderPass = true;

            SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
            if (copyPass == nullptr)
            {
                const std::string error{SDL_GetError()};
                static_cast<void>(SDL_SubmitGPUCommandBuffer(commandBuffer));
                throw std::runtime_error{"SDL GPU presentation/capture copy-pass creation failed: " + error};
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
                1,
                false);

            if (captureRequested)
            {
                SDL_GPUTextureRegion captureSource{};
                captureSource.texture = offscreenColorTarget_;
                captureSource.w = targetWidth;
                captureSource.h = targetHeight;
                captureSource.d = 1;

                SDL_GPUTextureTransferInfo captureDestination{};
                captureDestination.transfer_buffer = captureTransferBuffer_;
                captureDestination.pixels_per_row = captureLayout.pixelsPerRow;
                captureDestination.rows_per_layer = targetHeight;

                SDL_DownloadFromGPUTexture(copyPass, &captureSource, &captureDestination);
                encodedCaptureDownload = true;
            }

            SDL_EndGPUCopyPass(copyPass);
        }

        if (captureRequested && !encodedCaptureDownload)
        {
            if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
            {
                throw MakeSdlError("SDL GPU command buffer submission failed while capture target was unavailable");
            }

            CommitFrameMetrics(
                false,
                encodedRenderPass,
                encodedSpriteDraws,
                culledSpriteCount,
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
                culledSpriteCount,
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
            culledSpriteCount,
            targetWidth,
            targetHeight);

        SDL_GPUFence* fences[]{fence};
        if (!SDL_WaitForGPUFences(device_, true, fences, 1))
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
            for (SDL_GPUTexture*& texture : textures_)
            {
                if (texture != nullptr)
                {
                    SDL_ReleaseGPUTexture(device_, texture);
                    texture = nullptr;
                }
            }

            if (captureTransferBuffer_ != nullptr)
            {
                SDL_ReleaseGPUTransferBuffer(device_, captureTransferBuffer_);
                captureTransferBuffer_ = nullptr;
                captureTransferBufferCapacity_ = 0;
            }

            if (offscreenColorTarget_ != nullptr)
            {
                SDL_ReleaseGPUTexture(device_, offscreenColorTarget_);
                offscreenColorTarget_ = nullptr;
                offscreenTargetWidth_ = 0;
                offscreenTargetHeight_ = 0;
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
    Uint32 offscreenTargetWidth_{0};
    Uint32 offscreenTargetHeight_{0};
    SDL_GPUTransferBuffer* captureTransferBuffer_{nullptr};
    Uint32 captureTransferBufferCapacity_{0};
    SDL_GPUGraphicsPipeline* spritePipeline_{nullptr};
    SDL_GPUSampler* spriteSampler_{nullptr};
    SDL_GPUBuffer* spriteVertexBuffer_{nullptr};
    std::vector<SDL_GPUTexture*> textures_{};
    bool windowClaimed_{false};
    std::string driverName_{};
};

Renderer::Renderer(const RendererConfig& config, const platform::Platform& platform)
    : impl_{std::make_unique<Impl>(config, platform)}
{
}

Renderer::~Renderer() = default;

TextureHandle Renderer::CreateTextureRgba8(const Rgba8TextureData& textureData)
{
    return impl_->CreateTextureRgba8(textureData);
}

void Renderer::DestroyTexture(const TextureHandle texture) noexcept
{
    impl_->DestroyTexture(texture);
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
