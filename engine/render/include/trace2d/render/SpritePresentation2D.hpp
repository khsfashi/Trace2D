#pragma once

#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpriteGeometry2D.hpp>

#include <cstdint>
#include <type_traits>

namespace trace2d::render
{
enum class SpritePresentationError : std::uint8_t
{
    None = 0,
    Geometry,
    Appearance,
};

struct SpritePresentationStatus final
{
    SpritePresentationError error{SpritePresentationError::None};
    SpriteGeometryStatus geometry{};
    SpriteAppearanceStatus appearance{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpritePresentationError::None;
    }

    [[nodiscard]] bool operator==(const SpritePresentationStatus&) const noexcept = default;
};

// Backend-independent production Sprite submission payload. `quad` is exact SR2 position/UV
// truth; `appearance` is the resolved SR3 finite color/sampling/blend contract. Texture handles,
// GPU resources, painter-order/group/mask state and batching policy remain outside this value.
struct SpritePresentation2D final
{
    SpriteDrawQuad quad{};
    SpriteAppearanceContractData appearance{};

    [[nodiscard]] bool operator==(const SpritePresentation2D&) const noexcept = default;
};

// Transactionally derives the complete SR3 backend input from one pre-resolved Sprite region.
// On failure outPresentation is reset and no partial geometry/appearance result escapes.
// O(1), fixed-size, allocation-free, no renderer/GPU/filesystem/semantic lookup work.
[[nodiscard]] SpritePresentationStatus BuildSpritePresentation2D(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    float pixelsPerUnit,
    const SpriteAppearance2D& appearance,
    SpritePresentation2D& outPresentation) noexcept;

static_assert(std::is_trivially_copyable_v<SpritePresentationStatus>);
static_assert(std::is_trivially_copyable_v<SpritePresentation2D>);
} // namespace trace2d::render
