#include "SpriteGpuBackend.hpp"

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace trace2d::render::detail
{
namespace
{
constexpr std::size_t VerticesPerSprite = 6U;

struct SpriteGpuVertex final
{
    float clipX;
    float clipY;
    float u;
    float v;
};

struct alignas(16) SpriteFragmentUniform final
{
    std::array<float, 4> tint{};
    std::array<float, 4> sampleBounds{};
    std::array<float, 4> opacityAndPadding{};
};

static_assert(sizeof(SpriteFragmentUniform) == 48U);
static_assert(alignof(SpriteFragmentUniform) >= 16U);

constexpr char SpritePresentationVertexShaderHlsl[] = R"(
struct VertexInput
{
    float2 clipPosition : TEXCOORD0;
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
    output.position = float4(input.clipPosition, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}
)";

constexpr char SpritePresentationFragmentShaderHlsl[] = R"(
Texture2D SpriteTexture : register(t0, space2);
SamplerState SpriteSampler : register(s0, space2);

cbuffer SpriteAppearance : register(b0, space3)
{
    float4 Tint;
    float4 SampleBounds;
    float4 OpacityAndPadding;
};

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(FragmentInput input) : SV_Target0
{
    const float2 sampleUv = clamp(input.uv, SampleBounds.xy, SampleBounds.zw);
    const float4 sampledStraight = SpriteTexture.Sample(SpriteSampler, sampleUv);
    const float effectiveAlpha = sampledStraight.a * Tint.a * OpacityAndPadding.x;
    const float3 premultipliedRgb = sampledStraight.rgb * Tint.rgb * effectiveAlpha;
    return float4(premultipliedRgb, effectiveAlpha);
}
)";

[[nodiscard]] std::runtime_error MakeSdlError(const char* const context)
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
            throw MakeSdlError("SDL_shadercross initialization failed for Sprite SR3");
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

    std::size_t spirvSize = 0U;
    void* const spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
    if (spirv == nullptr)
    {
        throw MakeSdlError("Sprite SR3 HLSL to SPIR-V compilation failed");
    }

    SDL_ShaderCross_GraphicsShaderMetadata* const metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8*>(spirv), spirvSize, 0);
    if (metadata == nullptr)
    {
        SDL_free(spirv);
        throw MakeSdlError("Sprite SR3 SPIR-V reflection failed");
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
        throw MakeSdlError("Sprite SR3 SDL GPU shader compilation failed");
    }
    return shader;
}

[[nodiscard]] SDL_GPUBlendFactor SourceColorFactor(const SpriteBlendCompatibility blend)
{
    switch (blend)
    {
    case SpriteBlendCompatibility::Normal:
    case SpriteBlendCompatibility::Additive:
    case SpriteBlendCompatibility::Screen:
        return SDL_GPU_BLENDFACTOR_ONE;
    case SpriteBlendCompatibility::Multiply:
        return SDL_GPU_BLENDFACTOR_DST_COLOR;
    }
    throw std::invalid_argument{"Unsupported Sprite SR3 blend compatibility."};
}

[[nodiscard]] SDL_GPUBlendFactor DestinationColorFactor(const SpriteBlendCompatibility blend)
{
    switch (blend)
    {
    case SpriteBlendCompatibility::Normal:
    case SpriteBlendCompatibility::Multiply:
        return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case SpriteBlendCompatibility::Additive:
        return SDL_GPU_BLENDFACTOR_ONE;
    case SpriteBlendCompatibility::Screen:
        return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    }
    throw std::invalid_argument{"Unsupported Sprite SR3 blend compatibility."};
}

[[nodiscard]] SDL_GPUFilter ResolveFilter(const SpriteSamplerCompatibility sampler)
{
    switch (sampler)
    {
    case SpriteSamplerCompatibility::Nearest:
        return SDL_GPU_FILTER_NEAREST;
    case SpriteSamplerCompatibility::Linear:
        return SDL_GPU_FILTER_LINEAR;
    }
    throw std::invalid_argument{"Unsupported Sprite SR3 sampler compatibility."};
}

[[nodiscard]] SpriteGpuVertex BuildVertex(
    const OrthographicView& view,
    const SpriteDrawVertex& vertex) noexcept
{
    const Float2 clip = WorldToClip(view, vertex.position);
    return SpriteGpuVertex{clip.x, clip.y, vertex.uv.x, vertex.uv.y};
}

void WritePresentationVertices(
    SpriteGpuVertex* const destination,
    const OrthographicView& view,
    const SpriteDrawQuad& quad) noexcept
{
    destination[0] = BuildVertex(view, quad.topLeft);
    destination[1] = BuildVertex(view, quad.topRight);
    destination[2] = BuildVertex(view, quad.bottomRight);
    destination[3] = BuildVertex(view, quad.topLeft);
    destination[4] = BuildVertex(view, quad.bottomRight);
    destination[5] = BuildVertex(view, quad.bottomLeft);
}
} // namespace

SpriteGpuBackend::SpriteGpuBackend(
    SDL_GPUDevice* const device,
    const SDL_GPUTextureFormat colorTargetFormat)
    : device_{device}, colorTargetFormat_{colorTargetFormat}
{
    if (device_ == nullptr || colorTargetFormat_ == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        throw std::invalid_argument{"Sprite SR3 GPU backend requires a valid device and color target."};
    }

    try
    {
        CreateSamplers();
        CreatePipelines();
    }
    catch (...)
    {
        Cleanup();
        throw;
    }
}

SpriteGpuBackend::~SpriteGpuBackend()
{
    Cleanup();
}

SDL_GPUTextureFormat SpriteGpuBackend::ResolveTextureFormat(
    const SpriteTextureEncoding encoding) noexcept
{
    switch (encoding)
    {
    case SpriteTextureEncoding::Srgb:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case SpriteTextureEncoding::Linear:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

bool SpriteGpuBackend::SupportsTextureEncoding(
    SDL_GPUDevice* const device,
    const SpriteTextureEncoding encoding) noexcept
{
    if (device == nullptr)
    {
        return false;
    }
    const SDL_GPUTextureFormat format = ResolveTextureFormat(encoding);
    if (format == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        return false;
    }
    return SDL_GPUTextureSupportsFormat(
        device,
        format,
        SDL_GPU_TEXTURETYPE_2D,
        SDL_GPU_TEXTUREUSAGE_SAMPLER);
}

void SpriteGpuBackend::CreateSamplers()
{
    const auto createSampler = [this](const SpriteSamplerCompatibility sampler)
    {
        SDL_GPUSamplerCreateInfo samplerInfo{};
        const SDL_GPUFilter filter = ResolveFilter(sampler);
        samplerInfo.min_filter = filter;
        samplerInfo.mag_filter = filter;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.min_lod = 0.0F;
        samplerInfo.max_lod = 0.0F;
        samplerInfo.enable_anisotropy = false;
        samplerInfo.enable_compare = false;

        SDL_GPUSampler* const created = SDL_CreateGPUSampler(device_, &samplerInfo);
        if (created == nullptr)
        {
            throw MakeSdlError("SDL GPU Sprite SR3 sampler creation failed");
        }
        ++metrics_.samplerCreations;
        return created;
    };

    nearestSampler_ = createSampler(SpriteSamplerCompatibility::Nearest);
    linearSampler_ = createSampler(SpriteSamplerCompatibility::Linear);
}

void SpriteGpuBackend::CreatePipelines()
{
    const ShaderCrossScope shaderCrossScope{};
    SDL_GPUShader* vertexShader = nullptr;
    SDL_GPUShader* fragmentShader = nullptr;

    try
    {
        vertexShader = CompileHlslShader(
            device_,
            SpritePresentationVertexShaderHlsl,
            SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        fragmentShader = CompileHlslShader(
            device_,
            SpritePresentationFragmentShaderHlsl,
            SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

        std::array<SDL_GPUVertexBufferDescription, 1> bufferDescriptions{};
        bufferDescriptions[0].slot = 0U;
        bufferDescriptions[0].pitch = sizeof(SpriteGpuVertex);
        bufferDescriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        std::array<SDL_GPUVertexAttribute, 2> attributes{};
        attributes[0].location = 0U;
        attributes[0].buffer_slot = 0U;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[0].offset = static_cast<Uint32>(offsetof(SpriteGpuVertex, clipX));
        attributes[1].location = 1U;
        attributes[1].buffer_slot = 0U;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[1].offset = static_cast<Uint32>(offsetof(SpriteGpuVertex, u));

        const auto createPipeline = [this, vertexShader, fragmentShader, &bufferDescriptions, &attributes](
                                        const SpriteBlendCompatibility blend)
        {
            SDL_GPUColorTargetDescription colorTargetDescription{};
            colorTargetDescription.format = colorTargetFormat_;
            colorTargetDescription.blend_state.enable_blend = true;
            colorTargetDescription.blend_state.color_write_mask = 0xFU;
            colorTargetDescription.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            colorTargetDescription.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            colorTargetDescription.blend_state.src_color_blendfactor = SourceColorFactor(blend);
            colorTargetDescription.blend_state.dst_color_blendfactor = DestinationColorFactor(blend);
            colorTargetDescription.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            colorTargetDescription.blend_state.dst_alpha_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

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

            SDL_GPUGraphicsPipeline* const created =
                SDL_CreateGPUGraphicsPipeline(device_, &pipelineInfo);
            if (created == nullptr)
            {
                throw MakeSdlError("SDL GPU Sprite SR3 graphics-pipeline creation failed");
            }
            ++metrics_.pipelineCreations;
            return created;
        };

        normalPipeline_ = createPipeline(SpriteBlendCompatibility::Normal);
        additivePipeline_ = createPipeline(SpriteBlendCompatibility::Additive);
        multiplyPipeline_ = createPipeline(SpriteBlendCompatibility::Multiply);
        screenPipeline_ = createPipeline(SpriteBlendCompatibility::Screen);
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

void SpriteGpuBackend::EnsureVertexCapacity(const std::size_t requiredSprites)
{
    if (requiredSprites == 0U || requiredSprites <= vertexCapacitySprites_)
    {
        return;
    }

    constexpr std::uint64_t BytesPerSprite =
        static_cast<std::uint64_t>(VerticesPerSprite * sizeof(SpriteGpuVertex));
    constexpr std::uint64_t MaxSprites =
        static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()) / BytesPerSprite;
    if (static_cast<std::uint64_t>(requiredSprites) > MaxSprites)
    {
        throw std::length_error{"Sprite SR3 vertex upload exceeds SDL GPU buffer limits."};
    }

    std::uint64_t replacementCapacity = vertexCapacitySprites_ == 0U ? 1U : vertexCapacitySprites_;
    while (replacementCapacity < static_cast<std::uint64_t>(requiredSprites))
    {
        if (replacementCapacity > MaxSprites / 2U)
        {
            replacementCapacity = MaxSprites;
            break;
        }
        replacementCapacity *= 2U;
    }

    const Uint32 replacementBytes = static_cast<Uint32>(replacementCapacity * BytesPerSprite);

    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = replacementBytes;
    SDL_GPUBuffer* const replacementBuffer = SDL_CreateGPUBuffer(device_, &bufferInfo);
    if (replacementBuffer == nullptr)
    {
        throw MakeSdlError("SDL GPU Sprite SR3 vertex-buffer creation failed");
    }

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = replacementBytes;
    SDL_GPUTransferBuffer* const replacementTransfer =
        SDL_CreateGPUTransferBuffer(device_, &transferInfo);
    if (replacementTransfer == nullptr)
    {
        SDL_ReleaseGPUBuffer(device_, replacementBuffer);
        throw MakeSdlError("SDL GPU Sprite SR3 transfer-buffer creation failed");
    }

    if (vertexTransferBuffer_ != nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(device_, vertexTransferBuffer_);
    }
    if (vertexBuffer_ != nullptr)
    {
        SDL_ReleaseGPUBuffer(device_, vertexBuffer_);
    }

    vertexBuffer_ = replacementBuffer;
    vertexTransferBuffer_ = replacementTransfer;
    vertexCapacitySprites_ = static_cast<std::size_t>(replacementCapacity);
    metrics_.vertexCapacitySprites = replacementCapacity;
}

void SpriteGpuBackend::UploadPresentations(
    SDL_GPUCommandBuffer* const commandBuffer,
    const OrthographicView& view,
    const std::span<const SpritePresentationRenderData> presentations)
{
    if (presentations.empty())
    {
        return;
    }
    if (commandBuffer == nullptr)
    {
        throw std::invalid_argument{"Sprite SR3 upload requires a command buffer."};
    }

    EnsureVertexCapacity(presentations.size());
    void* const mapped = SDL_MapGPUTransferBuffer(device_, vertexTransferBuffer_, true);
    if (mapped == nullptr)
    {
        throw MakeSdlError("SDL GPU Sprite SR3 transfer-buffer mapping failed");
    }

    auto* const vertices = static_cast<SpriteGpuVertex*>(mapped);
    for (std::size_t index = 0U; index < presentations.size(); ++index)
    {
        WritePresentationVertices(
            vertices + index * VerticesPerSprite,
            view,
            presentations[index].presentation.quad);
    }
    SDL_UnmapGPUTransferBuffer(device_, vertexTransferBuffer_);

    SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (copyPass == nullptr)
    {
        throw MakeSdlError("SDL GPU Sprite SR3 upload copy-pass creation failed");
    }

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = vertexTransferBuffer_;
    SDL_GPUBufferRegion destination{};
    destination.buffer = vertexBuffer_;
    destination.size = static_cast<Uint32>(
        presentations.size() * VerticesPerSprite * sizeof(SpriteGpuVertex));
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);
    SDL_EndGPUCopyPass(copyPass);
}

void SpriteGpuBackend::DrawPresentation(
    SDL_GPUCommandBuffer* const commandBuffer,
    SDL_GPURenderPass* const renderPass,
    SDL_GPUTexture* const texture,
    const SpritePresentationRenderData& presentation,
    const std::size_t presentationIndex)
{
    if (commandBuffer == nullptr || renderPass == nullptr || texture == nullptr)
    {
        throw std::invalid_argument{"Sprite SR3 draw requires live GPU command/render/texture state."};
    }
    if (presentationIndex >= vertexCapacitySprites_)
    {
        throw std::out_of_range{"Sprite SR3 draw index exceeds uploaded vertex capacity."};
    }

    SDL_GPUSampler* sampler = nullptr;
    switch (presentation.presentation.appearance.sampler)
    {
    case SpriteSamplerCompatibility::Nearest:
        sampler = nearestSampler_;
        break;
    case SpriteSamplerCompatibility::Linear:
        sampler = linearSampler_;
        break;
    default:
        throw std::invalid_argument{"Unsupported Sprite SR3 sampler compatibility."};
    }

    SDL_GPUGraphicsPipeline* pipeline = nullptr;
    switch (presentation.presentation.appearance.blend)
    {
    case SpriteBlendCompatibility::Normal:
        pipeline = normalPipeline_;
        break;
    case SpriteBlendCompatibility::Additive:
        pipeline = additivePipeline_;
        break;
    case SpriteBlendCompatibility::Multiply:
        pipeline = multiplyPipeline_;
        break;
    case SpriteBlendCompatibility::Screen:
        pipeline = screenPipeline_;
        break;
    default:
        throw std::invalid_argument{"Unsupported Sprite SR3 blend compatibility."};
    }

    const SpriteAppearanceContractData& appearance = presentation.presentation.appearance;
    const SpriteFragmentUniform uniform{
        std::array<float, 4>{
            appearance.tint.red,
            appearance.tint.green,
            appearance.tint.blue,
            appearance.tint.alpha,
        },
        std::array<float, 4>{
            appearance.sampleBounds.minimum.x,
            appearance.sampleBounds.minimum.y,
            appearance.sampleBounds.maximum.x,
            appearance.sampleBounds.maximum.y,
        },
        std::array<float, 4>{appearance.opacity, 0.0F, 0.0F, 0.0F},
    };

    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = vertexBuffer_;
    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
    SDL_BindGPUVertexBuffers(renderPass, 0U, &vertexBinding, 1U);

    SDL_GPUTextureSamplerBinding textureBinding{};
    textureBinding.texture = texture;
    textureBinding.sampler = sampler;
    SDL_BindGPUFragmentSamplers(renderPass, 0U, &textureBinding, 1U);
    SDL_PushGPUFragmentUniformData(
        commandBuffer,
        0U,
        &uniform,
        static_cast<Uint32>(sizeof(uniform)));

    const std::uint64_t firstVertex64 =
        static_cast<std::uint64_t>(presentationIndex) * VerticesPerSprite;
    if (firstVertex64 > static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()))
    {
        throw std::length_error{"Sprite SR3 first vertex exceeds SDL GPU draw limits."};
    }
    SDL_DrawGPUPrimitives(
        renderPass,
        static_cast<Uint32>(VerticesPerSprite),
        1U,
        static_cast<Uint32>(firstVertex64),
        0U);
}

const SpriteGpuBackendMetrics& SpriteGpuBackend::Metrics() const noexcept
{
    return metrics_;
}

void SpriteGpuBackend::Cleanup() noexcept
{
    if (device_ == nullptr)
    {
        return;
    }

    if (vertexTransferBuffer_ != nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(device_, vertexTransferBuffer_);
        vertexTransferBuffer_ = nullptr;
    }
    if (vertexBuffer_ != nullptr)
    {
        SDL_ReleaseGPUBuffer(device_, vertexBuffer_);
        vertexBuffer_ = nullptr;
        vertexCapacitySprites_ = 0U;
    }
    if (screenPipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, screenPipeline_);
        screenPipeline_ = nullptr;
    }
    if (multiplyPipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, multiplyPipeline_);
        multiplyPipeline_ = nullptr;
    }
    if (additivePipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, additivePipeline_);
        additivePipeline_ = nullptr;
    }
    if (normalPipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, normalPipeline_);
        normalPipeline_ = nullptr;
    }
    if (linearSampler_ != nullptr)
    {
        SDL_ReleaseGPUSampler(device_, linearSampler_);
        linearSampler_ = nullptr;
    }
    if (nearestSampler_ != nullptr)
    {
        SDL_ReleaseGPUSampler(device_, nearestSampler_);
        nearestSampler_ = nullptr;
    }
}
} // namespace trace2d::render::detail
