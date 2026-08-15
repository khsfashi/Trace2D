#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace trace2d::tile
{
enum class TileErrorCode : std::uint8_t
{
    InvalidReference = 0,
    UnsupportedFormat,
    MissingFile,
    ReadFailure,
    ParseError,
    SchemaError,
    WriteFailure,
};

[[nodiscard]] std::string_view ToString(TileErrorCode code) noexcept;

struct TileDiagnostic final
{
    TileErrorCode code{TileErrorCode::SchemaError};
    std::string reference{};
    std::string resolvedPath{};
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct TileRegion final
{
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t width{0};
    std::int32_t height{0};

    [[nodiscard]] bool operator==(const TileRegion&) const noexcept = default;
};

struct TileTransform final
{
    bool flipX{false};
    bool flipY{false};
    std::uint8_t quarterTurns{0};

    [[nodiscard]] bool operator==(const TileTransform&) const noexcept = default;
};

struct TileDefinition final
{
    std::string semanticId{};
    TileRegion sourceRegion{};
    std::vector<std::string> tags{};

    [[nodiscard]] bool operator==(const TileDefinition&) const noexcept = default;
};

struct TileSetDocument final
{
    static constexpr std::int64_t FormatVersion = 1;

    std::string semanticId{};
    std::string textureReference{};
    std::int32_t sourceWidth{0};
    std::int32_t sourceHeight{0};
    std::vector<TileDefinition> tiles{};

    [[nodiscard]] bool operator==(const TileSetDocument&) const noexcept = default;
};

struct TileCellDocument final
{
    std::int32_t x{0};
    std::int32_t y{0};
    std::string tileSemanticId{};
    TileTransform transform{};

    [[nodiscard]] bool operator==(const TileCellDocument&) const noexcept = default;
};

struct TileLayerDocument final
{
    std::string semanticId{};
    std::int32_t order{0};
    std::int32_t originX{0};
    std::int32_t originY{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    bool visible{true};
    std::vector<TileCellDocument> cells{};

    [[nodiscard]] bool operator==(const TileLayerDocument&) const noexcept = default;
};

struct TileMapDocument final
{
    static constexpr std::int64_t FormatVersion = 1;

    std::string semanticId{};
    std::string tileSetSemanticId{};
    std::uint32_t cellWidth{0};
    std::uint32_t cellHeight{0};
    std::vector<TileLayerDocument> layers{};

    [[nodiscard]] bool operator==(const TileMapDocument&) const noexcept = default;
};

struct TileSetLoadResult final
{
    std::optional<TileSetDocument> document{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

struct TileMapLoadResult final
{
    std::optional<TileMapDocument> document{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

inline constexpr std::uint32_t EmptyTileIndex = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint64_t MaximumCompiledCellsPerLayer = 4ULL * 1024ULL * 1024ULL;

// Fixed-size compiled cell state. Strings/tags remain once per tile/layer definition,
// never once per runtime cell.
struct CompiledTileCell final
{
    std::uint32_t tileIndex{EmptyTileIndex};
    std::uint8_t transformBits{0};
    std::uint8_t reserved0{0};
    std::uint8_t reserved1{0};
    std::uint8_t reserved2{0};

    [[nodiscard]] bool Empty() const noexcept
    {
        return tileIndex == EmptyTileIndex;
    }

    [[nodiscard]] TileTransform Transform() const noexcept
    {
        return TileTransform{
            .flipX = (transformBits & 0x04U) != 0U,
            .flipY = (transformBits & 0x08U) != 0U,
            .quarterTurns = static_cast<std::uint8_t>(transformBits & 0x03U),
        };
    }
};

static_assert(std::is_trivially_copyable_v<CompiledTileCell>);
static_assert(sizeof(CompiledTileCell) == 8U);

struct CompiledTileDefinition final
{
    std::string semanticId{};
    TileRegion sourceRegion{};
    std::vector<std::string> tags{};
};

struct CompiledTileSet final
{
    std::string semanticId{};
    std::string textureReference{};
    std::int32_t sourceWidth{0};
    std::int32_t sourceHeight{0};
    std::vector<CompiledTileDefinition> tiles{};
};

struct CompiledTileLayer final
{
    std::string semanticId{};
    std::int32_t order{0};
    std::int32_t originX{0};
    std::int32_t originY{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    bool visible{true};
    std::vector<CompiledTileCell> cells{};
};

struct CompiledTileMap final
{
    std::string semanticId{};
    std::string tileSetSemanticId{};
    std::uint32_t cellWidth{0};
    std::uint32_t cellHeight{0};
    std::vector<CompiledTileLayer> layers{};
};

struct TileSetCompileResult final
{
    std::optional<CompiledTileSet> tileSet{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return tileSet.has_value() && diagnostics.empty();
    }
};

struct TileMapCompileResult final
{
    std::optional<CompiledTileMap> tileMap{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return tileMap.has_value() && diagnostics.empty();
    }
};

struct TileCellInspection final
{
    bool occupied{false};
    std::size_t layerIndex{0};
    std::int32_t worldX{0};
    std::int32_t worldY{0};
    std::string_view layerSemanticId{};
    std::string_view tileSemanticId{};
    TileRegion sourceRegion{};
    TileTransform transform{};
    std::span<const std::string> tags{};
};

[[nodiscard]] std::vector<TileDiagnostic> ValidateTileSet(const TileSetDocument& document);
[[nodiscard]] std::vector<TileDiagnostic> ValidateTileMap(const TileMapDocument& document);

[[nodiscard]] TileSetLoadResult ParseTileSetToml(std::string_view text, std::string_view sourceName = {});
[[nodiscard]] TileMapLoadResult ParseTileMapToml(std::string_view text, std::string_view sourceName = {});
[[nodiscard]] std::string SaveTileSetToml(const TileSetDocument& document);
[[nodiscard]] std::string SaveTileMapToml(const TileMapDocument& document);

[[nodiscard]] TileSetCompileResult CompileTileSet(const TileSetDocument& document);
[[nodiscard]] TileMapCompileResult CompileTileMap(const CompiledTileSet& tileSet, const TileMapDocument& document);

// Semantic-id lookups are setup/inspection operations. Hot cell reads use resolved indices.
[[nodiscard]] std::optional<std::size_t> FindTileIndex(const CompiledTileSet& tileSet, std::string_view semanticId) noexcept;
[[nodiscard]] std::optional<std::size_t> FindLayerIndex(const CompiledTileMap& tileMap, std::string_view semanticId) noexcept;
[[nodiscard]] const CompiledTileCell* CellAtWorld(
    const CompiledTileMap& tileMap,
    std::size_t layerIndex,
    std::int32_t worldX,
    std::int32_t worldY) noexcept;
[[nodiscard]] std::optional<TileCellInspection> InspectCell(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    std::string_view layerSemanticId,
    std::int32_t worldX,
    std::int32_t worldY) noexcept;

class TileDocumentStore final
{
public:
    explicit TileDocumentStore(std::filesystem::path projectRoot);

    [[nodiscard]] const std::filesystem::path& ProjectRoot() const noexcept;
    [[nodiscard]] TileSetLoadResult LoadTileSet(std::string_view projectRelativeReference) const;
    [[nodiscard]] TileMapLoadResult LoadTileMap(std::string_view projectRelativeReference) const;
    [[nodiscard]] std::vector<TileDiagnostic> SaveTileSet(
        std::string_view projectRelativeReference,
        const TileSetDocument& document) const;
    [[nodiscard]] std::vector<TileDiagnostic> SaveTileMap(
        std::string_view projectRelativeReference,
        const TileMapDocument& document) const;

private:
    std::filesystem::path projectRoot_{};
};
} // namespace trace2d::tile
