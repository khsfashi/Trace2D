#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/render/Material2D.hpp>
#include <trace2d/render/SpriteBatch2D.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace trace2d::render
{
enum class MaterialGpuPrepareError2D : std::uint8_t
{
    None = 0,
    InvalidMaterialHandle,
    InvalidShaderHandle,
    UnsupportedShaderLanguage,
    UnsupportedShaderStage,
    InvalidShaderEntryPoint,
    ParameterPreparationFailed,
    ShaderCompilationFailed,
    ShaderReflectionFailed,
    ShaderContractMismatch,
    PipelineCreationFailed,
};

[[nodiscard]] std::string_view ToString(MaterialGpuPrepareError2D value) noexcept;

// Setup-time result consumed by Sprite presentation. Canonical resources remain owned by #86;
// this value contains only generation-safe identities plus compact renderer preparation state.
// The parameter layout/default block are fixed-capacity MAT1 values and therefore require no
// per-instance map or allocation in steady rendering.
struct PreparedMaterial2D final
{
    assets::ResourceHandleUntyped materialIdentity{};
    assets::ResourceHandleUntyped shaderIdentity{};
    SpriteMaterialPipelineIdentity pipelineIdentity{InvalidSpriteMaterialPipelineIdentity};
    SpriteSamplerCompatibility sampler{SpriteSamplerCompatibility::Nearest};
    SpriteBlendCompatibility blend{SpriteBlendCompatibility::Normal};
    MaterialParameterLayout2D parameterLayout{};
    MaterialParameterBlock2D defaultParameters{};

    [[nodiscard]] bool operator==(const PreparedMaterial2D&) const noexcept = default;
};

struct MaterialGpuPrepareResult2D final
{
    PreparedMaterial2D material{};
    MaterialGpuPrepareError2D error{MaterialGpuPrepareError2D::None};
    bool reusedPreparedPipeline{false};
    std::string diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == MaterialGpuPrepareError2D::None;
    }
};
} // namespace trace2d::render
