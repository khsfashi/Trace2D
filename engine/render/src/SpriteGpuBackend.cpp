#include "SpriteGpuBackend.hpp"

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace trace2d::render::detail
{
namespace
{
constexpr std::size_t VerticesPerQuad = 6U;
constexpr std::size_t BlendCompatibilityCount = 4U;
constexpr std::size_t NoQuadOffset = std::numeric_limits<std::size_t>::max();

struct SpriteGpuVertex final
{
    float clipX;
    float clipY;
    float u;
    float v;
    float sampleMinU;
    float sampleMinV;
    float sampleMaxU;
    float sampleMaxV;
    float tintRed;
    float tintGreen;
    float tintBlue;
    float tintAlphaTimesOpacity;
};

static_assert(sizeof(SpriteGpuVertex) == 48U);
static_assert(BlendCompatibilityCount == 4U);

constexpr char SpritePresentationVertexShaderHlsl[] = R"(
struct VertexInput
{
    float2 clipPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 sampleBounds : TEXCOORD2;
    float4 tint : TEXCOORD3;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 sampleBounds : TEXCOORD1;
    float4 tint : TEXCOORD2;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.clipPosition, 0.0, 1.0);
    output.uv = input.uv;
    output.sampleBounds = input.sampleBounds;
    output.tint = input.tint;
    return output;
}
)";

constexpr char SpritePresentationFragmentShaderHlsl[] = R"(
Texture2D SpriteTexture : register(t0, space2);
SamplerState SpriteSampler : register(s0, space2);

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 sampleBounds : TEXCOORD1;
    float4 tint : TEXCOORD2;
};

float4 main(FragmentInput input) : SV_Target0
{
    const float2 sampleUv = clamp(input.uv, input.sampleBounds.xy, input.sampleBounds.zw);
    const float4 sampledStraight = SpriteTexture.Sample(SpriteSampler, sampleUv);
    const float effectiveAlpha = sampledStraight.a * input.tint.a;
    const float3 premultipliedRgb = sampledStraight.rgb * input.tint.rgb * effectiveAlpha;
    return float4(premultipliedRgb, effectiveAlpha);
}
)";

constexpr char SpriteMaskWriterFragmentShaderHlsl[] = R"(
Texture2D SpriteTexture : register(t0, space2);
SamplerState SpriteSampler : register(s0, space2);

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 sampleBounds : TEXCOORD1;
    float4 tint : TEXCOORD2;
};

float4 main(FragmentInput input) : SV_Target0
{
    const float2 sampleUv = clamp(input.uv, input.sampleBounds.xy, input.sampleBounds.zw);
    const float4 sampledStraight = SpriteTexture.Sample(SpriteSampler, sampleUv);
    const float effectiveAlpha = sampledStraight.a * input.tint.a;
    clip(effectiveAlpha - 0.5);
    return float4(0.0, 0.0, 0.0, 0.0);
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
            throw MakeSdlError("SDL_shadercross initialization failed for Sprite SR7");
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
        throw MakeSdlError("Sprite SR7 HLSL to SPIR-V compilation failed");
    }

    SDL_ShaderCross_GraphicsShaderMetadata* const metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8*>(spirv), spirvSize, 0);
    if (metadata == nullptr)
    {
        SDL_free(spirv);
        throw MakeSdlError("Sprite SR7 SPIR-V reflection failed");
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
        throw MakeSdlError("Sprite SR7 SDL GPU shader compilation failed");
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
    throw std::invalid_argument{"Unsupported Sprite SR7 blend compatibility."};
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
    throw std::invalid_argument{"Unsupported Sprite SR7 blend compatibility."};
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
    throw std::invalid_argument{"Unsupported Sprite SR7 sampler compatibility."};
}

[[nodiscard]] std::size_t BlendIndex(const SpriteBlendCompatibility blend)
{
    switch (blend)
    {
    case SpriteBlendCompatibility::Normal:
        return 0U;
    case SpriteBlendCompatibility::Additive:
        return 1U;
    case SpriteBlendCompatibility::Multiply:
        return 2U;
    case SpriteBlendCompatibility::Screen:
        return 3U;
    }
    throw std::invalid_argument{"Unsupported Sprite SR7 blend compatibility."};
}

[[nodiscard]] SDL_GPUTextureFormat ResolveDepthStencilTargetFormat(SDL_GPUDevice* const device)
{
    constexpr std::array<SDL_GPUTextureFormat, 2U> Candidates{
        SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
    };
    for (const SDL_GPUTextureFormat format : Candidates)
    {
        if (SDL_GPUTextureSupportsFormat(
                device,
                format,
                SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        {
            return format;
        }
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

[[nodiscard]] std::size_t PresentationQuadCount(
    const SpritePresentationRenderData& presentation)
{
    switch (presentation.geometryKind)
    {
    case SpritePresentationGeometryKind::Quad:
        return 1U;
    case SpritePresentationGeometryKind::PrimitivePatches:
        return presentation.primitivePatches.size();
    }
    throw std::invalid_argument{"Unsupported Sprite SR7 presentation geometry kind."};
}

[[nodiscard]] bool PresentationVisible(
    const OrthographicView& view,
    const SpritePresentationRenderData& presentation) noexcept
{
    switch (presentation.geometryKind)
    {
    case SpritePresentationGeometryKind::Quad:
        return IsSpritePresentationQuadVisible(view, presentation.presentation.quad);
    case SpritePresentationGeometryKind::PrimitivePatches:
        return IsSpritePresentationPrimitiveVisible(view, presentation.primitivePatches);
    }
    return false;
}

[[nodiscard]] SpriteBatchCompatibility2D CompatibilityOf(
    const SpritePresentationRenderData& presentation) noexcept
{
    return SpriteBatchCompatibility2D{
        presentation.texture,
        presentation.materialPipeline,
        presentation.presentation.appearance.sampler,
        presentation.presentation.appearance.blend,
        presentation.mask,
    };
}

[[nodiscard]] SpriteGpuVertex BuildVertex(
    const OrthographicView& view,
    const SpriteDrawVertex& vertex,
    const SpriteSampleBounds& sampleBounds,
    const SpriteAppearanceContractData& appearance) noexcept
{
    const Float2 clip = WorldToClip(view, vertex.position);
    return SpriteGpuVertex{
        clip.x,
        clip.y,
        vertex.uv.x,
        vertex.uv.y,
        sampleBounds.minimum.x,
        sampleBounds.minimum.y,
        sampleBounds.maximum.x,
        sampleBounds.maximum.y,
        appearance.tint.red,
        appearance.tint.green,
        appearance.tint.blue,
        appearance.tint.alpha * appearance.opacity,
    };
}

void WritePresentationVertices(
    SpriteGpuVertex* const destination,
    const OrthographicView& view,
    const SpriteDrawQuad& quad,
    const SpriteSampleBounds& sampleBounds,
    const SpriteAppearanceContractData& appearance) noexcept
{
    destination[0] = BuildVertex(view, quad.topLeft, sampleBounds, appearance);
    destination[1] = BuildVertex(view, quad.topRight, sampleBounds, appearance);
    destination[2] = BuildVertex(view, quad.bottomRight, sampleBounds, appearance);
    destination[3] = BuildVertex(view, quad.topLeft, sampleBounds, appearance);
    destination[4] = BuildVertex(view, quad.bottomRight, sampleBounds, appearance);
    destination[5] = BuildVertex(view, quad.bottomLeft, sampleBounds, appearance);
}

[[nodiscard]] const char* OrderMaskErrorMessage(const SpriteOrderMaskError error) noexcept
{
    switch (error)
    {
    case SpriteOrderMaskError::None:
        return "Sprite SR7 order/mask resolution unexpectedly reported success.";
    case SpriteOrderMaskError::InvalidSourceIndex:
        return "Sprite SR7 order scratch contains an invalid source index.";
    case SpriteOrderMaskError::InvalidStableOrder:
        return "Sprite SR7 order contains the reserved invalid stable order.";
    case SpriteOrderMaskError::InvalidSortingGroup:
        return "Sprite SR7 sorting-group state is malformed.";
    case SpriteOrderMaskError::InconsistentSortingGroup:
        return "Sprite SR7 sorting-group identity resolves to inconsistent anchors.";
    case SpriteOrderMaskError::InvalidMask:
        return "Sprite SR7 mask state is malformed.";
    case SpriteOrderMaskError::MaskTesterWithoutWriter:
        return "Sprite SR7 mask tester has no active preceding writer.";
    case SpriteOrderMaskError::MaskWriterAfterTester:
        return "Sprite SR7 mask writer appears after a tester in the same mask phase.";
    case SpriteOrderMaskError::MaskPhaseReentry:
        return "Sprite SR7 mask phase re-enters an identity after another writer replaced it.";
    }
    return "Sprite SR7 order/mask resolution failed.";
}
} // namespace

SpriteGpuBackend::SpriteGpuBackend(
    SDL_GPUDevice* const device,
    const SDL_GPUTextureFormat colorTargetFormat)
    : device_{device}, colorTargetFormat_{colorTargetFormat}
{
    if (device_ == nullptr || colorTargetFormat_ == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        throw std::invalid_argument{"Sprite SR7 GPU backend requires a valid device and color target."};
    }

    depthStencilTargetFormat_ = ResolveDepthStencilTargetFormat(device_);
    if (depthStencilTargetFormat_ == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        throw std::runtime_error{
            "SDL GPU backend does not expose a Sprite SR7 stencil-capable depth target format."};
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
            throw MakeSdlError("SDL GPU Sprite SR7 sampler creation failed");
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
    SDL_GPUShader* maskWriterFragmentShader = nullptr;

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
        maskWriterFragmentShader = CompileHlslShader(
            device_,
            SpriteMaskWriterFragmentShaderHlsl,
            SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

        std::array<SDL_GPUVertexBufferDescription, 1U> bufferDescriptions{};
        bufferDescriptions[0].slot = 0U;
        bufferDescriptions[0].pitch = sizeof(SpriteGpuVertex);
        bufferDescriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        std::array<SDL_GPUVertexAttribute, 4U> attributes{};
        attributes[0].location = 0U;
        attributes[0].buffer_slot = 0U;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[0].offset = static_cast<Uint32>(offsetof(SpriteGpuVertex, clipX));
        attributes[1].location = 1U;
        attributes[1].buffer_slot = 0U;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[1].offset = static_cast<Uint32>(offsetof(SpriteGpuVertex, u));
        attributes[2].location = 2U;
        attributes[2].buffer_slot = 0U;
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attributes[2].offset = static_cast<Uint32>(offsetof(SpriteGpuVertex, sampleMinU));
        attributes[3].location = 3U;
        attributes[3].buffer_slot = 0U;
        attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attributes[3].offset = static_cast<Uint32>(offsetof(SpriteGpuVertex, tintRed));

        const auto createPipeline =
            [this,
             vertexShader,
             fragmentShader,
             maskWriterFragmentShader,
             &bufferDescriptions,
             &attributes](
                const SpriteBlendCompatibility blend,
                const SpriteMaskMode maskMode,
                const bool hasStencilTarget)
        {
            SDL_GPUColorTargetDescription colorTargetDescription{};
            colorTargetDescription.format = colorTargetFormat_;
            colorTargetDescription.blend_state.enable_color_write_mask = true;

            if (maskMode == SpriteMaskMode::Write)
            {
                colorTargetDescription.blend_state.enable_blend = false;
                colorTargetDescription.blend_state.color_write_mask = 0U;
            }
            else
            {
                colorTargetDescription.blend_state.enable_blend = true;
                colorTargetDescription.blend_state.color_write_mask = 0xFU;
                colorTargetDescription.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
                colorTargetDescription.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
                colorTargetDescription.blend_state.src_color_blendfactor = SourceColorFactor(blend);
                colorTargetDescription.blend_state.dst_color_blendfactor =
                    DestinationColorFactor(blend);
                colorTargetDescription.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                colorTargetDescription.blend_state.dst_alpha_blendfactor =
                    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            }

            SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.vertex_shader = vertexShader;
            pipelineInfo.fragment_shader = maskMode == SpriteMaskMode::Write
                ? maskWriterFragmentShader
                : fragmentShader;
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
            pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;

            if (hasStencilTarget)
            {
                pipelineInfo.target_info.depth_stencil_format = depthStencilTargetFormat_;
                pipelineInfo.target_info.has_depth_stencil_target = true;
            }

            if (maskMode != SpriteMaskMode::None)
            {
                SDL_GPUStencilOpState stencilState{};
                stencilState.fail_op = SDL_GPU_STENCILOP_KEEP;
                stencilState.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
                stencilState.pass_op = maskMode == SpriteMaskMode::Write
                    ? SDL_GPU_STENCILOP_REPLACE
                    : SDL_GPU_STENCILOP_KEEP;
                switch (maskMode)
                {
                case SpriteMaskMode::Write:
                    stencilState.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
                    break;
                case SpriteMaskMode::TestInside:
                    stencilState.compare_op = SDL_GPU_COMPAREOP_EQUAL;
                    break;
                case SpriteMaskMode::TestOutside:
                    stencilState.compare_op = SDL_GPU_COMPAREOP_NOT_EQUAL;
                    break;
                case SpriteMaskMode::None:
                    break;
                }

                pipelineInfo.depth_stencil_state.front_stencil_state = stencilState;
                pipelineInfo.depth_stencil_state.back_stencil_state = stencilState;
                pipelineInfo.depth_stencil_state.compare_mask = 0xFFU;
                pipelineInfo.depth_stencil_state.write_mask =
                    maskMode == SpriteMaskMode::Write ? 0xFFU : 0U;
                pipelineInfo.depth_stencil_state.enable_stencil_test = true;
            }

            SDL_GPUGraphicsPipeline* const created =
                SDL_CreateGPUGraphicsPipeline(device_, &pipelineInfo);
            if (created == nullptr)
            {
                throw MakeSdlError("SDL GPU Sprite SR7 graphics-pipeline creation failed");
            }
            ++metrics_.pipelineCreations;
            return created;
        };

        constexpr std::array<SpriteBlendCompatibility, BlendCompatibilityCount> Blends{
            SpriteBlendCompatibility::Normal,
            SpriteBlendCompatibility::Additive,
            SpriteBlendCompatibility::Multiply,
            SpriteBlendCompatibility::Screen,
        };
        for (const SpriteBlendCompatibility blend : Blends)
        {
            const std::size_t index = BlendIndex(blend);
            unmaskedPipelines_[index] = createPipeline(blend, SpriteMaskMode::None, false);
            stencilCompatibleUnmaskedPipelines_[index] =
                createPipeline(blend, SpriteMaskMode::None, true);
            maskInsidePipelines_[index] =
                createPipeline(blend, SpriteMaskMode::TestInside, true);
            maskOutsidePipelines_[index] =
                createPipeline(blend, SpriteMaskMode::TestOutside, true);
        }
        maskWritePipeline_ =
            createPipeline(SpriteBlendCompatibility::Normal, SpriteMaskMode::Write, true);
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
        if (maskWriterFragmentShader != nullptr)
        {
            SDL_ReleaseGPUShader(device_, maskWriterFragmentShader);
        }
        throw;
    }

    SDL_ReleaseGPUShader(device_, vertexShader);
    SDL_ReleaseGPUShader(device_, fragmentShader);
    SDL_ReleaseGPUShader(device_, maskWriterFragmentShader);
}

void SpriteGpuBackend::EnsureVertexCapacity(const std::size_t requiredQuadSlots)
{
    if (requiredQuadSlots == 0U || requiredQuadSlots <= vertexCapacitySprites_)
    {
        return;
    }

    constexpr std::uint64_t BytesPerQuad =
        static_cast<std::uint64_t>(VerticesPerQuad * sizeof(SpriteGpuVertex));
    constexpr std::uint64_t MaxQuadSlots =
        static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()) / BytesPerQuad;
    if (static_cast<std::uint64_t>(requiredQuadSlots) > MaxQuadSlots)
    {
        throw std::length_error{"Sprite SR7 vertex upload exceeds SDL GPU buffer limits."};
    }

    std::uint64_t replacementCapacity = vertexCapacitySprites_ == 0U ? 1U : vertexCapacitySprites_;
    while (replacementCapacity < static_cast<std::uint64_t>(requiredQuadSlots))
    {
        if (replacementCapacity > MaxQuadSlots / 2U)
        {
            replacementCapacity = MaxQuadSlots;
            break;
        }
        replacementCapacity *= 2U;
    }

    const Uint32 replacementBytes = static_cast<Uint32>(replacementCapacity * BytesPerQuad);

    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = replacementBytes;
    SDL_GPUBuffer* const replacementBuffer = SDL_CreateGPUBuffer(device_, &bufferInfo);
    if (replacementBuffer == nullptr)
    {
        throw MakeSdlError("SDL GPU Sprite SR7 vertex-buffer creation failed");
    }

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = replacementBytes;
    SDL_GPUTransferBuffer* const replacementTransfer =
        SDL_CreateGPUTransferBuffer(device_, &transferInfo);
    if (replacementTransfer == nullptr)
    {
        SDL_ReleaseGPUBuffer(device_, replacementBuffer);
        throw MakeSdlError("SDL GPU Sprite SR7 transfer-buffer creation failed");
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
    metrics_.vertexCapacityBytes = replacementBytes;
}

void SpriteGpuBackend::EnsureMaskTarget(
    const std::uint32_t width,
    const std::uint32_t height)
{
    if (width == 0U || height == 0U)
    {
        throw std::invalid_argument{"Sprite SR7 mask target dimensions must be non-zero."};
    }
    if (maskTarget_ != nullptr && maskTargetWidth_ == width && maskTargetHeight_ == height)
    {
        return;
    }

    SDL_GPUTextureCreateInfo textureInfo{};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = depthStencilTargetFormat_;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    textureInfo.width = width;
    textureInfo.height = height;
    textureInfo.layer_count_or_depth = 1U;
    textureInfo.num_levels = 1U;
    textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture* const replacement = SDL_CreateGPUTexture(device_, &textureInfo);
    if (replacement == nullptr)
    {
        throw MakeSdlError("SDL GPU Sprite SR7 mask target creation failed");
    }

    if (maskTarget_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, maskTarget_);
    }
    maskTarget_ = replacement;
    maskTargetWidth_ = width;
    maskTargetHeight_ = height;
    ++metrics_.maskTargetCreations;
}

void SpriteGpuBackend::UploadPresentations(
    SDL_GPUCommandBuffer* const commandBuffer,
    const OrthographicView& view,
    const std::span<const SpritePresentationRenderData> presentations)
{
    maskingRequired_ = false;
    pixelPerfectViewport_ = SpritePixelPerfectViewport2D{};
    pixelPerfectViewportEnabled_ = false;
    orderScratch_.clear();
    patchOffsetScratch_.clear();
    batchQuadCountScratch_.clear();
    metrics_.lastSubmittedSprites = presentations.size();
    metrics_.lastVisibleSprites = 0U;
    metrics_.lastCulledSprites = 0U;
    metrics_.lastUploadedQuads = 0U;
    metrics_.lastUploadedVertexBytes = 0U;
    metrics_.lastCompatibilityRuns = 0U;

    if (presentations.empty())
    {
        return;
    }
    if (commandBuffer == nullptr)
    {
        throw std::invalid_argument{"Sprite SR7 upload requires a command buffer."};
    }
    if (presentations.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::length_error{"Sprite SR7 presentation count exceeds semantic source-index range."};
    }

    const SpritePixelPerfectViewport2D* const requestedViewport =
        presentations.front().pixelPerfectViewport;
    if (requestedViewport != nullptr)
    {
        const SpritePixelPerfectStatus status = ValidateSpritePixelPerfectViewport(*requestedViewport);
        if (!status.Succeeded())
        {
            throw std::invalid_argument{
                "Sprite SR7 presentation contains an invalid pixel-perfect viewport."};
        }
        pixelPerfectViewport_ = *requestedViewport;
        pixelPerfectViewportEnabled_ = true;
    }

    orderScratch_.resize(presentations.size());
    patchOffsetScratch_.assign(presentations.size(), NoQuadOffset);
    batchQuadCountScratch_.assign(presentations.size(), 0U);

    for (std::size_t index = 0U; index < presentations.size(); ++index)
    {
        const SpritePresentationRenderData& presentation = presentations[index];
        const SpritePixelPerfectViewport2D* const presentationViewport =
            presentation.pixelPerfectViewport;
        if (pixelPerfectViewportEnabled_)
        {
            if (presentationViewport == nullptr || *presentationViewport != pixelPerfectViewport_)
            {
                throw std::invalid_argument{
                    "Sprite SR7 frame requires one equal pixel-perfect viewport for every Sprite presentation."};
            }
            if (presentation.presentation.appearance.sampler != SpriteSamplerCompatibility::Nearest)
            {
                throw std::invalid_argument{
                    "Sprite SR7 exact pixel-perfect presentation requires nearest sampling."};
            }
        }
        else if (presentationViewport != nullptr)
        {
            throw std::invalid_argument{
                "Sprite SR7 frame cannot mix pixel-perfect and non-pixel-perfect Sprite presentations."};
        }

        if (presentation.materialPipeline != BuiltInSpriteMaterialPipelineIdentity)
        {
            throw std::invalid_argument{
                "Sprite SR7 only executes the built-in material/pipeline identity; #89 owns programmable materials."};
        }

        const std::size_t quadCount = PresentationQuadCount(presentation);
        if (quadCount > MaximumSpritePrimitiveQuads)
        {
            throw std::length_error{"Sprite SR7 primitive exceeds the per-Sprite expansion limit."};
        }

        orderScratch_[index] = SpriteOrderMaskEntry2D{
            presentation.order,
            presentation.mask,
            static_cast<std::uint32_t>(index),
        };
    }

    const SpriteOrderMaskStatus orderStatus = ResolveSpriteOrderMask2D(orderScratch_);
    if (!orderStatus.Succeeded())
    {
        throw std::invalid_argument{OrderMaskErrorMessage(orderStatus.error)};
    }

    const OrthographicView& presentationView =
        pixelPerfectViewportEnabled_ ? pixelPerfectViewport_.logicalView : view;

    bool hasRun = false;
    SpriteBatchCompatibility2D runCompatibility{};
    std::size_t runStartSourceIndex = 0U;
    std::size_t runQuadCount = 0U;
    std::size_t requiredQuadSlots = 0U;

    const auto flushRun = [&]()
    {
        if (!hasRun)
        {
            return;
        }
        batchQuadCountScratch_[runStartSourceIndex] = runQuadCount;
    };

    for (const SpriteOrderMaskEntry2D& orderedEntry : orderScratch_)
    {
        const std::size_t sourceIndex = static_cast<std::size_t>(orderedEntry.sourceIndex);
        const SpritePresentationRenderData& presentation = presentations[sourceIndex];
        const std::size_t quadCount = PresentationQuadCount(presentation);
        const bool visible = quadCount != 0U && PresentationVisible(presentationView, presentation);
        if (!visible)
        {
            ++metrics_.lastCulledSprites;
            continue;
        }
        if (requiredQuadSlots > std::numeric_limits<std::size_t>::max() - quadCount)
        {
            throw std::length_error{"Sprite SR7 visible primitive quad count overflows host size_t."};
        }

        const SpriteBatchCompatibility2D compatibility = CompatibilityOf(presentation);
        if (!hasRun || !(compatibility == runCompatibility))
        {
            flushRun();
            runCompatibility = compatibility;
            runStartSourceIndex = sourceIndex;
            runQuadCount = 0U;
            hasRun = true;
            ++metrics_.lastCompatibilityRuns;
        }

        patchOffsetScratch_[sourceIndex] = requiredQuadSlots;
        requiredQuadSlots += quadCount;
        runQuadCount += quadCount;
        ++metrics_.lastVisibleSprites;
        maskingRequired_ = maskingRequired_ || presentation.mask.mode != SpriteMaskMode::None;
    }
    flushRun();

    metrics_.lastUploadedQuads = requiredQuadSlots;
    metrics_.lastUploadedVertexBytes =
        static_cast<std::uint64_t>(requiredQuadSlots) * VerticesPerQuad * sizeof(SpriteGpuVertex);

    if (requiredQuadSlots == 0U)
    {
        return;
    }

    EnsureVertexCapacity(requiredQuadSlots);
    void* const mapped = SDL_MapGPUTransferBuffer(device_, vertexTransferBuffer_, true);
    if (mapped == nullptr)
    {
        throw MakeSdlError("SDL GPU Sprite SR7 transfer-buffer mapping failed");
    }

    auto* const vertices = static_cast<SpriteGpuVertex*>(mapped);
    std::size_t writtenQuadSlots = 0U;
    for (const SpriteOrderMaskEntry2D& orderedEntry : orderScratch_)
    {
        const std::size_t sourceIndex = static_cast<std::size_t>(orderedEntry.sourceIndex);
        if (patchOffsetScratch_[sourceIndex] == NoQuadOffset)
        {
            continue;
        }

        const SpritePresentationRenderData& presentation = presentations[sourceIndex];
        if (patchOffsetScratch_[sourceIndex] != writtenQuadSlots)
        {
            SDL_UnmapGPUTransferBuffer(device_, vertexTransferBuffer_);
            throw std::logic_error{"Sprite SR7 compacted visible-quad offsets are inconsistent."};
        }

        const SpriteAppearanceContractData& appearance = presentation.presentation.appearance;
        switch (presentation.geometryKind)
        {
        case SpritePresentationGeometryKind::Quad:
            WritePresentationVertices(
                vertices + writtenQuadSlots * VerticesPerQuad,
                presentationView,
                presentation.presentation.quad,
                appearance.sampleBounds,
                appearance);
            ++writtenQuadSlots;
            break;
        case SpritePresentationGeometryKind::PrimitivePatches:
            for (const SpritePrimitivePatch2D& patch : presentation.primitivePatches)
            {
                WritePresentationVertices(
                    vertices + writtenQuadSlots * VerticesPerQuad,
                    presentationView,
                    patch.quad,
                    patch.sampleBounds,
                    appearance);
                ++writtenQuadSlots;
            }
            break;
        default:
            SDL_UnmapGPUTransferBuffer(device_, vertexTransferBuffer_);
            throw std::invalid_argument{"Unsupported Sprite SR7 presentation geometry kind."};
        }
    }
    SDL_UnmapGPUTransferBuffer(device_, vertexTransferBuffer_);

    if (writtenQuadSlots != requiredQuadSlots)
    {
        throw std::logic_error{"Sprite SR7 upload emitted an inconsistent visible primitive quad count."};
    }

    SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (copyPass == nullptr)
    {
        throw MakeSdlError("SDL GPU Sprite SR7 upload copy-pass creation failed");
    }

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = vertexTransferBuffer_;
    SDL_GPUBufferRegion destination{};
    destination.buffer = vertexBuffer_;
    destination.size = static_cast<Uint32>(metrics_.lastUploadedVertexBytes);
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);
    SDL_EndGPUCopyPass(copyPass);
}

void SpriteGpuBackend::ApplyPixelPerfectRasterState(
    SDL_GPURenderPass* const renderPass,
    const std::uint32_t,
    const std::uint32_t) const
{
    if (!pixelPerfectViewportEnabled_ || renderPass == nullptr)
    {
        return;
    }

    const SpritePixelRect2D& rect = pixelPerfectViewport_.contentRect;
    SDL_GPUViewport gpuViewport{};
    gpuViewport.x = static_cast<float>(rect.x);
    gpuViewport.y = static_cast<float>(rect.y);
    gpuViewport.w = static_cast<float>(rect.width);
    gpuViewport.h = static_cast<float>(rect.height);
    gpuViewport.min_depth = 0.0F;
    gpuViewport.max_depth = 1.0F;
    SDL_SetGPUViewport(renderPass, &gpuViewport);

    const SDL_Rect scissor{
        static_cast<int>(rect.x),
        static_cast<int>(rect.y),
        static_cast<int>(rect.width),
        static_cast<int>(rect.height),
    };
    SDL_SetGPUScissor(renderPass, &scissor);
}

SDL_GPURenderPass* SpriteGpuBackend::BeginPresentationRenderPass(
    SDL_GPUCommandBuffer* const commandBuffer,
    const SDL_GPUColorTargetInfo& colorTarget,
    const std::uint32_t targetWidth,
    const std::uint32_t targetHeight)
{
    if (commandBuffer == nullptr || colorTarget.texture == nullptr)
    {
        throw std::invalid_argument{
            "Sprite SR7 render pass requires live command/color-target state."};
    }
    if (pixelPerfectViewportEnabled_)
    {
        if (targetWidth != pixelPerfectViewport_.targetWidth ||
            targetHeight != pixelPerfectViewport_.targetHeight)
        {
            throw std::invalid_argument{
                "Sprite SR7 pixel-perfect viewport is stale for the acquired presentation target."};
        }
        if (!ValidateSpritePixelPerfectViewport(pixelPerfectViewport_).Succeeded())
        {
            throw std::invalid_argument{
                "Sprite SR7 pixel-perfect viewport failed backend validation."};
        }
    }

    SDL_GPURenderPass* renderPass = nullptr;
    if (!maskingRequired_)
    {
        renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1U, nullptr);
    }
    else
    {
        EnsureMaskTarget(targetWidth, targetHeight);

        SDL_GPUDepthStencilTargetInfo depthStencilTarget{};
        depthStencilTarget.texture = maskTarget_;
        depthStencilTarget.clear_depth = 1.0F;
        depthStencilTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthStencilTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthStencilTarget.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
        depthStencilTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthStencilTarget.cycle = true;
        depthStencilTarget.clear_stencil = 0U;

        renderPass = SDL_BeginGPURenderPass(
            commandBuffer,
            &colorTarget,
            1U,
            &depthStencilTarget);
    }

    if (renderPass != nullptr)
    {
        ApplyPixelPerfectRasterState(renderPass, targetWidth, targetHeight);
    }
    return renderPass;
}

std::size_t SpriteGpuBackend::OrderedSourceIndex(const std::size_t orderedIndex) const
{
    if (orderedIndex >= orderScratch_.size())
    {
        throw std::out_of_range{"Sprite SR7 ordered presentation index is out of range."};
    }
    return static_cast<std::size_t>(orderScratch_[orderedIndex].sourceIndex);
}

SDL_GPUGraphicsPipeline* SpriteGpuBackend::ResolvePipeline(
    const SpriteBlendCompatibility blend,
    const SpriteMaskMode maskMode) const
{
    const std::size_t blendIndex = BlendIndex(blend);
    switch (maskMode)
    {
    case SpriteMaskMode::None:
        return maskingRequired_
            ? stencilCompatibleUnmaskedPipelines_[blendIndex]
            : unmaskedPipelines_[blendIndex];
    case SpriteMaskMode::Write:
        return maskWritePipeline_;
    case SpriteMaskMode::TestInside:
        return maskInsidePipelines_[blendIndex];
    case SpriteMaskMode::TestOutside:
        return maskOutsidePipelines_[blendIndex];
    }
    throw std::invalid_argument{"Unsupported Sprite SR7 mask mode."};
}

bool SpriteGpuBackend::DrawPresentation(
    SDL_GPUCommandBuffer* const commandBuffer,
    SDL_GPURenderPass* const renderPass,
    SDL_GPUTexture* const texture,
    const SpritePresentationRenderData& presentation,
    const std::size_t presentationIndex)
{
    if (commandBuffer == nullptr || renderPass == nullptr || texture == nullptr)
    {
        throw std::invalid_argument{
            "Sprite SR7 draw requires live GPU command/render/texture state."};
    }
    if (presentationIndex >= patchOffsetScratch_.size() ||
        presentationIndex >= batchQuadCountScratch_.size())
    {
        throw std::out_of_range{"Sprite SR7 draw index exceeds current presentation scratch."};
    }

    const std::size_t quadCount = batchQuadCountScratch_[presentationIndex];
    if (quadCount == 0U)
    {
        return false;
    }
    const std::size_t firstQuad = patchOffsetScratch_[presentationIndex];
    if (firstQuad == NoQuadOffset ||
        firstQuad >= vertexCapacitySprites_ ||
        quadCount > vertexCapacitySprites_ - firstQuad)
    {
        throw std::out_of_range{"Sprite SR7 batch draw range exceeds uploaded vertex capacity."};
    }

    if (presentation.materialPipeline != BuiltInSpriteMaterialPipelineIdentity)
    {
        throw std::invalid_argument{"Sprite SR7 batch draw has no executable material/pipeline."};
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
        throw std::invalid_argument{"Unsupported Sprite SR7 sampler compatibility."};
    }

    SDL_GPUGraphicsPipeline* const pipeline = ResolvePipeline(
        presentation.presentation.appearance.blend,
        presentation.mask.mode);
    if (pipeline == nullptr)
    {
        throw std::runtime_error{"Sprite SR7 required graphics pipeline is unavailable."};
    }

    if (presentation.mask.mode != SpriteMaskMode::None)
    {
        SDL_SetGPUStencilReference(renderPass, presentation.mask.id);
    }

    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = vertexBuffer_;
    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
    SDL_BindGPUVertexBuffers(renderPass, 0U, &vertexBinding, 1U);

    SDL_GPUTextureSamplerBinding textureBinding{};
    textureBinding.texture = texture;
    textureBinding.sampler = sampler;
    SDL_BindGPUFragmentSamplers(renderPass, 0U, &textureBinding, 1U);

    const std::uint64_t firstVertex64 = static_cast<std::uint64_t>(firstQuad) * VerticesPerQuad;
    const std::uint64_t vertexCount64 = static_cast<std::uint64_t>(quadCount) * VerticesPerQuad;
    if (firstVertex64 > static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()) ||
        vertexCount64 > static_cast<std::uint64_t>(std::numeric_limits<Uint32>::max()))
    {
        throw std::length_error{"Sprite SR7 vertex draw range exceeds SDL GPU limits."};
    }
    SDL_DrawGPUPrimitives(
        renderPass,
        static_cast<Uint32>(vertexCount64),
        1U,
        static_cast<Uint32>(firstVertex64),
        0U);
    return true;
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

    maskingRequired_ = false;
    pixelPerfectViewport_ = SpritePixelPerfectViewport2D{};
    pixelPerfectViewportEnabled_ = false;
    orderScratch_.clear();
    patchOffsetScratch_.clear();
    batchQuadCountScratch_.clear();

    if (maskTarget_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, maskTarget_);
        maskTarget_ = nullptr;
        maskTargetWidth_ = 0U;
        maskTargetHeight_ = 0U;
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
        metrics_.vertexCapacitySprites = 0U;
        metrics_.vertexCapacityBytes = 0U;
    }
    if (maskWritePipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, maskWritePipeline_);
        maskWritePipeline_ = nullptr;
    }
    for (SDL_GPUGraphicsPipeline*& pipeline : maskOutsidePipelines_)
    {
        if (pipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline);
            pipeline = nullptr;
        }
    }
    for (SDL_GPUGraphicsPipeline*& pipeline : maskInsidePipelines_)
    {
        if (pipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline);
            pipeline = nullptr;
        }
    }
    for (SDL_GPUGraphicsPipeline*& pipeline : stencilCompatibleUnmaskedPipelines_)
    {
        if (pipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline);
            pipeline = nullptr;
        }
    }
    for (SDL_GPUGraphicsPipeline*& pipeline : unmaskedPipelines_)
    {
        if (pipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline);
            pipeline = nullptr;
        }
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