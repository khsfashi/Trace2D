#pragma once

#include <trace2d/render/RenderData.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace trace2d::render
{
enum class RendererWorkloadId : std::uint8_t
{
    DenseSingleTexture,
    AlternatingTwoTextures,
    InterleavedCulling,
};

struct RendererWorkloadSpec final
{
    RendererWorkloadId id{RendererWorkloadId::DenseSingleTexture};
    std::string_view name{};
    std::uint32_t targetWidth{0};
    std::uint32_t targetHeight{0};
    OrthographicCamera camera{};
    std::uint32_t authoredSpriteCount{0};
    std::uint32_t textureSlotCount{0};
};

struct RendererWorkload final
{
    RendererWorkloadSpec spec{};
    std::vector<SpriteRenderData> sprites{};
};

struct RendererWorkloadStructure final
{
    std::uint64_t authoredSprites{0};
    std::uint64_t visibleSprites{0};
    std::uint64_t culledSprites{0};
    std::uint64_t contiguousTextureRuns{0};

    [[nodiscard]] bool operator==(const RendererWorkloadStructure&) const noexcept = default;
};

inline constexpr std::size_t RendererWorkloadCount = 3;

[[nodiscard]] std::span<const RendererWorkloadSpec> RendererWorkloadSpecs() noexcept;
[[nodiscard]] std::string_view ToString(RendererWorkloadId id) noexcept;
[[nodiscard]] bool TryParseRendererWorkloadId(std::string_view name, RendererWorkloadId& outId) noexcept;

[[nodiscard]] RendererWorkload BuildRendererWorkload(
    RendererWorkloadId id,
    std::span<const TextureHandle> textureSlots);

[[nodiscard]] RendererWorkloadStructure MeasureRendererWorkloadStructure(
    const RendererWorkload& workload) noexcept;
} // namespace trace2d::render
