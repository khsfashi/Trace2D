#include <trace2d/render/SpritePresentation2D.hpp>

namespace trace2d::render
{
SpritePresentationStatus BuildSpritePresentation2D(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    const float pixelsPerUnit,
    const SpriteAppearance2D& appearance,
    SpritePresentation2D& outPresentation) noexcept
{
    outPresentation = SpritePresentation2D{};

    SpritePresentation2D resolved{};
    const SpriteGeometryStatus geometry =
        BuildSpriteDrawQuad(selection, pose, pixelsPerUnit, resolved.quad);
    if (!geometry.Succeeded())
    {
        return SpritePresentationStatus{
            SpritePresentationError::Geometry,
            geometry,
            SpriteAppearanceStatus{},
        };
    }

    const SpriteAppearanceStatus appearanceStatus =
        ExtractSpriteAppearanceContract(selection, appearance, resolved.appearance);
    if (!appearanceStatus.Succeeded())
    {
        return SpritePresentationStatus{
            SpritePresentationError::Appearance,
            SpriteGeometryStatus{},
            appearanceStatus,
        };
    }

    outPresentation = resolved;
    return SpritePresentationStatus{};
}
} // namespace trace2d::render
