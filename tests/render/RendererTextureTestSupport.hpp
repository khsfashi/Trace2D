#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/render/Renderer.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace trace2d::render::test
{
[[nodiscard]] constexpr TextureHandle TextureHandleFixture(
    const std::uint32_t slot,
    const std::uint32_t generation = 1U) noexcept
{
    return TextureHandle{slot, generation, assets::ResourceTypeDomain::Texture};
}

class TextureResourceFixture final
{
public:
    [[nodiscard]] TextureHandle Publish(
        const Rgba8TextureData& textureData,
        const assets::TextureResourceColorSpace colorSpace =
            assets::TextureResourceColorSpace::Linear)
    {
        assets::TextureResource resource{};
        resource.width = textureData.width;
        resource.height = textureData.height;
        resource.colorSpace = colorSpace;
        resource.alphaMode = assets::TextureResourceAlphaMode::Straight;
        resource.cpuRetention = assets::CpuRetentionPolicy::Reacquirable;
        resource.retentionReason = "test fixture can recreate canonical RGBA8 pixels";
        resource.canonicalRgba8.assign(textureData.pixels.begin(), textureData.pixels.end());

        const std::string reference =
            "tests/render/runtime/texture-" + std::to_string(nextReference_++) + ".rgba8";
        const auto published = resources_.PublishTexture(reference, std::move(resource));
        if (!published.Succeeded())
        {
            throw std::runtime_error{
                published.diagnostic.has_value()
                    ? published.diagnostic->message
                    : "Texture resource fixture publish failed."};
        }
        return published.handle;
    }

    [[nodiscard]] assets::ResourceRegistry& Resources() noexcept
    {
        return resources_;
    }

private:
    assets::ResourceRegistry resources_{"."};
    std::uint64_t nextReference_{0U};
};
} // namespace trace2d::render::test
