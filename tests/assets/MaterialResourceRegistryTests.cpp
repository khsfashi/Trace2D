#include <trace2d/assets/ResourceRegistry.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace trace2d::assets
{
namespace
{
[[nodiscard]] Shader2DResource MakeFragmentShader(std::string source = {})
{
    Shader2DResource shader{};
    shader.entryPoint = "main";
    shader.canonicalSource = source.empty()
        ? "float4 main() : SV_Target0 { return float4(1.0, 1.0, 1.0, 1.0); }"
        : std::move(source);
    return shader;
}

[[nodiscard]] MaterialParameterValue2D FloatValue(const float value)
{
    return MaterialParameterValue2D{MaterialParameterType2D::Float, {value, 0.0F, 0.0F, 0.0F}};
}

[[nodiscard]] MaterialParameterValue2D Float2Value(const float x, const float y)
{
    return MaterialParameterValue2D{MaterialParameterType2D::Float2, {x, y, 0.0F, 0.0F}};
}

[[nodiscard]] Material2DResource MakeMaterial(const ResourceHandle<Shader2DResource> shader)
{
    Material2DResource material{};
    material.shader = shader.Untyped();
    material.parameters.push_back(MaterialParameterDefault2D{"flash", FloatValue(0.0F)});
    material.parameters.push_back(MaterialParameterDefault2D{"uv_scale", Float2Value(1.0F, 1.0F)});
    return material;
}
} // namespace

TEST(MaterialResourceRegistryTests, PublishesCanonicalShaderAndMaterialWithStrongDependency)
{
    ResourceRegistry registry("project");

    const auto shader = registry.PublishShader2D("shaders/hit_flash.shader2d", MakeFragmentShader());
    ASSERT_TRUE(shader.Succeeded());
    EXPECT_EQ(shader.handle.domain, ResourceTypeDomain::Shader2D);
    ASSERT_NE(registry.Resolve(shader.handle), nullptr);

    const auto material = registry.PublishMaterial2D(
        "materials/hit_flash.material2d",
        MakeMaterial(shader.handle));
    ASSERT_TRUE(material.Succeeded());
    EXPECT_EQ(material.handle.domain, ResourceTypeDomain::Material2D);
    ASSERT_NE(registry.Resolve(material.handle), nullptr);

    const auto materialSnapshot = registry.Inspect(material.handle.Untyped());
    ASSERT_TRUE(materialSnapshot.has_value());
    ASSERT_EQ(materialSnapshot->dependencies.size(), 1U);
    EXPECT_EQ(materialSnapshot->dependencies[0].domain, ResourceTypeDomain::Shader2D);
    EXPECT_EQ(materialSnapshot->dependencies[0].canonicalReference, "shaders/hit_flash.shader2d");

    const ResourceOperationResult blocked = registry.Unload(shader.handle.Untyped());
    ASSERT_FALSE(blocked.Succeeded());
    ASSERT_TRUE(blocked.diagnostic.has_value());
    EXPECT_EQ(blocked.diagnostic->code, ResourceErrorCode::HasDependents);
    ASSERT_EQ(blocked.diagnostic->chain.size(), 1U);
    EXPECT_EQ(blocked.diagnostic->chain[0].canonicalReference, "materials/hit_flash.material2d");

    ASSERT_TRUE(registry.Unload(material.handle.Untyped()).Succeeded());
    EXPECT_TRUE(registry.Unload(shader.handle.Untyped()).Succeeded());
}

TEST(MaterialResourceRegistryTests, RejectsWrongDomainStaleAndMalformedMaterialPayloads)
{
    ResourceRegistry registry("project");

    Material2DResource wrongDomain{};
    wrongDomain.shader = ResourceHandleUntyped{0U, 1U, ResourceTypeDomain::Texture};
    const auto wrongDomainResult = registry.PublishMaterial2D("materials/wrong.material2d", wrongDomain);
    ASSERT_FALSE(wrongDomainResult.Succeeded());
    EXPECT_EQ(wrongDomainResult.diagnostic->code, ResourceErrorCode::TypeMismatch);

    const auto staleShader = registry.PublishShader2D("shaders/stale.shader2d", MakeFragmentShader());
    ASSERT_TRUE(staleShader.Succeeded());
    ASSERT_TRUE(registry.Unload(staleShader.handle.Untyped()).Succeeded());
    const auto staleMaterial = registry.PublishMaterial2D(
        "materials/stale.material2d",
        MakeMaterial(staleShader.handle));
    ASSERT_FALSE(staleMaterial.Succeeded());
    EXPECT_EQ(staleMaterial.diagnostic->code, ResourceErrorCode::DependencyNotReady);

    const auto shader = registry.PublishShader2D("shaders/live.shader2d", MakeFragmentShader());
    ASSERT_TRUE(shader.Succeeded());

    Material2DResource duplicateNames = MakeMaterial(shader.handle);
    duplicateNames.parameters.push_back(MaterialParameterDefault2D{"flash", FloatValue(1.0F)});
    const auto duplicateResult = registry.PublishMaterial2D(
        "materials/duplicate.material2d",
        std::move(duplicateNames));
    ASSERT_FALSE(duplicateResult.Succeeded());
    EXPECT_EQ(duplicateResult.diagnostic->code, ResourceErrorCode::InvalidPayload);

    Material2DResource invalidName = MakeMaterial(shader.handle);
    invalidName.parameters[0].name = "not-valid";
    const auto invalidNameResult = registry.PublishMaterial2D(
        "materials/invalid_name.material2d",
        std::move(invalidName));
    ASSERT_FALSE(invalidNameResult.Succeeded());
    EXPECT_EQ(invalidNameResult.diagnostic->code, ResourceErrorCode::InvalidPayload);

    Material2DResource nonFinite = MakeMaterial(shader.handle);
    nonFinite.parameters[0].value.lanes[0] = std::numeric_limits<float>::infinity();
    const auto nonFiniteResult = registry.PublishMaterial2D(
        "materials/non_finite.material2d",
        std::move(nonFinite));
    ASSERT_FALSE(nonFiniteResult.Succeeded());
    EXPECT_EQ(nonFiniteResult.diagnostic->code, ResourceErrorCode::InvalidPayload);

    Material2DResource oversized = MakeMaterial(shader.handle);
    oversized.parameters.clear();
    for (std::size_t index = 0U; index <= MaximumMaterial2DParameters; ++index)
    {
        oversized.parameters.push_back(MaterialParameterDefault2D{
            "p" + std::to_string(index),
            FloatValue(static_cast<float>(index))});
    }
    const auto oversizedResult = registry.PublishMaterial2D(
        "materials/oversized.material2d",
        std::move(oversized));
    ASSERT_FALSE(oversizedResult.Succeeded());
    EXPECT_EQ(oversizedResult.diagnostic->code, ResourceErrorCode::InvalidPayload);
}

TEST(MaterialResourceRegistryTests, CanonicalizesInactiveLanesAndReusesImmutablePublications)
{
    ResourceRegistry registry("project");

    const auto shader = registry.PublishShader2D("shaders/pulse.shader2d", MakeFragmentShader());
    ASSERT_TRUE(shader.Succeeded());

    Material2DResource material = MakeMaterial(shader.handle);
    material.parameters[0].value.lanes[1] = 99.0F;
    material.parameters[0].value.lanes[2] = -17.0F;
    material.parameters[0].value.lanes[3] = 5.0F;
    const auto first = registry.PublishMaterial2D("materials/pulse.material2d", material);
    ASSERT_TRUE(first.Succeeded());

    const Material2DResource* canonical = registry.Resolve(first.handle);
    ASSERT_NE(canonical, nullptr);
    EXPECT_FLOAT_EQ(canonical->parameters[0].value.lanes[0], 0.0F);
    EXPECT_FLOAT_EQ(canonical->parameters[0].value.lanes[1], 0.0F);
    EXPECT_FLOAT_EQ(canonical->parameters[0].value.lanes[2], 0.0F);
    EXPECT_FLOAT_EQ(canonical->parameters[0].value.lanes[3], 0.0F);

    Material2DResource replacement = MakeMaterial(shader.handle);
    replacement.parameters[0].value = FloatValue(1.0F);
    const auto duplicate = registry.PublishMaterial2D(
        "materials/pulse.material2d",
        std::move(replacement));
    ASSERT_TRUE(duplicate.Succeeded());
    EXPECT_TRUE(duplicate.reusedExisting);
    EXPECT_EQ(duplicate.handle, first.handle);
    EXPECT_FLOAT_EQ(registry.Resolve(duplicate.handle)->parameters[0].value.lanes[0], 0.0F);
}

TEST(MaterialResourceRegistryTests, ShaderAndMaterialInspectionReportRetainedCpuEvidence)
{
    ResourceRegistry registry("project");

    const auto shader = registry.PublishShader2D("shaders/inspect.shader2d", MakeFragmentShader());
    ASSERT_TRUE(shader.Succeeded());
    const auto material = registry.PublishMaterial2D(
        "materials/inspect.material2d",
        MakeMaterial(shader.handle));
    ASSERT_TRUE(material.Succeeded());

    const auto shaderSnapshot = registry.Inspect(shader.handle.Untyped());
    const auto materialSnapshot = registry.Inspect(material.handle.Untyped());
    ASSERT_TRUE(shaderSnapshot.has_value());
    ASSERT_TRUE(materialSnapshot.has_value());

    EXPECT_EQ(ToString(shaderSnapshot->identity.domain), "shader_2d");
    EXPECT_EQ(ToString(materialSnapshot->identity.domain), "material_2d");
    EXPECT_TRUE(shaderSnapshot->memory.cpuPayloadResident);
    EXPECT_TRUE(materialSnapshot->memory.cpuPayloadResident);
    EXPECT_EQ(shaderSnapshot->memory.cpuRetention, CpuRetentionPolicy::Required);
    EXPECT_EQ(materialSnapshot->memory.cpuRetention, CpuRetentionPolicy::Required);
    EXPECT_GT(shaderSnapshot->memory.knownRetainedCpuBytes, sizeof(Shader2DResource));
    EXPECT_GT(materialSnapshot->memory.knownRetainedCpuBytes, sizeof(Material2DResource));
    EXPECT_GT(shaderSnapshot->memory.retainedContainerCapacityBytes, 0U);
    EXPECT_GT(materialSnapshot->memory.retainedContainerCapacityBytes, 0U);
}

TEST(MaterialResourceRegistryTests, RejectsUnsupportedShaderShapeAndRetention)
{
    ResourceRegistry registry("project");

    Shader2DResource empty = MakeFragmentShader();
    empty.canonicalSource.clear();
    EXPECT_FALSE(registry.PublishShader2D("shaders/empty.shader2d", std::move(empty)).Succeeded());

    Shader2DResource invalidEntry = MakeFragmentShader();
    invalidEntry.entryPoint = "bad-entry";
    EXPECT_FALSE(registry.PublishShader2D("shaders/entry.shader2d", std::move(invalidEntry)).Succeeded());

    Shader2DResource releasable = MakeFragmentShader();
    releasable.cpuRetention = CpuRetentionPolicy::Releasable;
    EXPECT_FALSE(registry.PublishShader2D("shaders/releasable.shader2d", std::move(releasable)).Succeeded());
}
} // namespace trace2d::assets
