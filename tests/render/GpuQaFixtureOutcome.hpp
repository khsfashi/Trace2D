#pragma once

#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <iostream>
#include <string_view>

namespace trace2d::test
{
enum class GpuQaFailureCategory
{
    None,
    UnsupportedCapability,
    GpuDeviceInitializationFailure,
    ShaderCompileOrReflectionFailure,
    PipelineOrResourceCreationFailure,
    RenderSubmitOrDeviceLossFailure,
    ReadbackOrCaptureFailure,
    ComparisonMismatch,
};

[[nodiscard]] constexpr std::string_view ToString(const GpuQaFailureCategory category) noexcept
{
    switch (category)
    {
    case GpuQaFailureCategory::None:
        return "none";
    case GpuQaFailureCategory::UnsupportedCapability:
        return "unsupported_capability";
    case GpuQaFailureCategory::GpuDeviceInitializationFailure:
        return "gpu_device_initialization_failure";
    case GpuQaFailureCategory::ShaderCompileOrReflectionFailure:
        return "shader_compile_or_reflection_failure";
    case GpuQaFailureCategory::PipelineOrResourceCreationFailure:
        return "pipeline_or_resource_creation_failure";
    case GpuQaFailureCategory::RenderSubmitOrDeviceLossFailure:
        return "render_submit_or_device_loss_failure";
    case GpuQaFailureCategory::ReadbackOrCaptureFailure:
        return "readback_or_capture_failure";
    case GpuQaFailureCategory::ComparisonMismatch:
        return "comparison_mismatch";
    }
    return "unknown";
}

[[nodiscard]] constexpr GpuQaFailureCategory CategoryForMaterialPrepareError(
    const render::MaterialGpuPrepareError2D error) noexcept
{
    using render::MaterialGpuPrepareError2D;
    switch (error)
    {
    case MaterialGpuPrepareError2D::ShaderCompilationFailed:
    case MaterialGpuPrepareError2D::ShaderContractMismatch:
        return GpuQaFailureCategory::ShaderCompileOrReflectionFailure;
    case MaterialGpuPrepareError2D::PipelineCreationFailed:
    case MaterialGpuPrepareError2D::InvalidMaterialHandle:
        return GpuQaFailureCategory::PipelineOrResourceCreationFailure;
    case MaterialGpuPrepareError2D::None:
        return GpuQaFailureCategory::None;
    }
    return GpuQaFailureCategory::PipelineOrResourceCreationFailure;
}

class GpuQaFixtureOutcome final
{
public:
    explicit GpuQaFixtureOutcome(const std::string_view testName) noexcept
        : testName_(testName)
    {
    }

    GpuQaFixtureOutcome(const GpuQaFixtureOutcome&) = delete;
    GpuQaFixtureOutcome& operator=(const GpuQaFixtureOutcome&) = delete;

    ~GpuQaFixtureOutcome()
    {
        if (::testing::Test::IsSkipped())
        {
            Emit("skipped", GpuQaFailureCategory::UnsupportedCapability);
            return;
        }
        if (::testing::Test::HasFailure())
        {
            Emit(phase_, category_);
            return;
        }
        Emit("complete", GpuQaFailureCategory::None);
    }

    void SetFailurePoint(
        const std::string_view phase,
        const GpuQaFailureCategory category) noexcept
    {
        phase_ = phase;
        category_ = category;
    }

    void SetMaterialPrepareFailure(
        const render::MaterialGpuPrepareError2D error) noexcept
    {
        if (error == render::MaterialGpuPrepareError2D::None)
        {
            return;
        }
        phase_ = "material_prepare";
        category_ = CategoryForMaterialPrepareError(error);
    }

private:
    void Emit(
        const std::string_view phase,
        const GpuQaFailureCategory category) const
    {
        std::cout
            << "TRACE2D_GPUQA_FIXTURE_V1"
            << " test=" << testName_
            << " phase=" << phase
            << " failure_category=" << ToString(category)
            << '\n';
    }

    std::string_view testName_{};
    std::string_view phase_{"fixture_setup"};
    GpuQaFailureCategory category_{GpuQaFailureCategory::PipelineOrResourceCreationFailure};
};
} // namespace trace2d::test
