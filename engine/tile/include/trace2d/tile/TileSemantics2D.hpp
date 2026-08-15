#pragma once

#include <trace2d/tile/TileMap.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace trace2d::tile
{
enum class TileCollisionHandoff : std::uint8_t
{
    None = 0,
    Solid,
};

enum class TileNavigationHandoff : std::uint8_t
{
    None = 0,
    Walkable,
    Blocked,
};

enum class TileOcclusionHandoff : std::uint8_t
{
    None = 0,
    Opaque,
};

[[nodiscard]] std::string_view ToString(TileCollisionHandoff value) noexcept;
[[nodiscard]] std::string_view ToString(TileNavigationHandoff value) noexcept;
[[nodiscard]] std::string_view ToString(TileOcclusionHandoff value) noexcept;

struct TileSemanticRuleDocument final
{
    std::string tileSemanticId{};
    TileCollisionHandoff collision{TileCollisionHandoff::None};
    TileNavigationHandoff navigation{TileNavigationHandoff::None};
    TileOcclusionHandoff occlusion{TileOcclusionHandoff::None};

    [[nodiscard]] bool operator==(const TileSemanticRuleDocument&) const noexcept = default;
};

struct TileMarkerDocument final
{
    std::string semanticId{};
    std::string kind{};
    std::string layerSemanticId{};
    std::int32_t worldX{0};
    std::int32_t worldY{0};
    std::vector<std::string> tags{};

    [[nodiscard]] bool operator==(const TileMarkerDocument&) const noexcept = default;
};

struct TileSemanticOverlayDocument final
{
    static constexpr std::int64_t FormatVersion = 1;

    std::string semanticId{};
    std::string tileSetSemanticId{};
    std::string tileMapSemanticId{};
    std::vector<TileSemanticRuleDocument> rules{};
    std::vector<TileMarkerDocument> markers{};

    [[nodiscard]] bool operator==(const TileSemanticOverlayDocument&) const noexcept = default;
};

struct TileSemanticOverlayLoadResult final
{
    std::optional<TileSemanticOverlayDocument> document{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

// One compact entry per TileSet definition. Runtime cells keep only their existing
// uint32 tile index; no semantic strings/maps/objects are copied into the grid.
struct CompiledTileSemanticState final
{
    TileCollisionHandoff collision{TileCollisionHandoff::None};
    TileNavigationHandoff navigation{TileNavigationHandoff::None};
    TileOcclusionHandoff occlusion{TileOcclusionHandoff::None};
    std::uint8_t reserved{0};

    [[nodiscard]] bool operator==(const CompiledTileSemanticState&) const noexcept = default;
};

static_assert(std::is_trivially_copyable_v<CompiledTileSemanticState>);
static_assert(sizeof(CompiledTileSemanticState) == 4U);

struct CompiledTileMarker final
{
    std::string semanticId{};
    std::string kind{};
    std::uint32_t layerIndex{0};
    std::int32_t worldX{0};
    std::int32_t worldY{0};
    std::vector<std::string> tags{};
};

struct CompiledTileSemanticOverlay final
{
    std::string semanticId{};
    std::string tileSetSemanticId{};
    std::string tileMapSemanticId{};
    std::vector<CompiledTileSemanticState> tileStates{};
    std::vector<CompiledTileMarker> markers{};
};

struct TileSemanticOverlayCompileResult final
{
    std::optional<CompiledTileSemanticOverlay> overlay{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return overlay.has_value() && diagnostics.empty();
    }
};

struct TileMarkerInspection final
{
    std::size_t markerIndex{0};
    std::string_view semanticId{};
    std::string_view kind{};
    std::size_t layerIndex{0};
    std::string_view layerSemanticId{};
    std::int32_t worldX{0};
    std::int32_t worldY{0};
    std::span<const std::string> tags{};
};

[[nodiscard]] TileSemanticOverlayLoadResult ParseTileSemanticOverlayToml(
    std::string_view text,
    std::string_view sourceName = {});
[[nodiscard]] std::string SaveTileSemanticOverlayToml(const TileSemanticOverlayDocument& document);

[[nodiscard]] std::vector<TileDiagnostic> ValidateTileSemanticOverlay(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileSemanticOverlayDocument& document);
[[nodiscard]] TileSemanticOverlayCompileResult CompileTileSemanticOverlay(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileSemanticOverlayDocument& document);

[[nodiscard]] const CompiledTileSemanticState* SemanticStateForTile(
    const CompiledTileSemanticOverlay& overlay,
    std::uint32_t tileIndex) noexcept;
[[nodiscard]] const CompiledTileSemanticState* SemanticStateForCell(
    const CompiledTileSemanticOverlay& overlay,
    const CompiledTileMap& tileMap,
    std::size_t layerIndex,
    std::int32_t worldX,
    std::int32_t worldY) noexcept;
[[nodiscard]] std::optional<std::size_t> FindMarkerIndex(
    const CompiledTileSemanticOverlay& overlay,
    std::string_view semanticId) noexcept;
[[nodiscard]] std::optional<TileMarkerInspection> InspectMarker(
    const CompiledTileSemanticOverlay& overlay,
    const CompiledTileMap& tileMap,
    std::string_view semanticId) noexcept;
} // namespace trace2d::tile
