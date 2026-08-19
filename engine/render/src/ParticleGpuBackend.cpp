#include "ParticleGpuBackend.hpp"

#include <trace2d/particles/ParticleProgram.hpp>

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::render::detail
{
namespace
{
constexpr std::uint32_t WorkgroupSize = 64U;
constexpr std::uint32_t MissingOffset = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t ProgramWordCount = 13U;
constexpr Uint32 ParticleQuadVertexCount = 6U;

struct alignas(16) ProgramUniforms final
{
    std::array<std::array<std::uint32_t, 4>, ProgramWordCount> words{};
};

struct alignas(16) ClearUniforms final
{
    std::array<std::uint32_t, 4> words{};
};

struct alignas(16) StepUniforms final
{
    std::array<std::uint32_t, 4> words{};
    std::array<float, 4> emitter{};
};

struct alignas(16) DrawUniforms final
{
    std::array<float, 4> view{};
    std::array<float, 4> emitter{};
};

static_assert(sizeof(ProgramUniforms) == ProgramWordCount * 16U);
static_assert(sizeof(ClearUniforms) == 16U);
static_assert(sizeof(StepUniforms) == 32U);
static_assert(sizeof(DrawUniforms) == 32U);

[[nodiscard]] std::runtime_error MakeSdlError(const char* const context)
{
    return std::runtime_error{std::string{context} + ": " + SDL_GetError()};
}

[[nodiscard]] std::uint32_t FloatBits(const float value) noexcept
{
    return std::bit_cast<std::uint32_t>(value);
}

void SetFloat(std::array<std::uint32_t, 4>& word, const std::size_t component, const float value) noexcept
{
    word[component] = FloatBits(value);
}

[[nodiscard]] std::uint32_t FieldOffset(
    const particles::ParticleGpuCompileArtifact& artifact,
    const particles::ParticleGpuRuntimeFieldKind kind) noexcept
{
    for (std::uint32_t index = 0U; index < artifact.fieldCount; ++index)
    {
        if (artifact.fields[index].kind == kind)
        {
            return artifact.fields[index].offsetBytes;
        }
    }
    return MissingOffset;
}

[[nodiscard]] ProgramUniforms BuildProgramUniforms(
    const particles::ParticleProgram& program,
    const particles::ParticleGpuCompileArtifact& artifact,
    const std::uint64_t globalSeed,
    const std::uint64_t emitterStableId) noexcept
{
    ProgramUniforms uniforms{};
    uniforms.words[0] = {
        artifact.strideBytes,
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::Position),
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::Velocity),
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::AgeFrames),
    };
    uniforms.words[1] = {
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::LifetimeFrames),
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::InitialSize),
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::InitialRotation),
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::AngularVelocity),
    };
    uniforms.words[2] = {
        FieldOffset(artifact, particles::ParticleGpuRuntimeFieldKind::InitialColor),
        artifact.capacity,
        program.definition.lifetimeFrames.minValue,
        program.definition.lifetimeFrames.maxValue,
    };
    uniforms.words[3] = {
        static_cast<std::uint32_t>(program.definition.spawnShape.type),
        static_cast<std::uint32_t>(program.definition.simulationSpace),
        program.definition.spriteChoiceCount,
        0U,
    };

    SetFloat(uniforms.words[4], 0U, program.definition.spawnShape.offset.x);
    SetFloat(uniforms.words[4], 1U, program.definition.spawnShape.offset.y);
    SetFloat(uniforms.words[4], 2U, program.definition.spawnShape.boxHalfExtents.x);
    SetFloat(uniforms.words[4], 3U, program.definition.spawnShape.boxHalfExtents.y);

    SetFloat(uniforms.words[5], 0U, program.definition.spawnShape.circleRadius);
    SetFloat(uniforms.words[5], 1U, program.definition.speed.minValue);
    SetFloat(uniforms.words[5], 2U, program.definition.speed.maxValue);
    SetFloat(uniforms.words[5], 3U, program.definition.angleRadians.minValue);

    SetFloat(uniforms.words[6], 0U, program.definition.angleRadians.maxValue);
    SetFloat(uniforms.words[6], 1U, program.definition.acceleration.x);
    SetFloat(uniforms.words[6], 2U, program.definition.acceleration.y);
    SetFloat(uniforms.words[6], 3U, program.definition.initialSize.minValue);

    SetFloat(uniforms.words[7], 0U, program.definition.initialSize.maxValue);
    SetFloat(uniforms.words[7], 1U, program.definition.endSizeMultiplier);
    SetFloat(uniforms.words[7], 2U, program.definition.rotationRadians.minValue);
    SetFloat(uniforms.words[7], 3U, program.definition.rotationRadians.maxValue);

    SetFloat(uniforms.words[8], 0U, program.definition.angularVelocityRadiansPerFrame.minValue);
    SetFloat(uniforms.words[8], 1U, program.definition.angularVelocityRadiansPerFrame.maxValue);
    SetFloat(uniforms.words[8], 2U, program.definition.initialColor.minValue.r);
    SetFloat(uniforms.words[8], 3U, program.definition.initialColor.minValue.g);

    SetFloat(uniforms.words[9], 0U, program.definition.initialColor.minValue.b);
    SetFloat(uniforms.words[9], 1U, program.definition.initialColor.minValue.a);
    SetFloat(uniforms.words[9], 2U, program.definition.initialColor.maxValue.r);
    SetFloat(uniforms.words[9], 3U, program.definition.initialColor.maxValue.g);

    SetFloat(uniforms.words[10], 0U, program.definition.initialColor.maxValue.b);
    SetFloat(uniforms.words[10], 1U, program.definition.initialColor.maxValue.a);
    SetFloat(uniforms.words[10], 2U, program.definition.endColor.r);
    SetFloat(uniforms.words[10], 3U, program.definition.endColor.g);

    SetFloat(uniforms.words[11], 0U, program.definition.endColor.b);
    SetFloat(uniforms.words[11], 1U, program.definition.endColor.a);
    uniforms.words[11][2] = static_cast<std::uint32_t>(globalSeed);
    uniforms.words[11][3] = static_cast<std::uint32_t>(globalSeed >> 32U);

    uniforms.words[12][0] = static_cast<std::uint32_t>(emitterStableId);
    uniforms.words[12][1] = static_cast<std::uint32_t>(emitterStableId >> 32U);
    return uniforms;
}

[[nodiscard]] SDL_GPUComputePipeline* CompileHlslComputePipeline(
    SDL_GPUDevice* const device,
    const char* const source)
{
    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.source = source;
    hlslInfo.entrypoint = "main";
    hlslInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;

    std::size_t spirvSize = 0U;
    void* const spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
    if (spirv == nullptr)
    {
        throw MakeSdlError("Particle HLSL to SPIR-V compute compilation failed");
    }

    SDL_ShaderCross_ComputePipelineMetadata* const metadata =
        SDL_ShaderCross_ReflectComputeSPIRV(static_cast<const Uint8*>(spirv), spirvSize, 0);
    if (metadata == nullptr)
    {
        SDL_free(spirv);
        throw MakeSdlError("Particle compute SPIR-V reflection failed");
    }

    SDL_ShaderCross_SPIRV_Info spirvInfo{};
    spirvInfo.bytecode = static_cast<const Uint8*>(spirv);
    spirvInfo.bytecode_size = spirvSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;

    SDL_GPUComputePipeline* const pipeline =
        SDL_ShaderCross_CompileComputePipelineFromSPIRV(device, &spirvInfo, metadata, 0);

    SDL_free(metadata);
    SDL_free(spirv);

    if (pipeline == nullptr)
    {
        throw MakeSdlError("SDL GPU particle compute pipeline compilation failed");
    }
    return pipeline;
}

[[nodiscard]] SDL_GPUShader* CompileHlslGraphicsShader(
    SDL_GPUDevice* const device,
    const char* const source,
    const SDL_ShaderCross_ShaderStage stage)
{
    SDL_ShaderCross_HLSL_Info hlslInfo{};
    hlslInfo.source = source;
    hlslInfo.entrypoint = "main";
    hlslInfo.shader_stage = stage;

    std::size_t spirvSize = 0U;
    void* const spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
    if (spirv == nullptr)
    {
        throw MakeSdlError("Particle HLSL to SPIR-V graphics compilation failed");
    }

    SDL_ShaderCross_GraphicsShaderMetadata* const metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8*>(spirv), spirvSize, 0);
    if (metadata == nullptr)
    {
        SDL_free(spirv);
        throw MakeSdlError("Particle graphics SPIR-V reflection failed");
    }

    SDL_ShaderCross_SPIRV_Info spirvInfo{};
    spirvInfo.bytecode = static_cast<const Uint8*>(spirv);
    spirvInfo.bytecode_size = spirvSize;
    spirvInfo.entrypoint = "main";
    spirvInfo.shader_stage = stage;

    SDL_GPUShader* const shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device, &spirvInfo, &metadata->resource_info, 0);

    SDL_free(metadata);
    SDL_free(spirv);

    if (shader == nullptr)
    {
        throw MakeSdlError("SDL GPU particle graphics shader compilation failed");
    }
    return shader;
}

constexpr char ParticleClearShaderHlsl[] = R"(
RWByteAddressBuffer ParticleData : register(u0, space1);

cbuffer ClearConstants : register(b0, space2)
{
    uint4 ClearWords;
};

[numthreads(64, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    const uint index = threadId.x;
    const uint strideBytes = ClearWords.x;
    const uint ageOffsetBytes = ClearWords.y;
    const uint capacity = ClearWords.z;
    if (index >= capacity)
    {
        return;
    }

    ParticleData.Store(index * strideBytes + ageOffsetBytes, 0xffffffffu);
}
)";

constexpr char ParticleUpdateShaderHlsl[] = R"(
RWByteAddressBuffer ParticleData : register(u0, space1);

StructuredBuffer<uint4> P : register(t0, space0);

static const uint MissingOffset = 0xffffffffu;

float2 InitialVelocity()
{
    const float speed = asfloat(P[5].y);
    const float angle = asfloat(P[5].w);
    return float2(cos(angle), sin(angle)) * speed;
}

uint LifetimeAt(uint particleBase)
{
    const uint lifetimeOffset = P[1].x;
    return lifetimeOffset == MissingOffset
        ? P[2].z
        : ParticleData.Load(particleBase + lifetimeOffset);
}

[numthreads(64, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    const uint index = threadId.x;
    const uint capacity = P[2].y;
    if (index >= capacity)
    {
        return;
    }

    const uint baseOffset = index * P[0].x;
    const uint ageOffset = P[0].w;
    const uint age = ParticleData.Load(baseOffset + ageOffset);
    if (age == 0xffffffffu)
    {
        return;
    }

    const uint lifetime = LifetimeAt(baseOffset);
    if (age >= lifetime)
    {
        return;
    }

    const float2 acceleration = float2(asfloat(P[6].y), asfloat(P[6].z));
    float2 velocity = InitialVelocity();
    if (P[0].z != MissingOffset)
    {
        velocity = asfloat(ParticleData.Load2(baseOffset + P[0].z));
        velocity += acceleration;
        ParticleData.Store2(baseOffset + P[0].z, asuint(velocity));
    }
    else
    {
        velocity += acceleration * float(age + 1u);
    }

    float2 position = asfloat(ParticleData.Load2(baseOffset + P[0].y));
    position += velocity;
    ParticleData.Store2(baseOffset + P[0].y, asuint(position));
    ParticleData.Store(baseOffset + ageOffset, age + 1u);
}
)";

constexpr char ParticleSpawnShaderHlsl[] = R"(
RWByteAddressBuffer ParticleData : register(u0, space1);

StructuredBuffer<uint4> P : register(t0, space0);

cbuffer StepConstants : register(b0, space2)
{
    uint4 StepWords;
    float4 EmitterWorld;
};

static const uint MissingOffset = 0xffffffffu;
static const float UnitFloatScale = 5.9604644775390625e-8f;
static const float TwoPi = 6.28318530717958647692f;

uint MulHigh32(uint a, uint b)
{
    const uint a0 = a & 0xffffu;
    const uint a1 = a >> 16u;
    const uint b0 = b & 0xffffu;
    const uint b1 = b >> 16u;

    const uint p0 = a0 * b0;
    const uint p1 = a0 * b1;
    const uint p2 = a1 * b0;
    const uint p3 = a1 * b1;
    const uint carry = (p0 >> 16u) + (p1 & 0xffffu) + (p2 & 0xffffu);
    return p3 + (p1 >> 16u) + (p2 >> 16u) + (carry >> 16u);
}

uint2 Mul64(uint2 a, uint2 b)
{
    const uint low = a.x * b.x;
    const uint high = MulHigh32(a.x, b.x) + (a.x * b.y) + (a.y * b.x);
    return uint2(low, high);
}

uint2 ShiftRight64(uint2 value, uint shift)
{
    if (shift == 0u)
    {
        return value;
    }
    if (shift < 32u)
    {
        return uint2(
            (value.x >> shift) | (value.y << (32u - shift)),
            value.y >> shift);
    }
    if (shift < 64u)
    {
        return uint2(value.y >> (shift - 32u), 0u);
    }
    return uint2(0u, 0u);
}

uint2 Mix64(uint2 value)
{
    value ^= ShiftRight64(value, 30u);
    value = Mul64(value, uint2(0x1ce4e5b9u, 0xbf58476du));
    value ^= ShiftRight64(value, 27u);
    value = Mul64(value, uint2(0x133111ebu, 0x94d049bbu));
    value ^= ShiftRight64(value, 31u);
    return value;
}

uint2 AddOrdinal(uint2 baseOrdinal, uint addend)
{
    const uint low = baseOrdinal.x + addend;
    const uint carry = low < baseOrdinal.x ? 1u : 0u;
    return uint2(low, baseOrdinal.y + carry);
}

uint RandomU32(uint2 ordinal, uint channel)
{
    const uint2 seed = uint2(P[11].z, P[11].w);
    const uint2 emitter = uint2(P[12].x, P[12].y);

    const uint2 seedPart = Mix64(seed ^ uint2(0x85a308d3u, 0x243f6a88u));
    const uint2 emitterPart = Mix64(emitter ^ uint2(0x03707344u, 0x13198a2eu));
    const uint2 ordinalPart = Mix64(ordinal ^ uint2(0x299f31d0u, 0xa4093822u));
    const uint2 channelPart = Mix64(uint2(channel, 0u) ^ uint2(0xec4e6c89u, 0x082efa98u));
    return Mix64(seedPart ^ emitterPart ^ ordinalPart ^ channelPart).y;
}

float RandomUnit(uint2 ordinal, uint channel)
{
    return float(RandomU32(ordinal, channel) >> 8u) * UnitFloatScale;
}

float SampleFloat(uint2 ordinal, uint channel, float minValue, float maxValue)
{
    return minValue == maxValue
        ? minValue
        : minValue + ((maxValue - minValue) * RandomUnit(ordinal, channel));
}

uint SampleUIntInclusive(uint2 ordinal, uint channel, uint minValue, uint maxValue)
{
    if (minValue == maxValue)
    {
        return minValue;
    }

    const uint width = maxValue - minValue + 1u;
    return minValue + MulHigh32(RandomU32(ordinal, channel), width);
}

uint LifetimeAt(uint particleBase)
{
    const uint lifetimeOffset = P[1].x;
    return lifetimeOffset == MissingOffset
        ? P[2].z
        : ParticleData.Load(particleBase + lifetimeOffset);
}

bool IsDead(uint particleBase)
{
    const uint age = ParticleData.Load(particleBase + P[0].w);
    if (age == 0xffffffffu)
    {
        return true;
    }
    return age >= LifetimeAt(particleBase);
}

float2 SampleSpawnPosition(uint2 ordinal)
{
    const uint shapeType = P[3].x;
    const float2 offset = float2(asfloat(P[4].x), asfloat(P[4].y));
    if (shapeType == 0u)
    {
        return offset;
    }

    if (shapeType == 1u)
    {
        const float2 halfExtents = float2(asfloat(P[4].z), asfloat(P[4].w));
        return offset + float2(
            SampleFloat(ordinal, 0x00010001u, -halfExtents.x, halfExtents.x),
            SampleFloat(ordinal, 0x00010002u, -halfExtents.y, halfExtents.y));
    }

    const float angle = RandomUnit(ordinal, 0x00010001u) * TwoPi;
    const float radius = sqrt(RandomUnit(ordinal, 0x00010002u)) * asfloat(P[5].x);
    return offset + float2(cos(angle), sin(angle)) * radius;
}

float2 SampleVelocity(uint2 ordinal)
{
    const float speed = SampleFloat(
        ordinal, 0x00030001u, asfloat(P[5].y), asfloat(P[5].z));
    const float angle = SampleFloat(
        ordinal, 0x00030002u, asfloat(P[5].w), asfloat(P[6].x));
    return float2(cos(angle), sin(angle)) * speed;
}

float4 SampleColor(uint2 ordinal)
{
    const float4 minColor = float4(
        asfloat(P[8].z), asfloat(P[8].w), asfloat(P[9].x), asfloat(P[9].y));
    const float4 maxColor = float4(
        asfloat(P[9].z), asfloat(P[9].w), asfloat(P[10].x), asfloat(P[10].y));
    return float4(
        SampleFloat(ordinal, 0x00060001u, minColor.r, maxColor.r),
        SampleFloat(ordinal, 0x00060002u, minColor.g, maxColor.g),
        SampleFloat(ordinal, 0x00060003u, minColor.b, maxColor.b),
        SampleFloat(ordinal, 0x00060004u, minColor.a, maxColor.a));
}

void SpawnParticle(uint particleBase, uint2 ordinal)
{
    float2 position = SampleSpawnPosition(ordinal);
    if (P[3].y == 1u)
    {
        position += EmitterWorld.xy;
    }

    ParticleData.Store2(particleBase + P[0].y, asuint(position));

    if (P[0].z != MissingOffset)
    {
        ParticleData.Store2(particleBase + P[0].z, asuint(SampleVelocity(ordinal)));
    }

    ParticleData.Store(particleBase + P[0].w, 0u);

    if (P[1].x != MissingOffset)
    {
        const uint lifetime = SampleUIntInclusive(
            ordinal, 0x00020001u, P[2].z, P[2].w);
        ParticleData.Store(particleBase + P[1].x, lifetime);
    }

    if (P[1].y != MissingOffset)
    {
        const float initialSize = SampleFloat(
            ordinal, 0x00050001u, asfloat(P[6].w), asfloat(P[7].x));
        ParticleData.Store(particleBase + P[1].y, asuint(initialSize));
    }

    if (P[1].z != MissingOffset)
    {
        const float initialRotation = SampleFloat(
            ordinal, 0x00040001u, asfloat(P[7].z), asfloat(P[7].w));
        ParticleData.Store(particleBase + P[1].z, asuint(initialRotation));
    }

    if (P[1].w != MissingOffset)
    {
        const float angularVelocity = SampleFloat(
            ordinal, 0x00040002u, asfloat(P[8].x), asfloat(P[8].y));
        ParticleData.Store(particleBase + P[1].w, asuint(angularVelocity));
    }

    if (P[2].x != MissingOffset)
    {
        ParticleData.Store4(particleBase + P[2].x, asuint(SampleColor(ordinal)));
    }
}

[numthreads(1, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x != 0u)
    {
        return;
    }

    const uint attempts = StepWords.z;
    const uint capacity = P[2].y;
    uint freeCursor = 0u;
    const uint2 firstOrdinal = uint2(StepWords.x, StepWords.y);

    for (uint attempt = 0u; attempt < attempts; ++attempt)
    {
        while (freeCursor < capacity)
        {
            const uint candidateBase = freeCursor * P[0].x;
            if (IsDead(candidateBase))
            {
                break;
            }
            ++freeCursor;
        }

        if (freeCursor >= capacity)
        {
            break;
        }

        const uint particleBase = freeCursor * P[0].x;
        SpawnParticle(particleBase, AddOrdinal(firstOrdinal, attempt));
        ++freeCursor;
    }
}
)";

constexpr char ParticleVertexShaderHlsl[] = R"(
ByteAddressBuffer ParticleData : register(t0, space0);
StructuredBuffer<uint4> P : register(t1, space0);

cbuffer DrawConstants : register(b0, space1)
{
    float4 ViewData;
    float4 EmitterWorld;
};

static const uint MissingOffset = 0xffffffffu;

struct VertexInput
{
    float2 localPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
    uint instanceId : SV_InstanceID;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

uint LifetimeAt(uint particleBase)
{
    return P[1].x == MissingOffset
        ? P[2].z
        : ParticleData.Load(particleBase + P[1].x);
}

float4 InitialColorAt(uint particleBase)
{
    if (P[2].x != MissingOffset)
    {
        return asfloat(ParticleData.Load4(particleBase + P[2].x));
    }

    return float4(
        asfloat(P[8].z), asfloat(P[8].w), asfloat(P[9].x), asfloat(P[9].y));
}

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    const uint particleBase = input.instanceId * P[0].x;
    const uint age = ParticleData.Load(particleBase + P[0].w);
    const uint lifetime = LifetimeAt(particleBase);

    if (age == 0xffffffffu || age >= lifetime)
    {
        output.position = float4(2.0f, 2.0f, 0.0f, 1.0f);
        output.uv = input.uv;
        output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }

    float2 worldPosition = asfloat(ParticleData.Load2(particleBase + P[0].y));
    if (P[3].y == 0u)
    {
        worldPosition += EmitterWorld.xy;
    }

    const float t = lifetime <= 1u
        ? 0.0f
        : float(age) / float(lifetime - 1u);

    const float initialSize = P[1].y == MissingOffset
        ? asfloat(P[6].w)
        : asfloat(ParticleData.Load(particleBase + P[1].y));
    const float sizeMultiplier = lerp(1.0f, asfloat(P[7].y), t);
    const float halfSize = initialSize * sizeMultiplier * 0.5f;

    const float initialRotation = P[1].z == MissingOffset
        ? asfloat(P[7].z)
        : asfloat(ParticleData.Load(particleBase + P[1].z));
    const float angularVelocity = P[1].w == MissingOffset
        ? asfloat(P[8].x)
        : asfloat(ParticleData.Load(particleBase + P[1].w));
    const float rotation = initialRotation + angularVelocity * float(age);
    const float cosine = cos(rotation);
    const float sine = sin(rotation);
    const float2 local = input.localPosition * halfSize;
    const float2 rotated = float2(
        local.x * cosine - local.y * sine,
        local.x * sine + local.y * cosine);

    const float2 vertexWorld = worldPosition + rotated;
    const float2 clip = (vertexWorld - ViewData.xy) * ViewData.zw;

    const float4 initialColor = InitialColorAt(particleBase);
    const float4 endColor = float4(
        asfloat(P[10].z), asfloat(P[10].w), asfloat(P[11].x), asfloat(P[11].y));

    output.position = float4(clip, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = lerp(initialColor, endColor, t);
    return output;
}
)";

constexpr char ParticleFragmentShaderHlsl[] = R"(
Texture2D ParticleTexture : register(t0, space2);
SamplerState ParticleSampler : register(s0, space2);

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

float4 main(FragmentInput input) : SV_Target0
{
    return ParticleTexture.Sample(ParticleSampler, input.uv) * input.color;
}
)";

[[nodiscard]] SDL_GPUGraphicsPipeline* CreateParticleGraphicsPipeline(
    SDL_GPUDevice* const device,
    const SDL_GPUTextureFormat colorTargetFormat,
    const bool additive)
{
    SDL_GPUShader* vertexShader = nullptr;
    SDL_GPUShader* fragmentShader = nullptr;

    try
    {
        vertexShader = CompileHlslGraphicsShader(
            device, ParticleVertexShaderHlsl, SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
        fragmentShader = CompileHlslGraphicsShader(
            device, ParticleFragmentShaderHlsl, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

        SDL_GPUColorTargetDescription colorTargetDescription{};
        colorTargetDescription.format = colorTargetFormat;
        colorTargetDescription.blend_state.enable_blend = true;
        colorTargetDescription.blend_state.color_write_mask = 0xFU;
        colorTargetDescription.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTargetDescription.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTargetDescription.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTargetDescription.blend_state.dst_color_blendfactor =
            additive ? SDL_GPU_BLENDFACTOR_ONE : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTargetDescription.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTargetDescription.blend_state.dst_alpha_blendfactor =
            additive ? SDL_GPU_BLENDFACTOR_ONE : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

        SDL_GPUVertexBufferDescription vertexBufferDescription{};
        vertexBufferDescription.slot = 0U;
        vertexBufferDescription.pitch = 4U * sizeof(float);
        vertexBufferDescription.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescription.instance_step_rate = 0U;

        std::array<SDL_GPUVertexAttribute, 2> attributes{};
        attributes[0].location = 0U;
        attributes[0].buffer_slot = 0U;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[0].offset = 0U;
        attributes[1].location = 1U;
        attributes[1].buffer_slot = 0U;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[1].offset = 2U * sizeof(float);

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.vertex_shader = vertexShader;
        pipelineInfo.fragment_shader = fragmentShader;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDescription;
        pipelineInfo.vertex_input_state.num_vertex_buffers = 1U;
        pipelineInfo.vertex_input_state.vertex_attributes = attributes.data();
        pipelineInfo.vertex_input_state.num_vertex_attributes =
            static_cast<Uint32>(attributes.size());
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.rasterizer_state.enable_depth_clip = true;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDescription;
        pipelineInfo.target_info.num_color_targets = 1U;

        SDL_GPUGraphicsPipeline* const pipeline =
            SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (pipeline == nullptr)
        {
            throw MakeSdlError("SDL GPU particle graphics pipeline creation failed");
        }

        SDL_ReleaseGPUShader(device, vertexShader);
        SDL_ReleaseGPUShader(device, fragmentShader);
        return pipeline;
    }
    catch (...)
    {
        if (vertexShader != nullptr)
        {
            SDL_ReleaseGPUShader(device, vertexShader);
        }
        if (fragmentShader != nullptr)
        {
            SDL_ReleaseGPUShader(device, fragmentShader);
        }
        throw;
    }
}

[[nodiscard]] std::uint32_t CeilDivide(
    const std::uint32_t value,
    const std::uint32_t divisor) noexcept
{
    return (value + divisor - 1U) / divisor;
}

[[nodiscard]] std::uint32_t SpawnAttemptsForFrame(
    const particles::ParticleProgram& program,
    const std::uint32_t cycleFrame,
    std::uint32_t& nextBurstIndex) noexcept
{
    std::uint64_t attempts = 0U;
    while (nextBurstIndex < program.bursts.size() &&
           program.bursts[nextBurstIndex].frame == cycleFrame)
    {
        attempts += program.bursts[nextBurstIndex].count;
        ++nextBurstIndex;
    }

    const particles::ParticleReferenceDefinition& definition = program.definition;
    if (definition.periodicCount != 0U &&
        cycleFrame >= definition.periodicStartFrame &&
        ((static_cast<std::uint64_t>(cycleFrame) - definition.periodicStartFrame) %
         definition.periodicEveryFrames) == 0U)
    {
        attempts += definition.periodicCount;
    }

    return static_cast<std::uint32_t>(attempts);
}
} // namespace

struct ParticleGpuBackend::Impl final
{
    struct Emitter final
    {
        particles::ParticleProgram program{};
        particles::ParticleGpuCompileArtifact artifact{};
        ProgramUniforms programUniforms{};
        SDL_GPUBuffer* particleBuffer{nullptr};
        SDL_GPUBuffer* programBuffer{nullptr};
        TextureHandle texture{InvalidTextureHandle};
        GpuParticleEmitterMetrics metrics{};
        std::uint64_t nextSpawnOrdinal{0U};
        std::uint64_t completedLoops{0U};
        std::uint32_t cycleFrame{0U};
        std::uint32_t nextBurstIndex{0U};
        std::uint32_t instanceUpperBound{0U};
        bool playing{false};
        bool resetBeforeNextStep{false};
    };

    Impl(SDL_GPUDevice* const deviceValue, const SDL_GPUTextureFormat colorTargetFormat)
        : device{deviceValue}
    {
        if (device == nullptr)
        {
            throw std::invalid_argument{"Particle GPU backend requires a live SDL GPU device."};
        }

        try
        {
            clearPipeline = CompileHlslComputePipeline(device, ParticleClearShaderHlsl);
            updatePipeline = CompileHlslComputePipeline(device, ParticleUpdateShaderHlsl);
            spawnPipeline = CompileHlslComputePipeline(device, ParticleSpawnShaderHlsl);
            alphaPipeline = CreateParticleGraphicsPipeline(device, colorTargetFormat, false);
            additivePipeline = CreateParticleGraphicsPipeline(device, colorTargetFormat, true);
        }
        catch (...)
        {
            CleanupPipelines();
            throw;
        }
    }

    ~Impl()
    {
        for (std::unique_ptr<Emitter>& emitter : emitters)
        {
            ReleaseEmitter(emitter);
        }
        CleanupPipelines();
    }

    void CleanupPipelines() noexcept
    {
        if (additivePipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(device, additivePipeline);
            additivePipeline = nullptr;
        }
        if (alphaPipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(device, alphaPipeline);
            alphaPipeline = nullptr;
        }
        if (spawnPipeline != nullptr)
        {
            SDL_ReleaseGPUComputePipeline(device, spawnPipeline);
            spawnPipeline = nullptr;
        }
        if (updatePipeline != nullptr)
        {
            SDL_ReleaseGPUComputePipeline(device, updatePipeline);
            updatePipeline = nullptr;
        }
        if (clearPipeline != nullptr)
        {
            SDL_ReleaseGPUComputePipeline(device, clearPipeline);
            clearPipeline = nullptr;
        }
    }

    void ReleaseEmitter(std::unique_ptr<Emitter>& emitter) noexcept
    {
        if (emitter != nullptr && emitter->programBuffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device, emitter->programBuffer);
            emitter->programBuffer = nullptr;
        }
        if (emitter != nullptr && emitter->particleBuffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(device, emitter->particleBuffer);
            emitter->particleBuffer = nullptr;
        }
        emitter.reset();
    }

    [[nodiscard]] Emitter& Resolve(const GpuParticleEmitterHandle handle)
    {
        if (handle == InvalidGpuParticleEmitterHandle)
        {
            throw std::invalid_argument{"GPU particle emitter handle is invalid."};
        }
        const std::size_t index = static_cast<std::size_t>(handle - 1U);
        if (index >= emitters.size() || emitters[index] == nullptr)
        {
            throw std::invalid_argument{"GPU particle emitter handle does not reference a live emitter."};
        }
        return *emitters[index];
    }

    [[nodiscard]] const Emitter& Resolve(const GpuParticleEmitterHandle handle) const
    {
        if (handle == InvalidGpuParticleEmitterHandle)
        {
            throw std::invalid_argument{"GPU particle emitter handle is invalid."};
        }
        const std::size_t index = static_cast<std::size_t>(handle - 1U);
        if (index >= emitters.size() || emitters[index] == nullptr)
        {
            throw std::invalid_argument{"GPU particle emitter handle does not reference a live emitter."};
        }
        return *emitters[index];
    }

    void EncodeClear(SDL_GPUCommandBuffer* const commandBuffer, Emitter& emitter)
    {
        const ClearUniforms uniforms{{
            emitter.artifact.strideBytes,
            FieldOffset(emitter.artifact, particles::ParticleGpuRuntimeFieldKind::AgeFrames),
            emitter.artifact.capacity,
            0U,
        }};
        SDL_PushGPUComputeUniformData(
            commandBuffer, 0U, &uniforms, static_cast<Uint32>(sizeof(uniforms)));

        SDL_GPUStorageBufferReadWriteBinding writable{};
        writable.buffer = emitter.particleBuffer;
        writable.cycle = false;
        SDL_GPUComputePass* const pass =
            SDL_BeginGPUComputePass(commandBuffer, nullptr, 0U, &writable, 1U);
        if (pass == nullptr)
        {
            throw MakeSdlError("SDL GPU particle clear compute pass creation failed");
        }

        SDL_BindGPUComputePipeline(pass, clearPipeline);
        SDL_DispatchGPUCompute(
            pass, CeilDivide(emitter.artifact.capacity, WorkgroupSize), 1U, 1U);
        SDL_EndGPUComputePass(pass);
        ++emitter.metrics.clearDispatches;
        emitter.instanceUpperBound = 0U;
        emitter.metrics.instanceUpperBound = 0U;
    }

    void EncodeUpdate(
        SDL_GPUCommandBuffer* const commandBuffer,
        Emitter& emitter)
    {
        if (emitter.instanceUpperBound == 0U)
        {
            return;
        }

        SDL_GPUStorageBufferReadWriteBinding writable{};
        writable.buffer = emitter.particleBuffer;
        writable.cycle = false;
        SDL_GPUComputePass* const pass =
            SDL_BeginGPUComputePass(commandBuffer, nullptr, 0U, &writable, 1U);
        if (pass == nullptr)
        {
            throw MakeSdlError("SDL GPU particle update compute pass creation failed");
        }

        SDL_BindGPUComputePipeline(pass, updatePipeline);
        SDL_GPUBuffer* readonlyBuffers[]{emitter.programBuffer};
        SDL_BindGPUComputeStorageBuffers(pass, 0U, readonlyBuffers, 1U);
        SDL_DispatchGPUCompute(
            pass, CeilDivide(emitter.instanceUpperBound, WorkgroupSize), 1U, 1U);
        SDL_EndGPUComputePass(pass);
        ++emitter.metrics.updateDispatches;
    }

    void EncodeSpawn(
        SDL_GPUCommandBuffer* const commandBuffer,
        Emitter& emitter,
        const std::uint32_t attempts,
        const Float2 emitterWorldPosition)
    {
        if (attempts == 0U)
        {
            return;
        }

        StepUniforms step{};
        step.words = {
            static_cast<std::uint32_t>(emitter.nextSpawnOrdinal),
            static_cast<std::uint32_t>(emitter.nextSpawnOrdinal >> 32U),
            attempts,
            emitter.artifact.capacity,
        };
        step.emitter = {emitterWorldPosition.x, emitterWorldPosition.y, 0.0F, 0.0F};

        SDL_PushGPUComputeUniformData(
            commandBuffer, 0U, &step, static_cast<Uint32>(sizeof(step)));

        SDL_GPUStorageBufferReadWriteBinding writable{};
        writable.buffer = emitter.particleBuffer;
        writable.cycle = false;
        SDL_GPUComputePass* const pass =
            SDL_BeginGPUComputePass(commandBuffer, nullptr, 0U, &writable, 1U);
        if (pass == nullptr)
        {
            throw MakeSdlError("SDL GPU particle spawn compute pass creation failed");
        }

        SDL_BindGPUComputePipeline(pass, spawnPipeline);
        SDL_GPUBuffer* readonlyBuffers[]{emitter.programBuffer};
        SDL_BindGPUComputeStorageBuffers(pass, 0U, readonlyBuffers, 1U);
        SDL_DispatchGPUCompute(pass, 1U, 1U, 1U);
        SDL_EndGPUComputePass(pass);

        emitter.nextSpawnOrdinal += attempts;
        emitter.instanceUpperBound = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            emitter.artifact.capacity,
            static_cast<std::uint64_t>(emitter.instanceUpperBound) + attempts));
        ++emitter.metrics.spawnDispatches;
        emitter.metrics.submittedSpawnAttempts += attempts;
        emitter.metrics.instanceUpperBound = emitter.instanceUpperBound;
    }

    SDL_GPUDevice* device{nullptr};
    SDL_GPUComputePipeline* clearPipeline{nullptr};
    SDL_GPUComputePipeline* updatePipeline{nullptr};
    SDL_GPUComputePipeline* spawnPipeline{nullptr};
    SDL_GPUGraphicsPipeline* alphaPipeline{nullptr};
    SDL_GPUGraphicsPipeline* additivePipeline{nullptr};
    std::vector<std::unique_ptr<Emitter>> emitters{};
};

ParticleGpuBackend::ParticleGpuBackend(
    SDL_GPUDevice* const device,
    const SDL_GPUTextureFormat colorTargetFormat)
    : impl_{std::make_unique<Impl>(device, colorTargetFormat)}
{
}

ParticleGpuBackend::~ParticleGpuBackend() = default;

GpuParticleEmitterCreateResult ParticleGpuBackend::CreateEmitter(
    const particles::ParticleProgram& program,
    const std::uint64_t globalSeed,
    const std::uint64_t emitterStableId,
    const TextureHandle texture)
{
    GpuParticleEmitterCreateResult result{};
    result.support = AnalyzeGpuParticleRuntimeSupport(program);
    if (!result.support.Ok())
    {
        return result;
    }
    if (texture == InvalidTextureHandle)
    {
        result.support.error = GpuParticleRuntimeError::InvalidTexture;
        return result;
    }
    if (impl_->emitters.size() >=
        static_cast<std::size_t>(std::numeric_limits<GpuParticleEmitterHandle>::max() - 1U))
    {
        throw std::length_error{"GPU particle emitter handle space exhausted."};
    }

    const particles::ParticleGpuCompileResult compile = particles::CompileParticleGpuArtifact(program);
    if (!compile.Ok())
    {
        result.support.error = GpuParticleRuntimeError::UnsupportedFeature;
        return result;
    }

    auto emitter = std::make_unique<Impl::Emitter>();
    emitter->program = program;
    emitter->artifact = compile.artifact;
    emitter->programUniforms =
        BuildProgramUniforms(program, compile.artifact, globalSeed, emitterStableId);
    emitter->texture = texture;
    emitter->playing = program.lifecycle.playOnLoad;

    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage =
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
        SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    bufferInfo.size = static_cast<Uint32>(compile.artifact.bufferBytes);
    emitter->particleBuffer = SDL_CreateGPUBuffer(impl_->device, &bufferInfo);
    if (emitter->particleBuffer == nullptr)
    {
        throw MakeSdlError("SDL GPU particle storage buffer creation failed");
    }

    SDL_GPUBufferCreateInfo programBufferInfo{};
    programBufferInfo.usage =
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
        SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    programBufferInfo.size = static_cast<Uint32>(sizeof(ProgramUniforms));
    emitter->programBuffer = SDL_CreateGPUBuffer(impl_->device, &programBufferInfo);
    if (emitter->programBuffer == nullptr)
    {
        SDL_ReleaseGPUBuffer(impl_->device, emitter->particleBuffer);
        emitter->particleBuffer = nullptr;
        throw MakeSdlError("SDL GPU particle program buffer creation failed");
    }

    emitter->metrics.programFingerprint = program.fingerprint;
    emitter->metrics.artifactFingerprint = compile.artifact.artifactFingerprint;
    emitter->metrics.pipelineVariantId = compile.artifact.pipelineVariantId;
    emitter->metrics.capacity = compile.artifact.capacity;
    emitter->metrics.strideBytes = compile.artifact.strideBytes;
    emitter->metrics.particleBufferBytes = compile.artifact.bufferBytes;
    emitter->metrics.retainedGpuBytes = compile.artifact.bufferBytes + sizeof(ProgramUniforms);
    emitter->metrics.particleBufferCreations = 1U;

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<Uint32>(sizeof(ProgramUniforms));
    SDL_GPUTransferBuffer* const transferBuffer =
        SDL_CreateGPUTransferBuffer(impl_->device, &transferInfo);
    if (transferBuffer == nullptr)
    {
        impl_->ReleaseEmitter(emitter);
        throw MakeSdlError("SDL GPU particle program transfer-buffer creation failed");
    }

    void* const mapped = SDL_MapGPUTransferBuffer(impl_->device, transferBuffer, false);
    if (mapped == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(impl_->device, transferBuffer);
        impl_->ReleaseEmitter(emitter);
        throw MakeSdlError("SDL GPU particle program transfer-buffer mapping failed");
    }
    SDL_memcpy(mapped, &emitter->programUniforms, sizeof(emitter->programUniforms));
    SDL_UnmapGPUTransferBuffer(impl_->device, transferBuffer);

    SDL_GPUCommandBuffer* const commandBuffer = SDL_AcquireGPUCommandBuffer(impl_->device);
    if (commandBuffer == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(impl_->device, transferBuffer);
        impl_->ReleaseEmitter(emitter);
        throw MakeSdlError("SDL GPU particle initialization command buffer acquisition failed");
    }

    try
    {
        SDL_GPUCopyPass* const copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        if (copyPass == nullptr)
        {
            throw MakeSdlError("SDL GPU particle program upload copy-pass creation failed");
        }

        SDL_GPUTransferBufferLocation source{};
        source.transfer_buffer = transferBuffer;
        SDL_GPUBufferRegion destination{};
        destination.buffer = emitter->programBuffer;
        destination.size = static_cast<Uint32>(sizeof(ProgramUniforms));
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
        SDL_EndGPUCopyPass(copyPass);

        impl_->EncodeClear(commandBuffer, *emitter);
    }
    catch (...)
    {
        SDL_CancelGPUCommandBuffer(commandBuffer);
        SDL_ReleaseGPUTransferBuffer(impl_->device, transferBuffer);
        impl_->ReleaseEmitter(emitter);
        throw;
    }

    if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
    {
        SDL_ReleaseGPUTransferBuffer(impl_->device, transferBuffer);
        impl_->ReleaseEmitter(emitter);
        throw MakeSdlError("SDL GPU particle initialization submission failed");
    }
    SDL_ReleaseGPUTransferBuffer(impl_->device, transferBuffer);

    impl_->emitters.push_back(std::move(emitter));
    result.handle = static_cast<GpuParticleEmitterHandle>(impl_->emitters.size());
    return result;
}

void ParticleGpuBackend::DestroyEmitter(const GpuParticleEmitterHandle emitter) noexcept
{
    if (emitter == InvalidGpuParticleEmitterHandle)
    {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(emitter - 1U);
    if (index >= impl_->emitters.size())
    {
        return;
    }
    impl_->ReleaseEmitter(impl_->emitters[index]);
}

void ParticleGpuBackend::ResetEmitter(
    SDL_GPUCommandBuffer* const commandBuffer,
    const GpuParticleEmitterHandle emitterHandle)
{
    Impl::Emitter& emitter = impl_->Resolve(emitterHandle);
    impl_->EncodeClear(commandBuffer, emitter);
    emitter.nextSpawnOrdinal = 0U;
    emitter.completedLoops = 0U;
    emitter.cycleFrame = 0U;
    emitter.nextBurstIndex = 0U;
    emitter.resetBeforeNextStep = false;
    emitter.playing = emitter.program.lifecycle.playOnLoad;
}

void ParticleGpuBackend::PlayEmitter(
    SDL_GPUCommandBuffer* const commandBuffer,
    const GpuParticleEmitterHandle emitterHandle)
{
    Impl::Emitter& emitter = impl_->Resolve(emitterHandle);
    if (!emitter.program.lifecycle.loop &&
        emitter.cycleFrame >= emitter.program.lifecycle.durationFrames)
    {
        impl_->EncodeClear(commandBuffer, emitter);
        emitter.nextSpawnOrdinal = 0U;
        emitter.cycleFrame = 0U;
        emitter.nextBurstIndex = 0U;
        emitter.resetBeforeNextStep = false;
    }
    emitter.playing = true;
}

void ParticleGpuBackend::RestartEmitter(
    SDL_GPUCommandBuffer* const commandBuffer,
    const GpuParticleEmitterHandle emitterHandle)
{
    Impl::Emitter& emitter = impl_->Resolve(emitterHandle);
    impl_->EncodeClear(commandBuffer, emitter);
    emitter.nextSpawnOrdinal = 0U;
    emitter.completedLoops = 0U;
    emitter.cycleFrame = 0U;
    emitter.nextBurstIndex = 0U;
    emitter.resetBeforeNextStep = false;
    emitter.playing = true;
}

void ParticleGpuBackend::StopEmitter(const GpuParticleEmitterHandle emitter) noexcept
{
    if (!IsLive(emitter))
    {
        return;
    }
    impl_->Resolve(emitter).playing = false;
}

bool ParticleGpuBackend::StepEmitter(
    SDL_GPUCommandBuffer* const commandBuffer,
    const GpuParticleStepData& step)
{
    Impl::Emitter& emitter = impl_->Resolve(step.emitter);
    if (!emitter.playing)
    {
        return true;
    }

    if (emitter.resetBeforeNextStep)
    {
        impl_->EncodeClear(commandBuffer, emitter);
        emitter.nextSpawnOrdinal = 0U;
        emitter.cycleFrame = 0U;
        emitter.nextBurstIndex = 0U;
        emitter.resetBeforeNextStep = false;
    }

    impl_->EncodeUpdate(commandBuffer, emitter);
    const std::uint32_t attempts =
        SpawnAttemptsForFrame(emitter.program, emitter.cycleFrame, emitter.nextBurstIndex);
    impl_->EncodeSpawn(commandBuffer, emitter, attempts, step.emitterWorldPosition);

    ++emitter.metrics.submittedSteps;
    ++emitter.cycleFrame;
    if (emitter.cycleFrame >= emitter.program.lifecycle.durationFrames)
    {
        if (emitter.program.lifecycle.loop)
        {
            ++emitter.completedLoops;
            emitter.resetBeforeNextStep = true;
        }
        else
        {
            emitter.playing = false;
        }
    }
    return true;
}

bool ParticleGpuBackend::IsLive(const GpuParticleEmitterHandle emitter) const noexcept
{
    if (emitter == InvalidGpuParticleEmitterHandle)
    {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(emitter - 1U);
    return index < impl_->emitters.size() && impl_->emitters[index] != nullptr;
}

TextureHandle ParticleGpuBackend::Texture(const GpuParticleEmitterHandle emitter) const
{
    return impl_->Resolve(emitter).texture;
}

GpuParticleEmitterMetrics ParticleGpuBackend::Metrics(
    const GpuParticleEmitterHandle emitter) const
{
    return impl_->Resolve(emitter).metrics;
}

void ParticleGpuBackend::DrawEmitter(
    SDL_GPUCommandBuffer* const commandBuffer,
    SDL_GPURenderPass* const renderPass,
    const OrthographicView& view,
    const GpuParticleRenderData& renderData,
    SDL_GPUBuffer* const quadVertexBuffer,
    SDL_GPUSampler* const sampler,
    SDL_GPUTexture* const texture,
    std::uint64_t& encodedDraws,
    std::uint64_t& encodedInstances)
{
    Impl::Emitter& emitter = impl_->Resolve(renderData.emitter);
    if (emitter.instanceUpperBound == 0U)
    {
        return;
    }
    if (quadVertexBuffer == nullptr || sampler == nullptr || texture == nullptr)
    {
        throw std::invalid_argument{"GPU particle drawing requires live renderer resources."};
    }

    DrawUniforms draw{};
    draw.view = {view.center.x, view.center.y, view.clipScale.x, view.clipScale.y};
    draw.emitter = {
        renderData.emitterWorldPosition.x,
        renderData.emitterWorldPosition.y,
        0.0F,
        0.0F,
    };
    SDL_PushGPUVertexUniformData(
        commandBuffer, 0U, &draw, static_cast<Uint32>(sizeof(draw)));

    SDL_GPUGraphicsPipeline* const pipeline =
        emitter.program.blendMode == particles::ParticleBlendMode::Additive
            ? impl_->additivePipeline
            : impl_->alphaPipeline;
    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    SDL_GPUBufferBinding vertexBinding{};
    vertexBinding.buffer = quadVertexBuffer;
    SDL_BindGPUVertexBuffers(renderPass, 0U, &vertexBinding, 1U);

    SDL_GPUBuffer* storageBuffers[]{emitter.particleBuffer, emitter.programBuffer};
    SDL_BindGPUVertexStorageBuffers(renderPass, 0U, storageBuffers, 2U);

    SDL_GPUTextureSamplerBinding textureBinding{};
    textureBinding.texture = texture;
    textureBinding.sampler = sampler;
    SDL_BindGPUFragmentSamplers(renderPass, 0U, &textureBinding, 1U);

    SDL_DrawGPUPrimitives(
        renderPass,
        ParticleQuadVertexCount,
        emitter.instanceUpperBound,
        0U,
        0U);

    ++emitter.metrics.renderDraws;
    emitter.metrics.renderInstances += emitter.instanceUpperBound;
    ++encodedDraws;
    encodedInstances += emitter.instanceUpperBound;
}
} // namespace trace2d::render::detail
