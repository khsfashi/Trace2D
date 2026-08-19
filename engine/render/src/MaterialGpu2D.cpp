#include <trace2d/render/MaterialGpu2D.hpp>

namespace trace2d::render
{
std::string_view ToString(const MaterialGpuPrepareError2D value) noexcept
{
    switch (value)
    {
    case MaterialGpuPrepareError2D::None:
        return "none";
    case MaterialGpuPrepareError2D::InvalidMaterialHandle:
        return "invalid_material_handle";
    case MaterialGpuPrepareError2D::InvalidShaderHandle:
        return "invalid_shader_handle";
    case MaterialGpuPrepareError2D::UnsupportedShaderLanguage:
        return "unsupported_shader_language";
    case MaterialGpuPrepareError2D::UnsupportedShaderStage:
        return "unsupported_shader_stage";
    case MaterialGpuPrepareError2D::InvalidShaderEntryPoint:
        return "invalid_shader_entry_point";
    case MaterialGpuPrepareError2D::ParameterPreparationFailed:
        return "parameter_preparation_failed";
    case MaterialGpuPrepareError2D::ShaderCompilationFailed:
        return "shader_compilation_failed";
    case MaterialGpuPrepareError2D::ShaderReflectionFailed:
        return "shader_reflection_failed";
    case MaterialGpuPrepareError2D::ShaderContractMismatch:
        return "shader_contract_mismatch";
    case MaterialGpuPrepareError2D::PipelineCreationFailed:
        return "pipeline_creation_failed";
    }
    return "unknown";
}
} // namespace trace2d::render
