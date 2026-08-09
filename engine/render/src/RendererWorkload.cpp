#include <trace2d/render/RendererWorkload.hpp>

#include <array>
#include <stdexcept>

namespace trace2d::render
{
namespace
{
constexpr std::array<RendererWorkloadSpec, RendererWorkloadCount> WorkloadSpecs{
    RendererWorkloadSpec{
        RendererWorkloadId::DenseSingleTexture,
        "dense_single_texture",
        1280,
        720,
        OrthographicCamera{Float2{0.0F, 0.0F}, 24.0F},
        400,
        1,
    },
    RendererWorkloadSpec{
        RendererWorkloadId::AlternatingTwoTextures,
        "alternating_two_textures",
        1280,
        720,
        OrthographicCamera{Float2{0.0F, 0.0F}, 24.0F},
        400,
        2,
    },
    RendererWorkloadSpec{
        RendererWorkloadId::InterleavedCulling,
        "interleaved_culling",
        1280,
        720,
        OrthographicCamera{Float2{0.0F, 0.0F}, 24.0F},
        600,
        2,
    },
};

[[nodiscard]] const RendererWorkloadSpec* FindSpec(const RendererWorkloadId id) noexcept
{
    for (const RendererWorkloadSpec& spec : WorkloadSpecs)
    {
        if (spec.id == id)
        {
            return &spec;
        }
    }

    return nullptr;
}

[[nodiscard]] Float2 VisibleGridPosition(const std::uint32_t index) noexcept
{
    constexpr std::uint32_t Columns = 20;
    const std::uint32_t column = index % Columns;
    const std::uint32_t row = index / Columns;
    return Float2{
        -9.5F + static_cast<float>(column),
        -9.5F + static_cast<float>(row),
    };
}

void AppendSprite(
    RendererWorkload& workload,
    const Float2 center,
    const TextureHandle texture)
{
    SpriteRenderData sprite{};
    sprite.center = center;
    sprite.halfExtents = Float2{0.45F, 0.45F};
    sprite.texture = texture;
    sprite.layer = 0;
    sprite.stableOrder = static_cast<std::uint64_t>(workload.sprites.size());
    workload.sprites.push_back(sprite);
}
} // namespace

std::span<const RendererWorkloadSpec> RendererWorkloadSpecs() noexcept
{
    return WorkloadSpecs;
}

std::string_view ToString(const RendererWorkloadId id) noexcept
{
    const RendererWorkloadSpec* const spec = FindSpec(id);
    return spec == nullptr ? std::string_view{} : spec->name;
}

bool TryParseRendererWorkloadId(const std::string_view name, RendererWorkloadId& outId) noexcept
{
    for (const RendererWorkloadSpec& spec : WorkloadSpecs)
    {
        if (spec.name == name)
        {
            outId = spec.id;
            return true;
        }
    }

    return false;
}

RendererWorkload BuildRendererWorkload(
    const RendererWorkloadId id,
    const std::span<const TextureHandle> textureSlots)
{
    const RendererWorkloadSpec* const spec = FindSpec(id);
    if (spec == nullptr)
    {
        throw std::invalid_argument{"Unknown renderer workload id."};
    }

    if (textureSlots.size() < spec->textureSlotCount)
    {
        throw std::invalid_argument{"Renderer workload requires more texture slots."};
    }

    for (std::uint32_t index = 0; index < spec->textureSlotCount; ++index)
    {
        if (textureSlots[index] == InvalidTextureHandle)
        {
            throw std::invalid_argument{"Renderer workload texture slots must be valid handles."};
        }
    }

    RendererWorkload workload{};
    workload.spec = *spec;
    workload.sprites.reserve(spec->authoredSpriteCount);

    switch (id)
    {
    case RendererWorkloadId::DenseSingleTexture:
        for (std::uint32_t index = 0; index < 400; ++index)
        {
            AppendSprite(workload, VisibleGridPosition(index), textureSlots[0]);
        }
        break;

    case RendererWorkloadId::AlternatingTwoTextures:
        for (std::uint32_t index = 0; index < 400; ++index)
        {
            const TextureHandle texture = textureSlots[index % 2U];
            AppendSprite(workload, VisibleGridPosition(index), texture);
        }
        break;

    case RendererWorkloadId::InterleavedCulling:
        for (std::uint32_t group = 0; group < 200; ++group)
        {
            AppendSprite(workload, VisibleGridPosition(group * 2U), textureSlots[0]);
            AppendSprite(
                workload,
                Float2{100.0F + static_cast<float>(group), 100.0F},
                textureSlots[1]);
            AppendSprite(workload, VisibleGridPosition(group * 2U + 1U), textureSlots[0]);
        }
        break;
    }

    if (workload.sprites.size() != spec->authoredSpriteCount)
    {
        throw std::logic_error{"Renderer workload authored sprite count drifted from its committed spec."};
    }

    return workload;
}

RendererWorkloadStructure MeasureRendererWorkloadStructure(const RendererWorkload& workload) noexcept
{
    RendererWorkloadStructure structure{};
    structure.authoredSprites = workload.sprites.size();

    OrthographicView view{};
    if (!TryBuildOrthographicView(
            workload.spec.camera,
            workload.spec.targetWidth,
            workload.spec.targetHeight,
            view))
    {
        return structure;
    }

    const SpriteBatchMeasurement measurement = MeasureContiguousTextureBatching(view, workload.sprites);
    structure.visibleSprites = measurement.visibleSprites;
    structure.culledSprites = measurement.culledSprites;
    structure.contiguousTextureRuns = measurement.contiguousTextureRuns;
    return structure;
}
} // namespace trace2d::render
