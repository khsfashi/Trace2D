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

SpriteExtractionResult Failure(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec,
    const SpriteExtractionErrorCode code,
    std::string id,
    std::string message)
{
    SpriteExtractionResult result{};
    result.sheetId = std::string{sheet.id};
    result.mode = spec.mode;
    result.diagnostics.push_back(SpriteExtractionDiagnostic{
        .code = code,
        .id = std::move(id),
        .message = std::move(message),
    });
    return result;
}

bool CheckedByteCount(
    const std::uint32_t width,
    const std::uint32_t height,
    std::size_t& outByteCount) noexcept
{
    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixelCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / kBytesPerPixel)
    {
        return false;
    }

    outByteCount = static_cast<std::size_t>(pixelCount * kBytesPerPixel);
    return true;
}

bool RectInBounds(
    const SpritePixelRect rect,
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    if (rect.width == 0U || rect.height == 0U)
    {
        return false;
    }

    const std::uint64_t right = static_cast<std::uint64_t>(rect.x) + rect.width;
    const std::uint64_t bottom = static_cast<std::uint64_t>(rect.y) + rect.height;
    return right <= width && bottom <= height;
}

bool CheckedGridCoordinate(
    const std::uint32_t origin,
    const std::uint32_t ordinal,
    const std::uint32_t extent,
    const std::uint32_t spacing,
    std::uint32_t& outCoordinate) noexcept
{
    const std::uint64_t step = static_cast<std::uint64_t>(extent) + spacing;
    const std::uint64_t offset = static_cast<std::uint64_t>(ordinal) * step;
    const std::uint64_t coordinate = static_cast<std::uint64_t>(origin) + offset;
    if (coordinate > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    outCoordinate = static_cast<std::uint32_t>(coordinate);
    return true;
}

void ReadCleanedPixel(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const std::size_t pixelIndex,
    std::uint8_t (&rgba)[4]) noexcept
{
    const std::size_t byteIndex = pixelIndex * 4U;
    rgba[0] = sheet.rgba8[byteIndex + 0U];
    rgba[1] = sheet.rgba8[byteIndex + 1U];
    rgba[2] = sheet.rgba8[byteIndex + 2U];
    rgba[3] = sheet.rgba8[byteIndex + 3U];

    bool makeTransparent = false;
    if (cleanup.exactBackgroundRgb.has_value())
    {
        const SpriteExtractionRgbKey key = *cleanup.exactBackgroundRgb;
        makeTransparent =
            rgba[0] == key.red &&
            rgba[1] == key.green &&
            rgba[2] == key.blue;
    }

    if (!makeTransparent && cleanup.alphaCutoff.has_value())
    {
        makeTransparent = rgba[3] <= *cleanup.alphaCutoff;
    }

    if (makeTransparent || (cleanup.zeroTransparentRgb && rgba[3] == 0U))
    {
        rgba[0] = 0U;
        rgba[1] = 0U;
        rgba[2] = 0U;
        rgba[3] = 0U;
    }
}

bool IsVisible(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const std::size_t pixelIndex) noexcept
{
    std::uint8_t rgba[4]{};
    ReadCleanedPixel(sheet, cleanup, pixelIndex, rgba);
    return rgba[3] > 0U;
}

std::optional<SpritePixelRect> FindVisibleBounds(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const SpritePixelRect rect) noexcept
{
    std::uint32_t minX = rect.x + rect.width;
    std::uint32_t minY = rect.y + rect.height;
    std::uint32_t maxX = rect.x;
    std::uint32_t maxY = rect.y;
    bool visible = false;

    const std::uint32_t endY = rect.y + rect.height;
    const std::uint32_t endX = rect.x + rect.width;
    for (std::uint32_t y = rect.y; y < endY; ++y)
    {
        for (std::uint32_t x = rect.x; x < endX; ++x)
        {
            const std::size_t pixelIndex =
                static_cast<std::size_t>(y) * sheet.width + x;
            if (!IsVisible(sheet, cleanup, pixelIndex))
            {
                continue;
            }

            visible = true;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (!visible)
    {
        return std::nullopt;
    }

    return SpritePixelRect{
        .x = minX,
        .y = minY,
        .width = maxX - minX + 1U,
        .height = maxY - minY + 1U,
    };
}

bool CopyCleanedRect(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionCleanup& cleanup,
    const SpritePixelRect rect,
    std::vector<std::uint8_t>& outPixels)
{
    std::size_t byteCount = 0U;
    if (!CheckedByteCount(rect.width, rect.height, byteCount))
    {
        return false;
    }

    outPixels.resize(byteCount);
    std::size_t outputByteIndex = 0U;
    const std::uint32_t endY = rect.y + rect.height;
    const std::uint32_t endX = rect.x + rect.width;
    for (std::uint32_t y = rect.y; y < endY; ++y)
    {
        for (std::uint32_t x = rect.x; x < endX; ++x)
        {
            const std::size_t sourcePixelIndex =
                static_cast<std::size_t>(y) * sheet.width + x;
            std::uint8_t rgba[4]{};
            ReadCleanedPixel(sheet, cleanup, sourcePixelIndex, rgba);
            outPixels[outputByteIndex + 0U] = rgba[0];
            outPixels[outputByteIndex + 1U] = rgba[1];
            outPixels[outputByteIndex + 2U] = rgba[2];
            outPixels[outputByteIndex + 3U] = rgba[3];
            outputByteIndex += 4U;
        }
    }
    return true;
}

std::string GeneratedFrameId(const std::string_view sheetId, const std::size_t ordinal)
{
    std::string id{sheetId};
    id.append("#frame-");
    id.append(std::to_string(ordinal));
    return id;
}

SpriteExtractionResult ValidateSheetAndSpec(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec)
{
    if (sheet.id.empty())
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::EmptySheetId,
            {},
            "Sprite extraction requires a stable non-empty sheet ID.");
    }
    if (sheet.width == 0U || sheet.height == 0U)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::InvalidDimensions,
            std::string{sheet.id},
            "Sheet width and height must both be greater than zero.");
    }

    std::size_t expectedBytes = 0U;
    if (!CheckedByteCount(sheet.width, sheet.height, expectedBytes))
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::SizeOverflow,
            std::string{sheet.id},
            "Sheet RGBA8 byte count cannot be represented by size_t.");
    }
    if (sheet.rgba8.size() != expectedBytes)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::InvalidByteCount,
            std::string{sheet.id},
            "Sheet RGBA8 byte count must equal width * height * 4 exactly.");
    }
    if (spec.expectedFrameCount == 0U)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::InvalidExpectedFrameCount,
            std::string{sheet.id},
            "expectedFrameCount must be greater than zero.");
    }

    return {};
}

SpriteExtractionResult PlanExplicitRects(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec,
    std::vector<PlannedFrame>& plans)
{
    if (spec.explicitRects.size() != spec.expectedFrameCount)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::ExpectedFrameCountMismatch,
            std::string{sheet.id},
            "Explicit rectangle count must equal expectedFrameCount exactly.");
    }

    std::unordered_set<std::string> frameIds{};
    frameIds.reserve(spec.explicitRects.size());
    plans.reserve(spec.explicitRects.size());

    for (const SpriteExtractionRectView& input : spec.explicitRects)
    {
        if (input.id.empty())
        {
            return Failure(
                sheet,
                spec,
                SpriteExtractionErrorCode::EmptyFrameId,
                std::string{sheet.id},
                "Every explicit extraction rectangle requires a stable non-empty frame ID.");
        }
        if (!frameIds.emplace(input.id).second)
        {
            return Failure(
                sheet,
                spec,
                SpriteExtractionErrorCode::DuplicateFrameId,
                std::string{input.id},
                "Explicit extraction frame IDs must be unique.");
        }
        if (input.rect.width == 0U || input.rect.height == 0U)
        {
            return Failure(
                sheet,
                spec,
                SpriteExtractionErrorCode::InvalidRectangle,
                std::string{input.id},
                "Extraction rectangles must have positive width and height.");
        }
        if (!RectInBounds(input.rect, sheet.width, sheet.height))
        {
            return Failure(
                sheet,
                spec,
                SpriteExtractionErrorCode::RectangleOutOfBounds,
                std::string{input.id},
                "Extraction rectangle must fit entirely within the source sheet.");
        }

        plans.push_back(PlannedFrame{
            .id = std::string{input.id},
            .rect = input.rect,
        });
    }

    return {};
}

SpriteExtractionResult PlanGrid(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec,
    std::vector<PlannedFrame>& plans)
{
    const SpriteExtractionGridSpec& grid = spec.grid;
    if (grid.cellWidth == 0U || grid.cellHeight == 0U ||
        grid.columns == 0U || grid.rows == 0U)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::InvalidGrid,
            std::string{sheet.id},
            "Grid cell size, row count and column count must all be greater than zero.");
    }

    if (grid.order != SpriteExtractionOrder::RowMajor &&
        grid.order != SpriteExtractionOrder::ColumnMajor)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::InvalidGrid,
            std::string{sheet.id},
            "Grid order is invalid.");
    }

    const std::uint64_t plannedCount =
        static_cast<std::uint64_t>(grid.columns) * grid.rows;
    if (plannedCount != spec.expectedFrameCount)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::ExpectedFrameCountMismatch,
            std::string{sheet.id},
            "Grid rows * columns must equal expectedFrameCount exactly.");
    }

    plans.reserve(spec.expectedFrameCount);
    for (std::uint32_t ordinal = 0U; ordinal < spec.expectedFrameCount; ++ordinal)
    {
        std::uint32_t row = 0U;
        std::uint32_t column = 0U;
        if (grid.order == SpriteExtractionOrder::RowMajor)
        {
            row = ordinal / grid.columns;
            column = ordinal % grid.columns;
        }
        else
        {
            column = ordinal / grid.rows;
            row = ordinal % grid.rows;
        }

        std::uint32_t x = 0U;
        std::uint32_t y = 0U;
        if (!CheckedGridCoordinate(grid.originX, column, grid.cellWidth, grid.spacingX, x) ||
            !CheckedGridCoordinate(grid.originY, row, grid.cellHeight, grid.spacingY, y))
        {
            return Failure(
                sheet,
                spec,
                SpriteExtractionErrorCode::SizeOverflow,
                std::string{sheet.id},
                "Grid coordinate arithmetic overflowed the supported uint32 range.");
        }

        const SpritePixelRect rect{x, y, grid.cellWidth, grid.cellHeight};
        if (!RectInBounds(rect, sheet.width, sheet.height))
        {
            return Failure(
                sheet,
                spec,
                SpriteExtractionErrorCode::RectangleOutOfBounds,
                GeneratedFrameId(sheet.id, ordinal),
                "Generated grid rectangle must fit entirely within the source sheet.");
        }

        plans.push_back(PlannedFrame{
            .id = GeneratedFrameId(sheet.id, ordinal),
            .rect = rect,
        });
    }

    return {};
}

SpriteExtractionResult PlanAlphaComponents(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec,
    std::vector<PlannedFrame>& plans)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(sheet.width) * static_cast<std::size_t>(sheet.height);
    std::vector<std::uint8_t> visited(pixelCount, 0U);
    std::vector<std::size_t> stack{};
    stack.reserve(std::min<std::size_t>(pixelCount, 4096U));
    plans.reserve(spec.expectedFrameCount);

    for (std::size_t seed = 0U; seed < pixelCount; ++seed)
    {
        if (visited[seed] != 0U)
        {
            continue;
        }
        visited[seed] = 1U;
        if (!IsVisible(sheet, spec.cleanup, seed))
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

            const std::uint32_t x =
                static_cast<std::uint32_t>(current % sheet.width);
            const std::uint32_t y =
                static_cast<std::uint32_t>(current / sheet.width);
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);

            const auto TryPush = [&](const std::uint32_t nx, const std::uint32_t ny)
            {
                const std::size_t neighbor =
                    static_cast<std::size_t>(ny) * sheet.width + nx;
                if (visited[neighbor] != 0U)
                {
                    return;
                }
                visited[neighbor] = 1U;
                if (IsVisible(sheet, spec.cleanup, neighbor))
                {
                    stack.push_back(neighbor);
                }
            };

            // Fixed four-neighbor discovery order. Component output order is determined
            // by the row-major seed scan above.
            if (x > 0U)
            {
                TryPush(x - 1U, y);
            }
            if (x + 1U < sheet.width)
            {
                TryPush(x + 1U, y);
            }
            if (y > 0U)
            {
                TryPush(x, y - 1U);
            }
            if (y + 1U < sheet.height)
            {
                TryPush(x, y + 1U);
            }
        }

        const std::size_t ordinal = plans.size();
        plans.push_back(PlannedFrame{
            .id = GeneratedFrameId(sheet.id, ordinal),
            .rect = SpritePixelRect{
                .x = minX,
                .y = minY,
                .width = maxX - minX + 1U,
                .height = maxY - minY + 1U,
            },
        });
    }

    if (plans.size() != spec.expectedFrameCount)
    {
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::ExpectedFrameCountMismatch,
            std::string{sheet.id},
            "Alpha-component count must equal expectedFrameCount exactly.");
    }

    return {};
}

void AppendUnsigned(std::string& output, const std::uint64_t value)
{
    char buffer[32]{};
    const auto conversion = std::to_chars(buffer, buffer + sizeof(buffer), value);
    output.append(buffer, conversion.ptr);
}

void AppendBool(std::string& output, const bool value)
{
    output.append(value ? "true" : "false");
}

void AppendJsonString(std::string& output, const std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";

    output.push_back('"');
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"':
            output.append("\\\"");
            break;
        case '\\':
            output.append("\\\\");
            break;
        case '\b':
            output.append("\\b");
            break;
        case '\f':
            output.append("\\f");
            break;
        case '\n':
            output.append("\\n");
            break;
        case '\r':
            output.append("\\r");
            break;
        case '\t':
            output.append("\\t");
            break;
        default:
            if (character < 0x20U)
            {
                output.append("\\u00");
                output.push_back(hex[(character >> 4U) & 0x0FU]);
                output.push_back(hex[character & 0x0FU]);
            }
            else
            {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

void AppendRect(std::string& output, const SpritePixelRect rect)
{
    output.append("{\"x\":");
    AppendUnsigned(output, rect.x);
    output.append(",\"y\":");
    AppendUnsigned(output, rect.y);
    output.append(",\"width\":");
    AppendUnsigned(output, rect.width);
    output.append(",\"height\":");
    AppendUnsigned(output, rect.height);
    output.push_back('}');
}
} // namespace

std::string_view ToString(const SpriteExtractionMode value) noexcept
{
    switch (value)
    {
    case SpriteExtractionMode::ExplicitRects:
        return "explicit_rects";
    case SpriteExtractionMode::UniformGrid:
        return "uniform_grid";
    case SpriteExtractionMode::AlphaComponents:
        return "alpha_components";
    }
    return "unknown";
}

std::string_view ToString(const SpriteExtractionOrder value) noexcept
{
    switch (value)
    {
    case SpriteExtractionOrder::RowMajor:
        return "row_major";
    case SpriteExtractionOrder::ColumnMajor:
        return "column_major";
    }
    return "unknown";
}

std::string_view ToString(const SpriteExtractionErrorCode value) noexcept
{
    switch (value)
    {
    case SpriteExtractionErrorCode::EmptySheetId:
        return "empty_sheet_id";
    case SpriteExtractionErrorCode::InvalidDimensions:
        return "invalid_dimensions";
    case SpriteExtractionErrorCode::InvalidByteCount:
        return "invalid_byte_count";
    case SpriteExtractionErrorCode::SizeOverflow:
        return "size_overflow";
    case SpriteExtractionErrorCode::InvalidExpectedFrameCount:
        return "invalid_expected_frame_count";
    case SpriteExtractionErrorCode::InvalidMode:
        return "invalid_mode";
    case SpriteExtractionErrorCode::EmptyFrameId:
        return "empty_frame_id";
    case SpriteExtractionErrorCode::DuplicateFrameId:
        return "duplicate_frame_id";
    case SpriteExtractionErrorCode::InvalidRectangle:
        return "invalid_rectangle";
    case SpriteExtractionErrorCode::RectangleOutOfBounds:
        return "rectangle_out_of_bounds";
    case SpriteExtractionErrorCode::InvalidGrid:
        return "invalid_grid";
    case SpriteExtractionErrorCode::ExpectedFrameCountMismatch:
        return "expected_frame_count_mismatch";
    case SpriteExtractionErrorCode::EmptyFrameAfterTrim:
        return "empty_frame_after_trim";
    case SpriteExtractionErrorCode::ProcessingFailure:
        return "processing_failure";
    }
    return "unknown";
}

SpriteExtractionResult ExtractSpriteFrames(
    const SpriteExtractionSheetView& sheet,
    const SpriteExtractionSpec& spec)
{
    SpriteExtractionResult validation = ValidateSheetAndSpec(sheet, spec);
    if (!validation.diagnostics.empty())
    {
        return validation;
    }

    std::vector<PlannedFrame> plans{};
    SpriteExtractionResult planResult{};
    switch (spec.mode)
    {
    case SpriteExtractionMode::ExplicitRects:
        planResult = PlanExplicitRects(sheet, spec, plans);
        break;
    case SpriteExtractionMode::UniformGrid:
        planResult = PlanGrid(sheet, spec, plans);
        break;
    case SpriteExtractionMode::AlphaComponents:
        planResult = PlanAlphaComponents(sheet, spec, plans);
        break;
    default:
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::InvalidMode,
            std::string{sheet.id},
            "Sprite extraction mode is invalid.");
    }
    if (!planResult.diagnostics.empty())
    {
        return planResult;
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
            const std::optional<SpritePixelRect> visibleBounds =
                FindVisibleBounds(sheet, spec.cleanup, plan.rect);
            if (!visibleBounds.has_value())
            {
                return Failure(
                    sheet,
                    spec,
                    SpriteExtractionErrorCode::EmptyFrameAfterTrim,
                    plan.id,
                    "trimToVisibleAlphaBounds cannot trim a frame with no visible post-cleanup pixels.");
            }
            sourceRect = *visibleBounds;
        }

        SpriteExtractedFrame frame{};
        frame.id = plan.id;
        frame.sourceRect = sourceRect;
        frame.width = sourceRect.width;
        frame.height = sourceRect.height;
        if (!CopyCleanedRect(sheet, spec.cleanup, sourceRect, frame.rgba8))
        {
            return Failure(
                sheet,
                spec,
                SpriteExtractionErrorCode::SizeOverflow,
                plan.id,
                "Extracted frame byte count cannot be represented by size_t.");
        }
        result.frames.push_back(std::move(frame));
    }

    std::vector<SpriteProcessingFrameView> processingViews{};
    processingViews.reserve(result.frames.size());
    for (const SpriteExtractedFrame& frame : result.frames)
    {
        processingViews.push_back(SpriteProcessingFrameView{
            .id = frame.id,
            .width = frame.width,
            .height = frame.height,
            .rgba8 = frame.rgba8,
        });
    }

    const SpriteProcessingResult processing =
        AnalyzeSpriteProcessing(processingViews, {});
    if (!processing.Succeeded())
    {
        std::string message{"SPP0 processing rejected extracted output."};
        if (!processing.diagnostics.empty())
        {
            message.append(" ");
            message.append(processing.diagnostics.front().message);
        }
        return Failure(
            sheet,
            spec,
            SpriteExtractionErrorCode::ProcessingFailure,
            std::string{sheet.id},
            std::move(message));
    }

    result.processingReport = *processing.report;
    return result;
}

std::string SerializeSpriteExtractionResultJson(const SpriteExtractionResult& result)
{
    std::string output{};
    output.reserve(512U + result.frames.size() * 128U);

    output.append("{\"schema_version\":");
    AppendUnsigned(output, result.schemaVersion);
    output.append(",\"sheet_id\":");
    AppendJsonString(output, result.sheetId);
    output.append(",\"mode\":");
    AppendJsonString(output, ToString(result.mode));
    output.append(",\"succeeded\":");
    AppendBool(output, result.Succeeded());

    output.append(",\"frames\":[");
    for (std::size_t index = 0U; index < result.frames.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        const SpriteExtractedFrame& frame = result.frames[index];
        output.append("{\"id\":");
        AppendJsonString(output, frame.id);
        output.append(",\"source_rect\":");
        AppendRect(output, frame.sourceRect);
        output.append(",\"width\":");
        AppendUnsigned(output, frame.width);
        output.append(",\"height\":");
        AppendUnsigned(output, frame.height);
        output.append(",\"rgba8_byte_count\":");
        AppendUnsigned(output, frame.rgba8.size());
        output.push_back('}');
    }
    output.push_back(']');

    output.append(",\"processing\":");
    if (result.processingReport.has_value())
    {
        output.append(SerializeSpriteProcessingReportJson(*result.processingReport));
    }
    else
    {
        output.append("null");
    }

    output.append(",\"diagnostics\":[");
    for (std::size_t index = 0U; index < result.diagnostics.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        const SpriteExtractionDiagnostic& diagnostic = result.diagnostics[index];
        output.append("{\"code\":");
        AppendJsonString(output, ToString(diagnostic.code));
        output.append(",\"id\":");
        AppendJsonString(output, diagnostic.id);
        output.append(",\"message\":");
        AppendJsonString(output, diagnostic.message);
        output.push_back('}');
    }
    output.append("]}");
    return output;
}
} // namespace trace2d::assets
