#pragma once

#include <trace2d/tile/TileMap.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::tile
{
inline constexpr std::int32_t GeneratedEmptyTileTableIndex = -1;
inline constexpr std::uint8_t GeneratedTileTransformMask = 0x0FU;

struct GeneratedTileLayerDocument final
{
    std::string semanticId{};
    std::int32_t order{0};
    std::int32_t originX{0};
    std::int32_t originY{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    bool visible{true};
    std::vector<std::int32_t> tileTableIndices{};
    // Empty means identity transforms for every dense cell. Otherwise this has
    // exactly width * height entries using the CompiledTileCell transform bits.
    std::vector<std::uint8_t> transformBits{};

    [[nodiscard]] bool operator==(const GeneratedTileLayerDocument&) const noexcept = default;
};

struct GeneratedTileMapDocument final
{
    static constexpr std::int64_t FormatVersion = 1;

    std::string semanticId{};
    std::string tileSetSemanticId{};
    std::uint32_t cellWidth{0};
    std::uint32_t cellHeight{0};
    // Dense payloads reference this setup-time semantic table by int32 index.
    // -1 is the only empty-cell sentinel.
    std::vector<std::string> tileTable{};
    std::vector<GeneratedTileLayerDocument> layers{};

    [[nodiscard]] bool operator==(const GeneratedTileMapDocument&) const noexcept = default;
};

struct GeneratedTileMapLoadResult final
{
    std::optional<GeneratedTileMapDocument> document{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

struct GeneratedTileMapConversionMetrics final
{
    std::uint64_t denseCellCount{0};
    std::uint64_t occupiedCellCount{0};
    std::uint64_t knownDensePayloadBytes{0};
    std::size_t tileTableEntries{0};
    std::size_t outputCellCapacity{0};
};

struct GeneratedTileMapConversionResult final
{
    std::optional<TileMapDocument> tileMap{};
    GeneratedTileMapConversionMetrics metrics{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return tileMap.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] GeneratedTileMapLoadResult ParseGeneratedTileMapToml(
    std::string_view text,
    std::string_view sourceName = {});
[[nodiscard]] std::string SaveGeneratedTileMapToml(const GeneratedTileMapDocument& document);

[[nodiscard]] std::vector<TileDiagnostic> ValidateGeneratedTileMap(
    const CompiledTileSet& tileSet,
    const GeneratedTileMapDocument& document);

// Generated/dense companion data is converted only during explicit setup/import.
// The result is ordinary canonical TileMap authoring data consumed by the existing
// CompileTileMap/runtime/presentation/semantic paths.
[[nodiscard]] GeneratedTileMapConversionResult ConvertGeneratedTileMap(
    const CompiledTileSet& tileSet,
    const GeneratedTileMapDocument& document);
} // namespace trace2d::tile
