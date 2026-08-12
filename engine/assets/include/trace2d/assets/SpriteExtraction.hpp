#pragma once

#include <trace2d/assets/SpriteProcessing.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::assets
{
enum class SpriteExtractionMode : std::uint8_t
{
    ExplicitRects = 0,
    UniformGrid,
    AlphaComponents,
};

enum class SpriteExtractionOrder : std::uint8_t
{
    RowMajor = 0,
    ColumnMajor,
};

enum class SpriteExtractionErrorCode : std::uint8_t
{
    EmptySheetId = 0,
    InvalidDimensions,
    InvalidByteCount,
    SizeOverflow,
    InvalidExpectedFrameCount,
    InvalidMode,
    EmptyFrameId,
    DuplicateFrameId,
    InvalidRectangle,
    RectangleOutOfBounds,
    InvalidGrid,
    ExpectedFrameCountMismatch,
    EmptyFrameAfterTrim,
    ProcessingFailure,
};

[[nodiscard]] std::string_view ToString(SpriteExtractionMode value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteExtractionOrder value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteExtractionErrorCode value) noexcept;

struct SpriteExtractionRgbKey final
{
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};

    [[nodiscard]] bool operator==(const SpriteExtractionRgbKey&) const noexcept = default;
};

struct SpriteExtractionCleanup final
{
    std::optional<SpriteExtractionRgbKey> exactBackgroundRgb{};
    std::optional<std::uint8_t> alphaCutoff{};
    bool zeroTransparentRgb{false};
};

struct SpriteExtractionSheetView final
{
    std::string_view id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::span<const std::uint8_t> rgba8{};
};

struct SpriteExtractionRectView final
{
    std::string_view id{};
    SpritePixelRect rect{};
};

struct SpriteExtractionGridSpec final
{
    std::uint32_t originX{0};
    std::uint32_t originY{0};
    std::uint32_t cellWidth{0};
    std::uint32_t cellHeight{0};
    std::uint32_t columns{0};
    std::uint32_t rows{0};
    std::uint32_t spacingX{0};
    std::uint32_t spacingY{0};
    SpriteExtractionOrder order{SpriteExtractionOrder::RowMajor};
};

struct SpriteExtractionSpec final
{
    SpriteExtractionMode mode{SpriteExtractionMode::ExplicitRects};
    std::span<const SpriteExtractionRectView> explicitRects{};
    SpriteExtractionGridSpec grid{};
    std::uint32_t expectedFrameCount{0};
    bool trimToVisibleAlphaBounds{false};
    SpriteExtractionCleanup cleanup{};
};

struct SpriteExtractedFrame final
{
    std::string id{};
    SpritePixelRect sourceRect{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8{};
};

struct SpriteExtractionDiagnostic final
{
    SpriteExtractionErrorCode code{SpriteExtractionErrorCode::InvalidDimensions};
    std::string id{};
    std::string message{};
};

struct SpriteExtractionResult final
{
    std::uint32_t schemaVersion{1};
    std::string sheetId{};
    SpriteExtractionMode mode{SpriteExtractionMode::ExplicitRects};
    std::vector<SpriteExtractedFrame> frames{};
    std::optional<SpriteProcessingReport> processingReport{};
    std::vector<SpriteExtractionDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return !frames.empty() && processingReport.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] SpriteExtractionResult ExtractSpriteFrames(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec);

[[nodiscard]] std::string SerializeSpriteExtractionResultJson(const SpriteExtractionResult& result);
} // namespace trace2d::assets
