#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace trace2d::render
{
inline constexpr std::uint8_t NoSpriteMaskId = 0U;
inline constexpr std::uint8_t MinSpriteMaskId = 1U;
inline constexpr std::uint8_t MaxSpriteMaskId = std::numeric_limits<std::uint8_t>::max();
inline constexpr std::uint8_t NoSpriteSortingGroupId = 0U;
inline constexpr std::uint64_t InvalidSpriteStableOrder =
    std::numeric_limits<std::uint64_t>::max();

// SR4 keeps masking deliberately finite. ID 0 is reserved for `None`; 1..255 map directly to
// the SDL GPU stencil reference. Only one mask phase is active at a time in painter order.
enum class SpriteMaskMode : std::uint8_t
{
    None = 0,
    Write,
    TestInside,
    TestOutside,
};

struct SpriteMask2D final
{
    SpriteMaskMode mode{SpriteMaskMode::None};
    std::uint8_t id{NoSpriteMaskId};

    [[nodiscard]] bool operator==(const SpriteMask2D&) const noexcept = default;
};

// SR4 supports one resolved sorting-group level. Hierarchical/nested group relations are not
// representable here; scene hierarchy remains owned by #71. ID 0 means ungrouped.
struct SpriteSortingGroup2D final
{
    std::uint8_t id{NoSpriteSortingGroupId};
    std::int32_t layer{0};
    std::int32_t order{0};
    std::uint64_t stableOrder{0U};

    [[nodiscard]] bool operator==(const SpriteSortingGroup2D&) const noexcept = default;
};

struct SpriteOrder2D final
{
    std::int32_t layer{0};
    std::int32_t order{0};
    std::uint64_t stableOrder{0U};
    SpriteSortingGroup2D group{};

    [[nodiscard]] bool operator==(const SpriteOrder2D&) const noexcept = default;
};

// Lightweight caller/renderer-owned scratch entry. `sourceIndex` must initially equal the entry's
// input ordinal. ResolveSpriteOrderMask2D mutates only this scratch sequence and leaves canonical
// Sprite presentation data untouched.
struct SpriteOrderMaskEntry2D final
{
    SpriteOrder2D order{};
    SpriteMask2D mask{};
    std::uint32_t sourceIndex{0U};

    [[nodiscard]] bool operator==(const SpriteOrderMaskEntry2D&) const noexcept = default;
};

enum class SpriteOrderMaskError : std::uint8_t
{
    None = 0,
    InvalidSourceIndex,
    InvalidStableOrder,
    InvalidSortingGroup,
    InconsistentSortingGroup,
    InvalidMask,
    MaskTesterWithoutWriter,
    MaskWriterAfterTester,
    MaskPhaseReentry,
};

struct SpriteOrderMaskStatus final
{
    SpriteOrderMaskError error{SpriteOrderMaskError::None};
    std::uint32_t sourceIndex{0U};
    std::uint8_t sortingGroupId{NoSpriteSortingGroupId};
    std::uint8_t maskId{NoSpriteMaskId};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpriteOrderMaskError::None;
    }

    [[nodiscard]] bool operator==(const SpriteOrderMaskStatus&) const noexcept = default;
};

// Total semantic comparator used after each entry has passed SR4 structural validation.
// Resource/material/sampler/blend/texture identity is intentionally absent.
[[nodiscard]] bool SpriteOrderMaskLess(
    const SpriteOrderMaskEntry2D& left,
    const SpriteOrderMaskEntry2D& right) noexcept;

// Validates group/mask structure and sorts scratch entries into exact SR4 painter order.
// Complexity: O(n log n) comparisons + O(n) validation, one in-place std::sort, no explicit heap
// allocation. Exact semantic ties preserve caller order through sourceIndex.
[[nodiscard]] SpriteOrderMaskStatus ResolveSpriteOrderMask2D(
    std::span<SpriteOrderMaskEntry2D> entries) noexcept;

static_assert(std::is_trivially_copyable_v<SpriteMask2D>);
static_assert(std::is_trivially_copyable_v<SpriteSortingGroup2D>);
static_assert(std::is_trivially_copyable_v<SpriteOrder2D>);
static_assert(std::is_trivially_copyable_v<SpriteOrderMaskEntry2D>);
static_assert(std::is_trivially_copyable_v<SpriteOrderMaskStatus>);
} // namespace trace2d::render
