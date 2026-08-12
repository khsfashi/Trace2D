#pragma once

#include <trace2d/render/SpriteGeometry2D.hpp>
#include <trace2d/scene/SpriteTransform2D.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace trace2d::render
{
enum class SpritePixelPerfectError : std::uint8_t
{
    None = 0,
    InvalidLogicalSize,
    InvalidTargetSize,
    TargetTooSmall,
    InvalidCamera,
    InvalidViewport,
    InvalidPose,
    InvalidPixelsPerUnit,
    InvalidSourcePixelGrid,
    MappingOverflow,
};

enum class SpritePixelPerfectField : std::uint8_t
{
    None = 0,
    LogicalViewport,
    Target,
    Camera,
    View,
    Pose,
    PixelsPerUnit,
    SourcePixelBasis,
    SourceOrigin,
};

enum class SpritePresentationTimeMode : std::uint8_t
{
    AuthoritativeCurrent = 0,
    Interpolated = 1,
};

[[nodiscard]] std::string_view ToString(SpritePixelPerfectError value) noexcept;
[[nodiscard]] std::string_view ToString(SpritePixelPerfectField value) noexcept;
[[nodiscard]] std::string_view ToString(SpritePresentationTimeMode value) noexcept;

struct SpritePixelPerfectStatus final
{
    SpritePixelPerfectError error{SpritePixelPerfectError::None};
    SpritePixelPerfectField field{SpritePixelPerfectField::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpritePixelPerfectError::None;
    }

    [[nodiscard]] bool operator==(const SpritePixelPerfectStatus&) const noexcept = default;
};

struct SpritePixelRect2D final
{
    std::uint32_t x{0U};
    std::uint32_t y{0U};
    std::uint32_t width{0U};
    std::uint32_t height{0U};

    [[nodiscard]] bool operator==(const SpritePixelRect2D&) const noexcept = default;
};

// Backend-independent SR6 frame mapping. `logicalView` is the exact view used both by
// pixel-grid resolution and production Sprite vertex conversion. `contentRect` is a final-target
// pixel rectangle with integer scale and deterministic centered remainder placement.
struct SpritePixelPerfectViewport2D final
{
    std::uint32_t logicalWidth{0U};
    std::uint32_t logicalHeight{0U};
    std::uint32_t targetWidth{0U};
    std::uint32_t targetHeight{0U};
    std::uint32_t integerScale{0U};
    SpritePixelRect2D contentRect{};
    OrthographicView logicalView{};

    [[nodiscard]] bool operator==(const SpritePixelPerfectViewport2D&) const noexcept = default;
};

struct SpritePixelPerfectPoseRequest final
{
    SpritePresentationTimeMode timeMode{SpritePresentationTimeMode::AuthoritativeCurrent};
    float interpolationAlpha{1.0F};

    [[nodiscard]] bool operator==(const SpritePixelPerfectPoseRequest&) const noexcept = default;
};

// Deterministic evidence for one derived Sprite presentation pose. The source origin is the
// untrimmed source pixel-edge (0,0); trim, pivot and packed atlas rotation do not replace it.
struct SpritePixelPerfectMapping2D final
{
    scene::SpritePose2D presentationPose{};
    Float2 sourceOriginLogicalBeforeSnap{};
    Float2 sourceOriginLogicalAfterSnap{};
    Float2 sourcePixelBasisXLogical{};
    Float2 sourcePixelBasisYLogical{};
    Float2 worldSnapDelta{};
    std::uint32_t sourcePixelScaleX{0U};
    std::uint32_t sourcePixelScaleY{0U};
    bool axesSwapped{false};

    [[nodiscard]] bool operator==(const SpritePixelPerfectMapping2D&) const noexcept = default;
};

// O(1), fixed-size, allocation-free logical-reference -> final-target mapping. The integer scale
// is floor(min(target/logical)); target-too-small is explicit instead of falling back to fractional
// scaling. The existing OrthographicCamera is resolved against the logical aspect only.
[[nodiscard]] SpritePixelPerfectStatus BuildSpritePixelPerfectViewport(
    const OrthographicCamera& camera,
    std::uint32_t logicalWidth,
    std::uint32_t logicalHeight,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight,
    SpritePixelPerfectViewport2D& outViewport) noexcept;

// Validates an already-built mapping before a renderer/backend consumes it. This protects the
// production GPU path from stale/corrupted target rectangles without making GPU state authority.
[[nodiscard]] SpritePixelPerfectStatus ValidateSpritePixelPerfectViewport(
    const SpritePixelPerfectViewport2D& viewport) noexcept;

// Resolves SR1 exact/interpolated history first, proves the transformed source-pixel grid maps to
// axis-aligned integer logical-pixel basis vectors, then snaps only the derived pose. Half-pixel
// ties use floor(x + 0.5), which is integer-translation invariant. Authoritative history is never
// mutated. O(1), fixed-size, allocation-free, no file/name/image/GPU/readback/fence work.
[[nodiscard]] SpritePixelPerfectStatus ResolveSpritePixelPerfectPose(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePoseHistory2D& history,
    float pixelsPerUnit,
    const SpritePixelPerfectViewport2D& viewport,
    const SpritePixelPerfectPoseRequest& request,
    SpritePixelPerfectMapping2D& outMapping) noexcept;

static_assert(std::is_trivially_copyable_v<SpritePixelPerfectStatus>);
static_assert(std::is_trivially_copyable_v<SpritePixelRect2D>);
static_assert(std::is_trivially_copyable_v<SpritePixelPerfectViewport2D>);
static_assert(std::is_trivially_copyable_v<SpritePixelPerfectPoseRequest>);
static_assert(std::is_trivially_copyable_v<SpritePixelPerfectMapping2D>);
} // namespace trace2d::render
