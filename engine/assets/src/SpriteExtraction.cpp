#include <trace2d/assets/SpriteExtraction.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace trace2d::assets
{
namespace
{
constexpr std::uint64_t kBytesPerPixel = 4U;

struct PlannedFrame final
{
    std::string id{};
    SpritePixelRect rect{};
};

SpriteExtractionResult Fail(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec,
    const SpriteExtractionErrorCode code,
    std::string id,
    std::string message)
{
    SpriteExtractionResult result{};
    result.sheetId = std::string{sheet.id};
    result.mode = spec.mode;
    result.diagnostics.push_back({code, std::move(id), std::move(message)});
    return result;
}

bool CheckedByteCount(
    const std::uint32_t width,
    const std::uint32_t height,
    std::size_t& outBytes) noexcept
{
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixels > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / kBytesPerPixel)
    {
        return false;
    }
    outBytes = static_cast<std::size_t>(pixels * kBytesPerPixel);
    return true;
}

bool InBounds(
    const SpritePixelRect rect,
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    if (rect.width == 0U || rect.height == 0U)
    {
        return false;
    }
    return static_cast<std::uint64_t>(rect.x) + rect.width <= width &&
           static_cast<std::uint64_t>(rect.y) + rect.height <= height;
}

bool GridCoordinate(
    const std::uint32_t origin,
    const std::uint32_t ordinal,
    const std::uint32_t extent,
    const std::uint32_t spacing,
    std::uint32_t& out) noexcept
{
    const std::uint64_t max = std::numeric_limits<std::uint32_t>::max();
    const std::uint64_t step = static_cast<std::uint64_t>(extent) + spacing;
    if (ordinal != 0U && step > max / ordinal)
    {
        return false;
    }
    const std::uint64_t offset = static_cast<std::uint64_t>(ordinal) * step;
    if (static_cast<std::uint64_t>(origin) > max - offset)
    {
        return false;
    }
    out = static_cast<std::uint32_t>(static_cast<std::uint64_t>(origin) + offset);
    return true;
}

void CleanPixel(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const std::size_t pixelIndex,
    std::uint8_t (&rgba)[4]) noexcept
{
    const std::size_t byteIndex = pixelIndex * 4U;
    for (std::size_t channel = 0U; channel < 4U; ++channel)
    {
        rgba[channel] = sheet.rgba8[byteIndex + channel];
    }

    bool clear = false;
    if (cleanup.exactBackgroundRgb.has_value())
    {
        const SpriteExtractionRgbKey key = *cleanup.exactBackgroundRgb;
        clear = rgba[0] == key.red && rgba[1] == key.green && rgba[2] == key.blue;
    }
    if (!clear && cleanup.alphaCutoff.has_value())
    {
        clear = rgba[3] <= *cleanup.alphaCutoff;
    }
    if (clear || (cleanup.zeroTransparentRgb && rgba[3] == 0U))
    {
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0U;
    }
}

bool Visible(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const std::size_t pixelIndex) noexcept
{
    std::uint8_t rgba[4]{};
    CleanPixel(sheet, cleanup, pixelIndex, rgba);
    return rgba[3] > 0U;
}

std::optional<SpritePixelRect> VisibleBounds(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const SpritePixelRect rect) noexcept
{
    std::uint32_t minX = rect.x + rect.width;
    std::uint32_t minY = rect.y + rect.height;
    std::uint32_t maxX = rect.x;
    std::uint32_t maxY = rect.y;
    bool found = false;

    for (std::uint32_t y = rect.y; y < rect.y + rect.height; ++y)
    {
        for (std::uint32_t x = rect.x; x < rect.x + rect.width; ++x)
        {
            const std::size_t index = static_cast<std::size_t>(y) * sheet.width + x;
            if (!Visible(sheet, cleanup, index))
            {
                continue;
            }
            found = true;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (!found)
    {
        return std::nullopt;
    }
    return SpritePixelRect{minX, minY, maxX - minX + 1U, maxY - minY + 1U};
}

bool CopyRect(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const SpritePixelRect rect,
    std::vector<std::uint8_t>& output)
{
    std::size_t bytes = 0U;
    if (!CheckedByteCount(rect.width, rect.height, bytes))
    {
        return false;
    }
    output.resize(bytes);

    std::size_t destination = 0U;
    for (std::uint32_t y = rect.y; y < rect.y + rect.height; ++y)
    {
        for (std::uint32_t x = rect.x; x < rect.x + rect.width; ++x)
        {
            std::uint8_t rgba[4]{};
            CleanPixel(
                sheet,
                cleanup,
                static_cast<std::size_t>(y) * sheet.width + x,
                rgba);
            for (std::size_t channel = 0U; channel < 4U; ++channel)
            {
                output[destination++] = rgba[channel];
            }
        }
    }
    return true;
}

std::string GeneratedId(const std::string_view sheetId, const std::size_t ordinal)
{
    return std::string{sheetId} + "#frame-" + std::to_string(ordinal);
}

void AppendUnsigned(std::string& out, const std::uint64_t value)
{
    char buffer[32]{};
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, converted.ptr);
}

void AppendJsonString(std::string& out, const std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    out.push_back('"');
    for (const unsigned char c : value)
    {
        switch (c)
        {
        case '"': out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\b': out.append("\\b"); break;
        case '\f': out.append("\\f"); break;
        case '\n': out.append("\\n"); break;
        case '\r': out.append("\\r"); break;
        case '\t': out.append("\\t"); break;
        default:
            if (c < 0x20U)
            {
                out.append("\\u00");
                out.push_back(hex[(c >> 4U) & 0x0FU]);
                out.push_back(hex[c & 0x0FU]);
            }
            else
            {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

void AppendRect(std::string& out, const SpritePixelRect rect)
{
    out.append("{\"x\":");
    AppendUnsigned(out, rect.x);
    out.append(",\"y\":");
    AppendUnsigned(out, rect.y);
    out.append(",\"width\":");
    AppendUnsigned(out, rect.width);
    out.append(",\"height\":");
    AppendUnsigned(out, rect.height);
    out.push_back('}');
}
} // namespace

std::string_view ToString(const SpriteExtractionMode value) noexcept
{
    switch (value)
    {
    case SpriteExtractionMode::ExplicitRects: return "explicit_rects";
    case SpriteExtractionMode::UniformGrid: return "uniform_grid";
    case SpriteExtractionMode::AlphaComponents: return "alpha_components";
    }
    return "unknown";
}

std::string_view ToString(const SpriteExtractionOrder value) noexcept
{
    switch (value)
    {
    case SpriteExtractionOrder::RowMajor: return "row_major";
    case SpriteExtractionOrder::ColumnMajor: return "column_major";
    }
    return "unknown";
}

std::string_view ToString(const SpriteExtractionErrorCode value) noexcept
{
    switch (value)
    {
    case SpriteExtractionErrorCode::EmptySheetId: return "empty_sheet_id";
    case SpriteExtractionErrorCode::InvalidDimensions: return "invalid_dimensions";
    case SpriteExtractionErrorCode::InvalidByteCount: return "invalid_byte_count";
    case SpriteExtractionErrorCode::SizeOverflow: return "size_overflow";
    case SpriteExtractionErrorCode::InvalidExpectedFrameCount: return "invalid_expected_frame_count";
    case SpriteExtractionErrorCode::InvalidMode: return "invalid_mode";
    case SpriteExtractionErrorCode::EmptyFrameId: return "empty_frame_id";
    case SpriteExtractionErrorCode::DuplicateFrameId: return "duplicate_frame_id";
    case SpriteExtractionErrorCode::InvalidRectangle: return "invalid_rectangle";
    case SpriteExtractionErrorCode::RectangleOutOfBounds: return "rectangle_out_of_bounds";
    case SpriteExtractionErrorCode::InvalidGrid: return "invalid_grid";
    case SpriteExtractionErrorCode::ExpectedFrameCountMismatch: return "expected_frame_count_mismatch";
    case SpriteExtractionErrorCode::EmptyFrameAfterTrim: return "empty_frame_after_trim";
    case SpriteExtractionErrorCode::ProcessingFailure: return "processing_failure";
    }
    return "unknown";
}

SpriteExtractionResult ExtractSpriteFrames(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec)
{
    if (sheet.id.empty())
    {
        return Fail(sheet, spec, SpriteExtractionErrorCode::EmptySheetId, {}, "Sheet ID must be non-empty.");
    }
    if (sheet.width == 0U || sheet.height == 0U)
    {
        return Fail(sheet, spec, SpriteExtractionErrorCode::InvalidDimensions, std::string{sheet.id}, "Sheet dimensions must be positive.");
    }

    std::size_t expectedBytes = 0U;
    if (!CheckedByteCount(sheet.width, sheet.height, expectedBytes))
    {
        return Fail(sheet, spec, SpriteExtractionErrorCode::SizeOverflow, std::string{sheet.id}, "Sheet byte count overflow.");
    }
    if (sheet.rgba8.size() != expectedBytes)
    {
        return Fail(sheet, spec, SpriteExtractionErrorCode::InvalidByteCount, std::string{sheet.id}, "RGBA8 byte count must equal width * height * 4.");
    }
    if (spec.expectedFrameCount == 0U)
    {
        return Fail(sheet, spec, SpriteExtractionErrorCode::InvalidExpectedFrameCount, std::string{sheet.id}, "expectedFrameCount must be positive.");
    }

    std::vector<PlannedFrame> plans{};
    switch (spec.mode)
    {
    case SpriteExtractionMode::ExplicitRects:
    {
        if (spec.explicitRects.size() != spec.expectedFrameCount)
        {
            return Fail(sheet, spec, SpriteExtractionErrorCode::ExpectedFrameCountMismatch, std::string{sheet.id}, "Explicit rectangle count mismatch.");
        }
        std::unordered_set<std::string> ids{};
        ids.reserve(spec.explicitRects.size());
        plans.reserve(spec.explicitRects.size());
        for (const SpriteExtractionRectView& input : spec.explicitRects)
        {
            if (input.id.empty())
            {
                return Fail(sheet, spec, SpriteExtractionErrorCode::EmptyFrameId, std::string{sheet.id}, "Explicit frame ID must be non-empty.");
            }
            if (!ids.emplace(input.id).second)
            {
                return Fail(sheet, spec, SpriteExtractionErrorCode::DuplicateFrameId, std::string{input.id}, "Explicit frame IDs must be unique.");
            }
            if (input.rect.width == 0U || input.rect.height == 0U)
            {
                return Fail(sheet, spec, SpriteExtractionErrorCode::InvalidRectangle, std::string{input.id}, "Rectangle dimensions must be positive.");
            }
            if (!InBounds(input.rect, sheet.width, sheet.height))
            {
                return Fail(sheet, spec, SpriteExtractionErrorCode::RectangleOutOfBounds, std::string{input.id}, "Rectangle is outside the source sheet.");
            }
            plans.push_back({std::string{input.id}, input.rect});
        }
        break;
    }
    case SpriteExtractionMode::UniformGrid:
    {
        const SpriteExtractionGridSpec& grid = spec.grid;
        if (grid.cellWidth == 0U || grid.cellHeight == 0U || grid.columns == 0U || grid.rows == 0U ||
            (grid.order != SpriteExtractionOrder::RowMajor && grid.order != SpriteExtractionOrder::ColumnMajor))
        {
            return Fail(sheet, spec, SpriteExtractionErrorCode::InvalidGrid, std::string{sheet.id}, "Grid geometry/order is invalid.");
        }
        if (static_cast<std::uint64_t>(grid.columns) * grid.rows != spec.expectedFrameCount)
        {
            return Fail(sheet, spec, SpriteExtractionErrorCode::ExpectedFrameCountMismatch, std::string{sheet.id}, "Grid frame count mismatch.");
        }

        std::uint32_t lastX = 0U;
        std::uint32_t lastY = 0U;
        if (!GridCoordinate(grid.originX, grid.columns - 1U, grid.cellWidth, grid.spacingX, lastX) ||
            !GridCoordinate(grid.originY, grid.rows - 1U, grid.cellHeight, grid.spacingY, lastY))
        {
            return Fail(sheet, spec, SpriteExtractionErrorCode::SizeOverflow, std::string{sheet.id}, "Grid coordinate arithmetic overflow.");
        }
        if (!InBounds({lastX, lastY, grid.cellWidth, grid.cellHeight}, sheet.width, sheet.height))
        {
            return Fail(sheet, spec, SpriteExtractionErrorCode::RectangleOutOfBounds, std::string{sheet.id}, "Grid extent is outside the source sheet.");
        }

        plans.reserve(spec.expectedFrameCount);
        for (std::uint32_t ordinal = 0U; ordinal < spec.expectedFrameCount; ++ordinal)
        {
            const std::uint32_t row =
                grid.order == SpriteExtractionOrder::RowMajor ? ordinal / grid.columns : ordinal % grid.rows;
            const std::uint32_t column =
                grid.order == SpriteExtractionOrder::RowMajor ? ordinal % grid.columns : ordinal / grid.rows;
            std::uint32_t x = 0U;
            std::uint32_t y = 0U;
            if (!GridCoordinate(grid.originX, column, grid.cellWidth, grid.spacingX, x) ||
                !GridCoordinate(grid.originY, row, grid.cellHeight, grid.spacingY, y))
            {
                return Fail(sheet, spec, SpriteExtractionErrorCode::SizeOverflow, std::string{sheet.id}, "Grid coordinate arithmetic overflow.");
            }
            const SpritePixelRect rect{x, y, grid.cellWidth, grid.cellHeight};
            if (!InBounds(rect, sheet.width, sheet.height))
            {
                return Fail(sheet, spec, SpriteExtractionErrorCode::RectangleOutOfBounds, GeneratedId(sheet.id, ordinal), "Grid rectangle is outside the source sheet.");
            }
            plans.push_back({GeneratedId(sheet.id, ordinal), rect});
        }
        break;
    }
    case SpriteExtractionMode::AlphaComponents:
    {
        const std::size_t pixelCount =
            static_cast<std::size_t>(sheet.width) * static_cast<std::size_t>(sheet.height);
        std::vector<std::uint8_t> visited(pixelCount, 0U);
        std::vector<std::size_t> stack{};
        stack.reserve(std::min<std::size_t>(pixelCount, 4096U));
        plans.reserve(std::min<std::size_t>(spec.expectedFrameCount, 1024U));

        for (std::size_t seed = 0U; seed < pixelCount; ++seed)
        {
            if (visited[seed] != 0U)
            {
                continue;
            }
            visited[seed] = 1U;
            if (!Visible(sheet, spec.cleanup, seed))
            {
                continue;
            }

            stack.clear();
            stack.push_back(seed);
            std::uint32_t minX = sheet.width;
            std::uint32_t minY = sheet.height;
            std::uint32_t maxX = 0U;
            std::uint32_t maxY = 0U;

            while (!stack.empty())
            {
                const std::size_t current = stack.back();
                stack.pop_back();
                const std::uint32_t x = static_cast<std::uint32_t>(current % sheet.width);
                const std::uint32_t y = static_cast<std::uint32_t>(current / sheet.width);
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);

                const auto TryVisit = [&](const std::uint32_t nx, const std::uint32_t ny)
                {
                    const std::size_t neighbor = static_cast<std::size_t>(ny) * sheet.width + nx;
                    if (visited[neighbor] != 0U)
                    {
                        return;
                    }
                    visited[neighbor] = 1U;
                    if (Visible(sheet, spec.cleanup, neighbor))
                    {
                        stack.push_back(neighbor);
                    }
                };

                if (x > 0U) TryVisit(x - 1U, y);
                if (x + 1U < sheet.width) TryVisit(x + 1U, y);
                if (y > 0U) TryVisit(x, y - 1U);
                if (y + 1U < sheet.height) TryVisit(x, y + 1U);
            }

            plans.push_back({
                GeneratedId(sheet.id, plans.size()),
                SpritePixelRect{minX, minY, maxX - minX + 1U, maxY - minY + 1U},
            });
        }

        if (plans.size() != spec.expectedFrameCount)
        {
            return Fail(sheet, spec, SpriteExtractionErrorCode::ExpectedFrameCountMismatch, std::string{sheet.id}, "Alpha-component count mismatch.");
        }
        break;
    }
    default:
        return Fail(sheet, spec, SpriteExtractionErrorCode::InvalidMode, std::string{sheet.id}, "Extraction mode is invalid.");
    }

    SpriteExtractionResult result{};
    result.sheetId = std::string{sheet.id};
    result.mode = spec.mode;
    result.frames.reserve(plans.size());

    for (const PlannedFrame& plan : plans)
    {
        SpritePixelRect sourceRect = plan.rect;
        if (spec.trimToVisibleAlphaBounds)
        {
            const std::optional<SpritePixelRect> bounds =
                VisibleBounds(sheet, spec.cleanup, plan.rect);
            if (!bounds.has_value())
            {
                return Fail(sheet, spec, SpriteExtractionErrorCode::EmptyFrameAfterTrim, plan.id, "Trim requested for an empty frame.");
            }
            sourceRect = *bounds;
        }

        SpriteExtractedFrame frame{};
        frame.id = plan.id;
        frame.sourceRect = sourceRect;
        frame.width = sourceRect.width;
        frame.height = sourceRect.height;
        if (!CopyRect(sheet, spec.cleanup, sourceRect, frame.rgba8))
        {
            return Fail(sheet, spec, SpriteExtractionErrorCode::SizeOverflow, plan.id, "Extracted byte count overflow.");
        }
        result.frames.push_back(std::move(frame));
    }

    std::vector<SpriteProcessingFrameView> views{};
    views.reserve(result.frames.size());
    for (const SpriteExtractedFrame& frame : result.frames)
    {
        views.push_back({
            .id = frame.id,
            .width = frame.width,
            .height = frame.height,
            .rgba8 = frame.rgba8,
        });
    }

    const SpriteProcessingResult processing = AnalyzeSpriteProcessing(views, {});
    if (!processing.Succeeded())
    {
        std::string message{"SPP0 rejected extracted output."};
        if (!processing.diagnostics.empty())
        {
            message.append(" ");
            message.append(processing.diagnostics.front().message);
        }
        return Fail(sheet, spec, SpriteExtractionErrorCode::ProcessingFailure, std::string{sheet.id}, std::move(message));
    }
    result.processingReport = *processing.report;
    return result;
}

std::string SerializeSpriteExtractionResultJson(const SpriteExtractionResult& result)
{
    std::string out{};
    out.reserve(512U + result.frames.size() * 128U);
    out.append("{\"schema_version\":");
    AppendUnsigned(out, result.schemaVersion);
    out.append(",\"sheet_id\":");
    AppendJsonString(out, result.sheetId);
    out.append(",\"mode\":");
    AppendJsonString(out, ToString(result.mode));
    out.append(",\"succeeded\":");
    out.append(result.Succeeded() ? "true" : "false");
    out.append(",\"frames\":[");

    for (std::size_t index = 0U; index < result.frames.size(); ++index)
    {
        if (index > 0U) out.push_back(',');
        const SpriteExtractedFrame& frame = result.frames[index];
        out.append("{\"id\":");
        AppendJsonString(out, frame.id);
        out.append(",\"source_rect\":");
        AppendRect(out, frame.sourceRect);
        out.append(",\"width\":");
        AppendUnsigned(out, frame.width);
        out.append(",\"height\":");
        AppendUnsigned(out, frame.height);
        out.append(",\"rgba8_byte_count\":");
        AppendUnsigned(out, frame.rgba8.size());
        out.push_back('}');
    }

    out.append("],\"processing\":");
    if (result.processingReport.has_value())
    {
        out.append(SerializeSpriteProcessingReportJson(*result.processingReport));
    }
    else
    {
        out.append("null");
    }
    out.append(",\"diagnostics\":[");

    for (std::size_t index = 0U; index < result.diagnostics.size(); ++index)
    {
        if (index > 0U) out.push_back(',');
        const SpriteExtractionDiagnostic& diagnostic = result.diagnostics[index];
        out.append("{\"code\":");
        AppendJsonString(out, ToString(diagnostic.code));
        out.append(",\"id\":");
        AppendJsonString(out, diagnostic.id);
        out.append(",\"message\":");
        AppendJsonString(out, diagnostic.message);
        out.push_back('}');
    }
    out.append("]}");
    return out;
}
} // namespace trace2d::assets
