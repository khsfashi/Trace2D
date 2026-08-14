#include <trace2d/assets/ResourceRegistry.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::assets
{
namespace
{
[[nodiscard]] TextureResource MakeTexture(
    std::uint32_t width = 1U,
    std::uint32_t height = 1U,
    CpuRetentionPolicy retention = CpuRetentionPolicy::Reacquirable)
{
    TextureResource resource{};
    resource.width = width;
    resource.height = height;
    resource.cpuRetention = retention;
    resource.retentionReason = retention == CpuRetentionPolicy::Required ? "CPU copy required" : "package can reacquire pixels";
    resource.canonicalRgba8.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0x7fU);
    return resource;
}

[[nodiscard]] SpriteResource MakeSprite(std::string id = "hero")
{
    SpriteResource resource{};
    resource.asset.id = std::move(id);
    resource.asset.schemaVersion = 1U;
    resource.asset.pages.push_back(SpriteAtlasPage{
        "main",
        "content/hero.png",
        SpritePixelSize{1U, 1U},
        SpriteColorSpace::Srgb,
        SpriteAlphaMode::Straight});
    return resource;
}
} // namespace

TEST(ResourceRegistryTests, CanonicalIdentityRejectsAbsoluteTraversalAndPortableDrivePaths)
{
    ResourceRegistry registry("project");

    const auto normalized = registry.PublishTexture("./content//characters\\hero.png", MakeTexture());
    ASSERT_TRUE(normalized.Succeeded());
    const auto snapshot = registry.Inspect(normalized.handle.Untyped());
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->identity.domain, ResourceTypeDomain::Texture);
    EXPECT_EQ(snapshot->identity.canonicalReference, "content/characters/hero.png");

    const auto absolute = registry.PublishTexture("/content/hero.png", MakeTexture());
    ASSERT_FALSE(absolute.Succeeded());
    EXPECT_EQ(absolute.diagnostic->code, ResourceErrorCode::InvalidReference);

    const auto traversal = registry.PublishTexture("content/../secret.png", MakeTexture());
    ASSERT_FALSE(traversal.Succeeded());
    EXPECT_EQ(traversal.diagnostic->code, ResourceErrorCode::InvalidReference);

    const auto drive = registry.PublishTexture("C:/content/hero.png", MakeTexture());
    ASSERT_FALSE(drive.Succeeded());
    EXPECT_EQ(drive.diagnostic->code, ResourceErrorCode::InvalidReference);
}

TEST(ResourceRegistryTests, DuplicateReadyPublishReusesSlotAndTypedDomainMismatchDoesNotResolve)
{
    ResourceRegistry registry("project");

    const auto first = registry.PublishTexture("content/hero.png", MakeTexture());
    ASSERT_TRUE(first.Succeeded());
    const TextureResource* firstResource = registry.Resolve(first.handle);
    ASSERT_NE(firstResource, nullptr);

    TextureResource replacement = MakeTexture();
    replacement.canonicalRgba8[0] = 0xffU;
    const auto duplicate = registry.PublishTexture("content/hero.png", std::move(replacement));
    ASSERT_TRUE(duplicate.Succeeded());
    EXPECT_TRUE(duplicate.reusedExisting);
    EXPECT_EQ(duplicate.handle, first.handle);
    EXPECT_EQ(registry.Resolve(duplicate.handle), firstResource);
    EXPECT_EQ(registry.Resolve(duplicate.handle)->canonicalRgba8[0], 0x7fU);

    const ResourceHandle<SpriteResource> wrongDomain{
        first.handle.slot,
        first.handle.generation,
        ResourceTypeDomain::Sprite};
    EXPECT_EQ(registry.Resolve(wrongDomain), nullptr);

    const ResourceRegistryStats stats = registry.Stats();
    EXPECT_EQ(stats.duplicateReadyLoads, 1U);
    EXPECT_EQ(stats.readyResources, 1U);
}

TEST(ResourceRegistryTests, FailedLoadRequiresExplicitInvalidationBeforeRetry)
{
    ResourceRegistry registry("project");

    const ResourceOperationResult recorded = registry.RecordLoadFailure(
        ResourceTypeDomain::Texture,
        "content/missing.png",
        ResourceErrorCode::InvalidPayload,
        "decode failed");
    ASSERT_TRUE(recorded.Succeeded());

    const auto blockedRetry = registry.PublishTexture("content/missing.png", MakeTexture());
    ASSERT_FALSE(blockedRetry.Succeeded());
    EXPECT_EQ(blockedRetry.diagnostic->code, ResourceErrorCode::RetryRequiresInvalidation);

    ASSERT_TRUE(registry.Invalidate(ResourceTypeDomain::Texture, "content/missing.png").Succeeded());
    const auto retry = registry.PublishTexture("content/missing.png", MakeTexture());
    EXPECT_TRUE(retry.Succeeded());
    EXPECT_EQ(registry.Stats().errorResources, 0U);
}

TEST(ResourceRegistryTests, StaleGenerationCannotAliasReusedSlot)
{
    ResourceRegistry registry("project");

    const auto first = registry.PublishTexture("content/a.png", MakeTexture());
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(registry.Unload(first.handle.Untyped()).Succeeded());
    EXPECT_EQ(registry.Resolve(first.handle), nullptr);

    const auto second = registry.PublishTexture("content/b.png", MakeTexture());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(second.handle.slot, first.handle.slot);
    EXPECT_NE(second.handle.generation, first.handle.generation);
    EXPECT_EQ(registry.Resolve(first.handle), nullptr);
    EXPECT_NE(registry.Resolve(second.handle), nullptr);
}

TEST(ResourceRegistryTests, StrongDependenciesRejectCyclesAndBlockUnloadBeneathDependents)
{
    ResourceRegistry registry("project");

    const auto texture = registry.PublishTexture("content/hero.png", MakeTexture());
    ASSERT_TRUE(texture.Succeeded());
    const std::array<ResourceHandleUntyped, 1> spriteDependencies{texture.handle.Untyped()};
    const auto sprite = registry.PublishSprite("content/hero.sprite.toml", MakeSprite(), spriteDependencies);
    ASSERT_TRUE(sprite.Succeeded());

    const ResourceOperationResult blockedUnload = registry.Unload(texture.handle.Untyped());
    ASSERT_FALSE(blockedUnload.Succeeded());
    EXPECT_EQ(blockedUnload.diagnostic->code, ResourceErrorCode::HasDependents);
    ASSERT_EQ(blockedUnload.diagnostic->chain.size(), 1U);
    EXPECT_EQ(blockedUnload.diagnostic->chain[0].canonicalReference, "content/hero.sprite.toml");

    const std::array<ResourceHandleUntyped, 1> cycleDependency{sprite.handle.Untyped()};
    const ResourceOperationResult cycle = registry.SetStrongDependencies(texture.handle.Untyped(), cycleDependency);
    ASSERT_FALSE(cycle.Succeeded());
    EXPECT_EQ(cycle.diagnostic->code, ResourceErrorCode::DependencyCycle);
    ASSERT_GE(cycle.diagnostic->chain.size(), 3U);

    ASSERT_TRUE(registry.Unload(sprite.handle.Untyped()).Succeeded());
    ASSERT_TRUE(registry.Unload(texture.handle.Untyped()).Succeeded());
}

TEST(ResourceRegistryTests, TextureCpuAndRendererResidencyAreIndependentAndMetadataSurvivesRelease)
{
    ResourceRegistry registry("project");
    TextureResource payload = MakeTexture(2U, 2U, CpuRetentionPolicy::Reacquirable);
    payload.colorSpace = TextureResourceColorSpace::Linear;
    payload.alphaMode = TextureResourceAlphaMode::Premultiplied;

    const auto texture = registry.PublishTexture("content/runtime.png", std::move(payload));
    ASSERT_TRUE(texture.Succeeded());
    ASSERT_TRUE(registry.SetTextureRendererResidency(texture.handle, true, 256U).Succeeded());

    const auto before = registry.Inspect(texture.handle.Untyped());
    ASSERT_TRUE(before.has_value());
    EXPECT_TRUE(before->memory.cpuPayloadResident);
    EXPECT_TRUE(before->memory.rendererResident);
    EXPECT_EQ(before->memory.knownRendererGpuBytes, 256U);
    EXPECT_EQ(before->memory.cpuRetention, CpuRetentionPolicy::Reacquirable);

    ASSERT_TRUE(registry.ReleaseTextureCpuPayload(texture.handle).Succeeded());
    const TextureResource* resolved = registry.Resolve(texture.handle);
    ASSERT_NE(resolved, nullptr);
    EXPECT_TRUE(resolved->canonicalRgba8.empty());
    EXPECT_EQ(resolved->width, 2U);
    EXPECT_EQ(resolved->height, 2U);
    EXPECT_EQ(resolved->colorSpace, TextureResourceColorSpace::Linear);
    EXPECT_EQ(resolved->alphaMode, TextureResourceAlphaMode::Premultiplied);

    const auto after = registry.Inspect(texture.handle.Untyped());
    ASSERT_TRUE(after.has_value());
    EXPECT_FALSE(after->memory.cpuPayloadResident);
    EXPECT_TRUE(after->memory.rendererResident);
    EXPECT_EQ(after->memory.knownRendererGpuBytes, 256U);
}

TEST(ResourceRegistryTests, RequiredTextureCpuRetentionRejectsRelease)
{
    ResourceRegistry registry("project");
    const auto texture = registry.PublishTexture(
        "content/readback.png",
        MakeTexture(1U, 1U, CpuRetentionPolicy::Required));
    ASSERT_TRUE(texture.Succeeded());

    const ResourceOperationResult release = registry.ReleaseTextureCpuPayload(texture.handle);
    ASSERT_FALSE(release.Succeeded());
    EXPECT_EQ(release.diagnostic->code, ResourceErrorCode::CpuRetentionRequired);
    ASSERT_NE(registry.Resolve(texture.handle), nullptr);
    EXPECT_FALSE(registry.Resolve(texture.handle)->canonicalRgba8.empty());
}

TEST(ResourceRegistryTests, ReleaseUnusedHonorsExplicitRetainsAndStrongDependencyReachability)
{
    ResourceRegistry registry("project");
    const auto texture = registry.PublishTexture("content/hero.png", MakeTexture());
    ASSERT_TRUE(texture.Succeeded());
    const std::array<ResourceHandleUntyped, 1> dependencies{texture.handle.Untyped()};
    const auto sprite = registry.PublishSprite("content/hero.sprite.toml", MakeSprite(), dependencies);
    ASSERT_TRUE(sprite.Succeeded());
    ASSERT_TRUE(registry.Retain(sprite.handle.Untyped()).Succeeded());

    EXPECT_EQ(registry.ReleaseUnused(), 0U);
    ASSERT_NE(registry.Resolve(texture.handle), nullptr);
    ASSERT_NE(registry.Resolve(sprite.handle), nullptr);

    ASSERT_TRUE(registry.Release(sprite.handle.Untyped()).Succeeded());
    EXPECT_EQ(registry.ReleaseUnused(), 2U);
    EXPECT_EQ(registry.Resolve(texture.handle), nullptr);
    EXPECT_EQ(registry.Resolve(sprite.handle), nullptr);
}

TEST(ResourceRegistryTests, ProjectClearUsesDeterministicDependentBeforeDependencyOrder)
{
    ResourceRegistry registry("project");
    const auto textureA = registry.PublishTexture("content/a.png", MakeTexture());
    const auto textureB = registry.PublishTexture("content/b.png", MakeTexture());
    ASSERT_TRUE(textureA.Succeeded());
    ASSERT_TRUE(textureB.Succeeded());

    const std::array<ResourceHandleUntyped, 2> dependencies{
        textureB.handle.Untyped(),
        textureA.handle.Untyped()};
    const auto sprite = registry.PublishSprite("content/hero.sprite.toml", MakeSprite(), dependencies);
    ASSERT_TRUE(sprite.Succeeded());
    ASSERT_TRUE(registry.Retain(textureA.handle.Untyped()).Succeeded());

    const ResourceClearReport report = registry.ClearProjectResources();
    ASSERT_EQ(report.unloadOrder.size(), 3U);
    EXPECT_EQ(report.unloadOrder[0].canonicalReference, "content/hero.sprite.toml");
    EXPECT_EQ(report.unloadOrder[1].canonicalReference, "content/a.png");
    EXPECT_EQ(report.unloadOrder[2].canonicalReference, "content/b.png");
    EXPECT_EQ(registry.Stats().readyResources, 0U);
}

TEST(ResourceRegistryTests, RetainedSteadyStateResolvePerformsNoCanonicalizationOrFilesystemWork)
{
    ResourceRegistry registry("project");
    const auto texture = registry.PublishTexture("content/steady.png", MakeTexture());
    ASSERT_TRUE(texture.Succeeded());
    ASSERT_TRUE(registry.Retain(texture.handle.Untyped()).Succeeded());

    const ResourceRegistryStats before = registry.Stats();
    for (std::size_t index = 0U; index < 10000U; ++index)
    {
        const TextureResource* resolved = registry.Resolve(texture.handle);
        ASSERT_NE(resolved, nullptr);
        EXPECT_EQ(resolved->width, 1U);
    }
    const ResourceRegistryStats after = registry.Stats();

    EXPECT_EQ(after.canonicalizationCalls, before.canonicalizationCalls);
    EXPECT_EQ(after.filesystemQueries, 0U);
    EXPECT_EQ(before.filesystemQueries, 0U);
}
} // namespace trace2d::assets
